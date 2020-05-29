// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using SearchResultPtr = chromeos::settings::mojom::SearchResultPtr;

constexpr char kOsSettingsResultPrefix[] = "os-settings://";
constexpr float kScoreEps = 1e-5;
constexpr size_t kNumRequestedResults = 5u;
constexpr size_t kMaxShownResults = 2u;

// Various error states of the OsSettingsProvider. kOk is currently not emitted,
// but may be used in future. These values persist to logs. Entries should not
// be renumbered and numeric values should never be reused.
enum class Error {
  kOk = 0,
  kAppServiceUnavailable = 1,
  kNoSettingsIcon = 2,
  kSearchHandlerUnavailable = 3,
  kMaxValue = kSearchHandlerUnavailable
};

void LogError(Error error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.Error", error);
}

std::vector<SearchResultPtr> DeduplicateResults(
    const std::vector<SearchResultPtr>& results) {
  base::flat_map<std::string, SearchResultPtr> url_to_result;
  for (const SearchResultPtr& result : results) {
    const std::string url = result->url_path_with_parameters;
    const auto it = url_to_result.find(url);

    // Update a result in three cases:
    // 1. This is the first result for its URL.
    // 2. This has higher score than the existing result for its URL.
    // 3. The scores are tied, but this result has more general search result
    // type. Result types are ordered general-to-specific.
    if (it == url_to_result.end() ||
        result->relevance_score > it->second->relevance_score ||
        (it->second->relevance_score == result->relevance_score &&
         result->type < it->second->type)) {
      url_to_result[url] = result->Clone();
    }
  }

  std::vector<SearchResultPtr> clean_results;
  for (const auto& pair : url_to_result) {
    clean_results.push_back(pair.second.Clone());
  }
  return clean_results;
}

}  // namespace

OsSettingsResult::OsSettingsResult(
    Profile* profile,
    const chromeos::settings::mojom::SearchResultPtr& result,
    const float relevance_score,
    const gfx::ImageSkia& icon)
    : profile_(profile), url_path_(result->url_path_with_parameters) {
  // TODO(crbug.com/1068851): Results need useful details text. Once this is
  // available in the SearchResultPtr, set the metadata here.
  set_id(kOsSettingsResultPrefix + url_path_);
  set_relevance(relevance_score);
  SetTitle(result->result_text);
  SetResultType(ResultType::kOsSettings);
  SetDisplayType(DisplayType::kList);
  SetIcon(icon);
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

ash::SearchResultType OsSettingsResult::GetSearchResultType() const {
  return ash::OS_SETTINGS;
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : profile_(profile),
      search_handler_(
          chromeos::settings::OsSettingsManagerFactory::GetForProfile(profile)
              ->search_handler()) {
  DCHECK(profile_);

  // |search_handler_| can be nullptr in the case that the new OS settings
  // search chrome flag is disabled. If it is, we should effectively disable the
  // search provider.
  if (!search_handler_) {
    LogError(Error::kSearchHandlerUnavailable);
    return;
  }

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (app_service_proxy_) {
    Observe(&app_service_proxy_->AppRegistryCache());
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  } else {
    LogError(Error::kAppServiceUnavailable);
  }
}

OsSettingsProvider::~OsSettingsProvider() = default;

void OsSettingsProvider::Start(const base::string16& query) {
  if (!search_handler_)
    return;

  ClearResultsSilently();

  // This provider does not handle zero-state, and shouldn't return any results
  // for a single-character query because they aren't meaningful enough.
  if (query.size() <= 1)
    return;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(query, kNumRequestedResults,
                          chromeos::settings::mojom::ParentResultBehavior::
                              kDoNotIncludeParentResults,
                          base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                                         weak_factory_.GetWeakPtr()));
}

void OsSettingsProvider::OnSearchReturned(
    std::vector<chromeos::settings::mojom::SearchResultPtr> results) {
  DCHECK_LE(results.size(), kNumRequestedResults);

  std::vector<SearchResultPtr> clean_results = DeduplicateResults(results);
  std::sort(clean_results.begin(), clean_results.end(),
            [](const SearchResultPtr& a, const SearchResultPtr& b) {
              return a->relevance_score > b->relevance_score;
            });

  // TODO(crbug.com/1068851): We are currently not ranking settings results.
  // Instead, we are gluing at most two to the top of the search box. Consider
  // ranking these with other results in the next version of the feature.
  SearchProvider::Results search_results;
  const size_t num_results = std::min(clean_results.size(), kMaxShownResults);
  for (size_t i = 0; i < num_results; ++i) {
    const auto& result = clean_results[i];
    const float score = 1.0f - i * kScoreEps;
    search_results.emplace_back(
        std::make_unique<OsSettingsResult>(profile_, result, score, icon_));
  }

  if (icon_.isNull())
    LogError(Error::kNoSettingsIcon);
  SwapResults(&search_results);
}

void OsSettingsProvider::OnAppUpdate(const apps::AppUpdate& update) {
  // Watch the app service for updates. On an update that marks the OS settings
  // app as ready, retrieve the icon for the app to use for search results.
  if (app_service_proxy_ &&
      update.AppId() == chromeos::default_web_apps::kOsSettingsAppId &&
      update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  }
}

void OsSettingsProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void OsSettingsProvider::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression ==
      apps::mojom::IconCompression::kUncompressed) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
