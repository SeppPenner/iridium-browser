// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_service.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/feeds/media_feeds_service_factory.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "media/base/media_switches.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_feeds {

namespace {

constexpr size_t kCacheSize = 2;

const char kFirstItemActionURL[] = "https://www.example.com/action";
const char kFirstItemPlayNextActionURL[] = "https://www.example.com/next";

}  // namespace

class MediaFeedsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFeedsServiceTest() = default;

  void SetUp() override {
    features_.InitAndEnableFeature(media::kMediaFeeds);

    ChromeRenderViewHostTestHarness::SetUp();

    stub_url_checker_ = std::make_unique<safe_search_api::StubURLChecker>();

    GetMediaFeedsService()->SetSafeSearchURLCheckerForTest(
        stub_url_checker_->BuildURLChecker(kCacheSize));
  }

  void WaitForDB() {
    base::RunLoop run_loop;
    GetMediaHistoryService()->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SimulateOnCheckURLDone(const int64_t id,
                              const GURL& url,
                              safe_search_api::Classification classification,
                              bool uncertain) {
    GetMediaFeedsService()->OnCheckURLDone(id, url, url, classification,
                                           uncertain);
  }

  bool AddInflightSafeSearchCheck(const int64_t id,
                                  const std::set<GURL>& urls) {
    return GetMediaFeedsService()->AddInflightSafeSearchCheck(id, urls);
  }

  media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList
  GetPendingSafeSearchCheckMediaFeedItemsSync() {
    base::RunLoop run_loop;
    media_history::MediaHistoryKeyedService::PendingSafeSearchCheckList out;

    GetMediaHistoryService()->GetPendingSafeSearchCheckMediaFeedItems(
        base::BindLambdaForTesting([&](media_history::MediaHistoryKeyedService::
                                           PendingSafeSearchCheckList rows) {
          out = std::move(rows);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  std::vector<media_feeds::mojom::MediaFeedItemPtr> GetItemsForMediaFeedSync(
      const int64_t feed_id) {
    base::RunLoop run_loop;
    std::vector<media_feeds::mojom::MediaFeedItemPtr> out;

    GetMediaHistoryService()->GetItemsForMediaFeedForDebug(
        feed_id,
        base::BindLambdaForTesting(
            [&](std::vector<media_feeds::mojom::MediaFeedItemPtr> rows) {
              out = std::move(rows);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  void SetSafeSearchEnabled(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kMediaFeedsSafeSearchEnabled,
                                      enabled);
  }

  safe_search_api::StubURLChecker* safe_search_checker() {
    return stub_url_checker_.get();
  }

  MediaFeedsService* GetMediaFeedsService() {
    return MediaFeedsServiceFactory::GetInstance()->GetForProfile(profile());
  }

  media_history::MediaHistoryKeyedService* GetMediaHistoryService() {
    return media_history::MediaHistoryKeyedService::Get(profile());
  }

  static media_feeds::mojom::MediaFeedItemPtr GetSingleExpectedItem() {
    auto item = media_feeds::mojom::MediaFeedItem::New();
    item->name = base::ASCIIToUTF16("The Movie");
    item->type = media_feeds::mojom::MediaFeedItemType::kMovie;
    item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMinutes(10));
    item->action_status =
        media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
    item->action = media_feeds::mojom::Action::New();
    item->action->url = GURL(kFirstItemActionURL);
    item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
    item->play_next_candidate->action = media_feeds::mojom::Action::New();
    item->play_next_candidate->action->url = GURL(kFirstItemPlayNextActionURL);

    return item;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems() {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
    items.push_back(GetSingleExpectedItem());

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action2");
      item->author = media_feeds::mojom::Author::New();
      item->author->name = "Media Site";
      items.push_back(std::move(item));
    }

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kTVSeries;
      item->name = base::ASCIIToUTF16("The Live TV Series");
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kPotential;
      item->action = media_feeds::mojom::Action::New();
      item->action->url = GURL("https://www.example.com/action3");
      item->live = media_feeds::mojom::LiveDetails::New();
      items.push_back(std::move(item));
    }

    return items;
  }

 private:
  base::test::ScopedFeatureList features_;

  std::unique_ptr<safe_search_api::StubURLChecker> stub_url_checker_;
};

TEST_F(MediaFeedsServiceTest, GetForProfile) {
  EXPECT_NE(nullptr, MediaFeedsServiceFactory::GetForProfile(profile()));

  Profile* otr_profile = profile()->GetOffTheRecordProfile();
  EXPECT_EQ(nullptr, MediaFeedsServiceFactory::GetForProfile(otr_profile));
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllSafe) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kSafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kSafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_AllUnsafe) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ true);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Request) {
  base::HistogramTester histogram_tester;

  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpFailedResponse();

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 3);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Failed_Pref) {
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should still be 3.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
  }

  // Check the items were updated.
  auto items = GetItemsForMediaFeedSync(1);
  EXPECT_EQ(3u, items.size());
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[0]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[1]->safe_search_result);
  EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
            items[2]->safe_search_result);

  histogram_tester.ExpectTotalCount(
      MediaFeedsService::kSafeSearchResultHistogramName, 0);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Inflight) {
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  base::RunLoop run_loop;
  GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
      run_loop.QuitClosure());

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  {
    // This checks we ignore the duplicate items for inflight checks.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(3u, pending_items.size());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items));
  }

  // Wait for the service and DB to finish.
  run_loop.Run();
  WaitForDB();

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_CheckTwice_Committed) {
  SetSafeSearchEnabled(true);
  safe_search_checker()->SetUpValidResponse(/* is_porn= */ false);

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, GetExpectedItems(), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  auto pending_items_a = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_a.size());

  auto pending_items_b = GetPendingSafeSearchCheckMediaFeedItemsSync();
  EXPECT_EQ(3u, pending_items_b.size());

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_a));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    base::RunLoop run_loop;
    GetMediaFeedsService()->SetSafeSearchCompletionCallbackForTest(
        run_loop.QuitClosure());
    GetMediaFeedsService()->CheckItemsAgainstSafeSearch(
        std::move(pending_items_b));

    // Wait for the service and DB to finish.
    run_loop.Run();
    WaitForDB();
  }

  {
    // The pending items should be empty.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_TRUE(pending_items.empty());
  }
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUnsafe) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, std::move(items), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_SafeUncertain) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, std::move(items), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnknown,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnknown, 1);
}

