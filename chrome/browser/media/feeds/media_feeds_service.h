// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_

#include <memory>
#include <set>

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class GURL;

namespace safe_search_api {
enum class Classification;
class URLChecker;
}  // namespace safe_search_api

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace media_feeds {

class MediaFeedsService : public KeyedService {
 public:
  static const char kSafeSearchResultHistogramName[];

  explicit MediaFeedsService(Profile* profile);
  ~MediaFeedsService() override;
  MediaFeedsService(const MediaFeedsService& t) = delete;
  MediaFeedsService& operator=(const MediaFeedsService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaFeedsService* Get(Profile* profile);

  // Register profile prefs in the pref registry.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Checks the list of pending items against the Safe Search API and stores
  // the result.
  void CheckItemsAgainstSafeSearch(
      media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList list);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

  // Stores a callback to be called once we have completed all inflight checks.
  void SetSafeSearchCompletionCallbackForTest(base::OnceClosure callback);

  // Fetches a media feed with the given ID and URL and then store it in the
  // feeds table in media history. Runs the given callback after storing. The
  // fetch will be skipped if another fetch is currently ongoing.
  void FetchMediaFeed(int64_t feed_id,
                      const GURL& url,
                      base::OnceClosure callback);

 private:
  friend class MediaFeedsServiceTest;

  bool AddInflightSafeSearchCheck(const int64_t id, const std::set<GURL>& urls);

  void CheckForSafeSearch(const int64_t id, const GURL& url);

  void OnCheckURLDone(const int64_t id,
                      const GURL& original_url,
                      const GURL& url,
                      safe_search_api::Classification classification,
                      bool uncertain);

  void MaybeCallCompletionCallback();

  bool IsSafeSearchCheckingEnabled() const;

  void OnFetchResponse(int64_t feed_id,
                       base::OnceClosure callback,
                       const schema_org::improved::mojom::EntityPtr& response,
                       MediaFeedsFetcher::Status status);

  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  // Used to fetch media feeds. Null if no fetch is ongoing.
  std::map<int64_t, std::unique_ptr<MediaFeedsFetcher>> fetchers_;

  struct InflightSafeSearchCheck {
    explicit InflightSafeSearchCheck(const std::set<GURL>& urls);
    ~InflightSafeSearchCheck();
    InflightSafeSearchCheck(const InflightSafeSearchCheck&) = delete;
    InflightSafeSearchCheck& operator=(const InflightSafeSearchCheck&) = delete;

    std::set<GURL> pending;

    bool is_safe = false;
    bool is_unsafe = false;
    bool is_uncertain = false;
  };

  std::map<int64_t, std::unique_ptr<InflightSafeSearchCheck>>
      inflight_safe_search_checks_;

  base::Optional<base::OnceClosure> safe_search_completion_callback_;

  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;
  Profile* const profile_;
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_SERVICE_H_
