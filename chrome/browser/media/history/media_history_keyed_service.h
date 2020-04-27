// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/media_player_watch_time.h"
#include "services/media_session/public/cpp/media_metadata.h"

class Profile;

namespace media_session {
struct MediaImage;
struct MediaMetadata;
struct MediaPosition;
}  // namespace media_session

namespace history {
class HistoryService;
}  // namespace history

namespace media_history {

class MediaHistoryKeyedService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  explicit MediaHistoryKeyedService(Profile* profile);
  ~MediaHistoryKeyedService() override;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static MediaHistoryKeyedService* Get(Profile* profile);

  // Overridden from KeyedService:
  void Shutdown() override;

  // Overridden from history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Saves a playback from a single player in the media history store.
  void SavePlayback(const content::MediaPlayerWatchTime& watch_time);

  void GetMediaHistoryStats(
      base::OnceCallback<void(mojom::MediaHistoryStatsPtr)> callback);

  // Returns all the rows in the origin table. This should only be used for
  // debugging because it is very slow.
  void GetOriginRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryOriginRowPtr>)>
          callback);

  // Returns all the rows in the playback table. This is only used for
  // debugging because it loads all rows in the table.
  void GetMediaHistoryPlaybackRowsForDebug(
      base::OnceCallback<void(std::vector<mojom::MediaHistoryPlaybackRowPtr>)>
          callback);

  // Gets the playback sessions from the media history store. The results will
  // be ordered by most recent first and be limited to the first |num_sessions|.
  // For each session it calls |filter| and if that returns |true| then that
  // session will be included in the results.
  using GetPlaybackSessionsFilter =
      base::RepeatingCallback<bool(const base::TimeDelta& duration,
                                   const base::TimeDelta& position)>;
  void GetPlaybackSessions(
      base::Optional<unsigned int> num_sessions,
      base::Optional<GetPlaybackSessionsFilter> filter,
      base::OnceCallback<void(
          std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>)> callback);

  // Saves a playback session in the media history store.
  void SavePlaybackSession(
      const GURL& url,
      const media_session::MediaMetadata& metadata,
      const base::Optional<media_session::MediaPosition>& position,
      const std::vector<media_session::MediaImage>& artwork);

  // Saves a newly discovered media feed in the media history store.
  void DiscoverMediaFeed(const GURL& url);

  // Gets the media items in |feed_id|.
  void GetItemsForMediaFeedForDebug(
      const int64_t feed_id,
      base::OnceCallback<
          void(std::vector<media_feeds::mojom::MediaFeedItemPtr>)> callback);

  // Replaces the media items in |feed_id|. This will delete any old feed items
  // and store the new ones in |items|. This will also update the |result|,
  // |logos| and |display_name| for the feed. If the feed was fetched from the
  // browser cache then |was_fetched_from_cache| should be true.
  void StoreMediaFeedFetchResult(
      const int64_t feed_id,
      std::vector<media_feeds::mojom::MediaFeedItemPtr> items,
      const media_feeds::mojom::FetchResult result,
      const bool was_fetched_from_cache,
      const std::vector<media_session::MediaImage>& logos,
      const std::string& display_name,
      const std::vector<url::Origin>& associated_origins,
      base::OnceClosure callback);

  void GetURLsInTableForTest(const std::string& table,
                             base::OnceCallback<void(std::set<GURL>)> callback);

  // Represents a Media Feed Item that needs to be checked against Safe Search.
  // Contains the ID of the feed item and a set of URLs that should be checked.
  struct PendingSafeSearchCheck {
    explicit PendingSafeSearchCheck(int64_t id);
    ~PendingSafeSearchCheck();
    PendingSafeSearchCheck(const PendingSafeSearchCheck&) = delete;
    PendingSafeSearchCheck& operator=(const PendingSafeSearchCheck&) = delete;

    int64_t const id;
    std::set<GURL> urls;
  };
  using PendingSafeSearchCheckList =
      std::vector<std::unique_ptr<PendingSafeSearchCheck>>;
  void GetPendingSafeSearchCheckMediaFeedItems(
      base::OnceCallback<void(PendingSafeSearchCheckList)> callback);

  // Store the Safe Search check results for multiple Media Feed Items. The
  // map key is the ID of the feed item.
  void StoreMediaFeedItemSafeSearchResults(
      std::map<int64_t, media_feeds::mojom::SafeSearchResult> results);

  // Posts an empty task to the database thread. The callback will be called
  // on the calling thread when the empty task is completed. This can be used
  // for waiting for database operations in tests.
  void PostTaskToDBForTest(base::OnceClosure callback);

  // Returns Media Feeds.
  struct GetMediaFeedsRequest {
    enum class Type {
      // Return all the fields for debugging.
      kDebugAll,

      // Returns the top feeds to be fetched. These will be sorted by the
      // by audio+video watchtime descending and we will also populate the
      // |origin_audio_video_watchtime_percentile| field in |MediaFeedPtr|.
      kTopFeedsForFetch,

      // Returns the top feeds to be displayed. These will be sorted by the
      // by audio+video watchtime descending and we will also populate the
      // |origin_audio_video_watchtime_percentile| field in |MediaFeedPtr|.
      kTopFeedsForDisplay
    };

    static GetMediaFeedsRequest CreateTopFeedsForFetch(
        unsigned limit,
        base::TimeDelta audio_video_watchtime_min);

    static GetMediaFeedsRequest CreateTopFeedsForDisplay(
        unsigned limit,
        base::TimeDelta audio_video_watchtime_min,
        int fetched_items_min,
        bool fetched_items_min_should_be_safe);

    GetMediaFeedsRequest();
    GetMediaFeedsRequest(const GetMediaFeedsRequest& t);

    Type type = Type::kDebugAll;

    // The maximum number of feeds to return. Only valid for |kTopFeedsForFetch|
    // and |kTopFeedsForDisplay|.
    base::Optional<unsigned> limit;

    // The minimum audio+video watchtime required on the origin to return the
    // feed. Only valid for |kTopFeedsForFetch| and |kTopFeedsForDisplay|.
    base::Optional<base::TimeDelta> audio_video_watchtime_min;

    // The minimum number of fetched items that are required and whether they
    // should have passed safe search. Only valid for |kTopFeedsForDisplay|.
    base::Optional<int> fetched_items_min;
    bool fetched_items_min_should_be_safe = false;
  };
  void GetMediaFeeds(
      const GetMediaFeedsRequest& request,
      base::OnceCallback<void(std::vector<media_feeds::mojom::MediaFeedPtr>)>
          callback);

  // Updates the display time for the Media Feed with |feed_id| to now.
  void UpdateMediaFeedDisplayTime(const int64_t feed_id);

  // Increment the media feed items shown counter by one.
  void IncrementMediaFeedItemsShownCount(const std::set<int64_t> feed_item_ids);

  // Marks a media feed item as clicked. This is when the user has opened the
  // item in the UI.
  void MarkMediaFeedItemAsClicked(const int64_t& feed_item_id);

  // Resets a Media Feed by deleting any items and resetting it to defaults.
  void ResetMediaFeed(const int64_t feed_id,
                      media_feeds::mojom::ResetReason reason);

 private:
  class StoreHolder;

  std::unique_ptr<StoreHolder> store_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryKeyedService);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_H_