TEST_F(MediaFeedsServiceTest, SafeSearch_Mixed_UnsafeUncertain) {
  SetSafeSearchEnabled(true);
  base::HistogramTester histogram_tester;

  // Store a Media Feed.
  GetMediaHistoryService()->DiscoverMediaFeed(
      GURL("https://www.google.com/feed"));
  WaitForDB();

  // Store some media feed items.
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  items.push_back(GetSingleExpectedItem());
  GetMediaHistoryService()->StoreMediaFeedFetchResult(
      1, std::move(items), media_feeds::mojom::FetchResult::kSuccess, false,
      std::vector<media_session::MediaImage>(), "test", base::DoNothing());
  WaitForDB();

  {
    // Get the pending items and check them against Safe Search.
    auto pending_items = GetPendingSafeSearchCheckMediaFeedItemsSync();
    EXPECT_EQ(1u, pending_items.size());
    EXPECT_TRUE(AddInflightSafeSearchCheck(pending_items[0]->id,
                                           pending_items[0]->urls));
  }

  SimulateOnCheckURLDone(1, GURL(kFirstItemActionURL),
                         safe_search_api::Classification::UNSAFE,
                         /*uncertain=*/false);
  SimulateOnCheckURLDone(1, GURL(kFirstItemPlayNextActionURL),
                         safe_search_api::Classification::SAFE,
                         /*uncertain=*/true);

  WaitForDB();

  {
    // Check the result of the item we stored.
    auto items = GetItemsForMediaFeedSync(1);
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(media_feeds::mojom::SafeSearchResult::kUnsafe,
              items[0]->safe_search_result);
  }

  histogram_tester.ExpectUniqueSample(
      MediaFeedsService::kSafeSearchResultHistogramName,
      media_feeds::mojom::SafeSearchResult::kUnsafe, 1);
}

}  // namespace media_feeds
