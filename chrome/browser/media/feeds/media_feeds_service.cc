// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "url/gurl.h"

namespace media_feeds {

namespace {

GURL Normalize(const GURL& url) {
  GURL normalized_url = url;
  GURL::Replacements replacements;
  // Strip username, password, query, and ref.
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace

const char MediaFeedsService::kSafeSearchResultHistogramName[] =
    "Media.Feeds.SafeSearch.Result";

MediaFeedsService::MediaFeedsService(Profile* profile) : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
}

// static
MediaFeedsService* MediaFeedsService::Get(Profile* profile) {
  return MediaFeedsServiceFactory::GetForProfile(profile);
}

MediaFeedsService::~MediaFeedsService() = default;

bool MediaFeedsService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kMediaFeeds);
}

// static
void MediaFeedsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kMediaFeedsSafeSearchEnabled, false);
}

void MediaFeedsService::CheckItemsAgainstSafeSearch(
    media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList list) {
  if (!IsSafeSearchCheckingEnabled()) {
    MaybeCallCompletionCallback();
    return;
  }

  for (auto& check : list) {
    if (!AddInflightSafeSearchCheck(check->id, check->urls))
      continue;

    for (auto& url : check->urls)
      CheckForSafeSearch(check->id, url);
  }
}

void MediaFeedsService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}

void MediaFeedsService::SetSafeSearchCompletionCallbackForTest(
    base::OnceClosure callback) {
  safe_search_completion_callback_ = std::move(callback);
}

bool MediaFeedsService::AddInflightSafeSearchCheck(const int64_t id,
                                                   const std::set<GURL>& urls) {
  if (base::Contains(inflight_safe_search_checks_, id))
    return false;

  inflight_safe_search_checks_.emplace(
      id, std::make_unique<InflightSafeSearchCheck>(urls));

  return true;
}

void MediaFeedsService::CheckForSafeSearch(const int64_t id, const GURL& url) {
  DCHECK(IsSafeSearchCheckingEnabled());

  if (!safe_search_url_checker_) {
    // TODO(https://crbug.com/1066643): Add a UI toggle to turn this feature on.
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("media_feeds_checker", R"(
          semantics {
            sender: "Media Feeds Safe Search Checker"
            description:
              "Media Feeds are feeds of personalized media recommendations "
              "that are fetched from media websites and displayed to the user. "
              "These feeds are discovered automatically on websites that embed "
              "them. Chrome will then periodically fetch the feeds in the "
              "background. This checker will check the media recommendations "
              "against the Google SafeSearch API to ensure the recommendations "
              "are safe and do not contain any inappropriate content."
            trigger:
              "Having a discovered feed that has not been fetched recently. "
              "Feeds are discovered when the browser visits any webpage with a "
              "feed link element in the header. Chrome will only fetch feeds "
              "from a website that meets certain media heuristics. This is to "
              "limit Media Feeds to only sites the user watches videos on."
            data: "URL to be checked."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is off by default and cannot be controlled in "
              "settings."
            chrome_policy {
              SavingBrowserHistoryDisabled {
                policy_options {mode: MANDATORY}
                SavingBrowserHistoryDisabled: false
              }
            }
          })");

    safe_search_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            content::BrowserContext::GetDefaultStoragePartition(profile_)
                ->GetURLLoaderFactoryForBrowserProcess(),
            traffic_annotation));
  }

  safe_search_url_checker_->CheckURL(
      Normalize(url), base::BindOnce(&MediaFeedsService::OnCheckURLDone,
                                     base::Unretained(this), id, url));
}

void MediaFeedsService::OnCheckURLDone(
    const int64_t id,
    const GURL& original_url,
    const GURL& url,
    safe_search_api::Classification classification,
    bool uncertain) {
  DCHECK(IsSafeSearchCheckingEnabled());

  // Get the inflight safe search check data.
  auto it = inflight_safe_search_checks_.find(id);
  if (it == inflight_safe_search_checks_.end())
    return;

  // Remove the url we just checked from the pending list.
  auto* check = it->second.get();
  check->pending.erase(original_url);

  if (uncertain) {
    check->is_uncertain = true;
  } else if (classification == safe_search_api::Classification::SAFE) {
    check->is_safe = true;
  } else if (classification == safe_search_api::Classification::UNSAFE) {
    check->is_unsafe = true;
  }

  // If we have no more URLs to check or the result is unsafe then we should
  // store now.
  if (!(check->pending.empty() || check->is_unsafe))
    return;

  auto result = media_feeds::mojom::SafeSearchResult::kUnknown;
  if (check->is_unsafe) {
    result = media_feeds::mojom::SafeSearchResult::kUnsafe;
  } else if (check->is_safe && !check->is_uncertain) {
    result = media_feeds::mojom::SafeSearchResult::kSafe;
  }

  std::map<int64_t, media_feeds::mojom::SafeSearchResult> results;
  results.emplace(id, result);
  inflight_safe_search_checks_.erase(id);

  auto* service =
      media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile_);
  DCHECK(service);
  service->StoreMediaFeedItemSafeSearchResults(results);

  base::UmaHistogramEnumeration(kSafeSearchResultHistogramName, result);

  MaybeCallCompletionCallback();
}

void MediaFeedsService::MaybeCallCompletionCallback() {
  if (inflight_safe_search_checks_.empty() &&
      safe_search_completion_callback_.has_value()) {
    std::move(*safe_search_completion_callback_).Run();
    safe_search_completion_callback_.reset();
  }
}

bool MediaFeedsService::IsSafeSearchCheckingEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kMediaFeedsSafeSearchEnabled);
}

MediaFeedsService::InflightSafeSearchCheck::InflightSafeSearchCheck(
    const std::set<GURL>& urls)
    : pending(urls) {}

MediaFeedsService::InflightSafeSearchCheck::~InflightSafeSearchCheck() =
    default;

}  // namespace media_feeds
