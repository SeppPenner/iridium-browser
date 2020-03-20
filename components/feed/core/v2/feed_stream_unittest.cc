// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_model_update_request.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

class TestSurface : public FeedStream::SurfaceInterface {
 public:
  // FeedStream::SurfaceInterface.
  void InitialStreamState(const feedui::StreamUpdate& stream_update) override {
    initial_state = stream_update;
  }
  void StreamUpdate(const feedui::StreamUpdate& stream_update) override {
    update = stream_update;
  }

  base::Optional<feedui::StreamUpdate> initial_state;
  base::Optional<feedui::StreamUpdate> update;
};

class TestFeedNetwork : public FeedNetwork {
 public:
  // FeedNetwork implementation.
  void SendQueryRequest(
      const feedwire::Request& request,
      base::OnceCallback<void(QueryRequestResult)> callback) override {}
  void SendActionRequest(
      const feedwire::ActionRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) override {}
  void CancelRequests() override {}
};

class FakeRefreshTaskScheduler : public RefreshTaskScheduler {
 public:
  // RefreshTaskScheduler implementation.
  void EnsureScheduled(base::TimeDelta period) override {
    scheduled_period = period;
  }
  void Cancel() override { canceled = true; }
  void RefreshTaskComplete() override { refresh_task_complete = true; }

  base::Optional<base::TimeDelta> scheduled_period;
  bool canceled = false;
  bool refresh_task_complete = false;
};

class TestEventObserver : public FeedStream::EventObserver {
 public:
  // FeedStreamUnittest::StreamEventObserver.
  void OnMaybeTriggerRefresh(TriggerType trigger,
                             bool clear_all_before_refresh) override {
    refresh_trigger_type = trigger;
  }
  void OnClearAll(base::TimeDelta time_since_last_clear) override {
    this->time_since_last_clear = time_since_last_clear;
  }

  // Test access.

  base::Optional<base::TimeDelta> time_since_last_clear;
  base::Optional<TriggerType> refresh_trigger_type;
};

class FeedStreamTest : public testing::Test, public FeedStream::Delegate {
 public:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    stream_ = std::make_unique<FeedStream>(
        &refresh_scheduler_, &event_observer_, this, &profile_prefs_, &network_,
        &clock_, &tick_clock_, task_environment_.GetMainThreadTaskRunner());
  }

  // FeedStream::Delegate.
  bool IsEulaAccepted() override { return true; }
  bool IsOffline() override { return false; }
 protected:
  TestEventObserver event_observer_;
  TestingPrefServiceSimple profile_prefs_;
  TestFeedNetwork network_;
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  FakeRefreshTaskScheduler refresh_scheduler_;
  std::unique_ptr<FeedStream> stream_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FeedStreamTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, SetArticlesListVisible) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(false);
  EXPECT_FALSE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(true);
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, RefreshIsScheduledOnInitialize) {
  stream_->InitializeScheduling();
  EXPECT_TRUE(refresh_scheduler_.scheduled_period);
}

TEST_F(FeedStreamTest, ScheduledRefreshTriggersRefresh) {
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_EQ(TriggerType::kFixedTimer, event_observer_.refresh_trigger_type);
  // TODO(harringtond): Once we actually perform the refresh, make sure
  // RefreshTaskComplete() is called.
  // EXPECT_TRUE(refresh_scheduler_.refresh_task_complete);
}

TEST_F(FeedStreamTest, DoNotRefreshIfArticlesListIsHidden) {
  stream_->SetArticlesListVisible(false);
  stream_->InitializeScheduling();
  stream_->ExecuteRefreshTask();

  EXPECT_TRUE(refresh_scheduler_.canceled);
  EXPECT_FALSE(event_observer_.refresh_trigger_type);
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesInitialContentLoadedAfterAttach) {
  TestSurface surface;
  stream_->AttachSurface(&surface);
  ASSERT_FALSE(surface.initial_state);
  {
    auto model = std::make_unique<StreamModel>();
    model->Update(MakeTypicalInitialModelState());
    stream_->LoadModelForTesting(std::move(model));
  }

  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedStreamTest, SurfaceReceivesUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  // Remove #1, add #2.
  stream_->ExecuteOperations({
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });
  ASSERT_TRUE(surface.update);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  const feedui::StreamUpdate& update = surface.update.value();

  ASSERT_EQ(2, update.updated_slices().size());
  // First slice is just an ID that matches the old 1st slice ID.
  EXPECT_EQ(initial_state.updated_slices(0).slice().slice_id(),
            update.updated_slices(0).slice_id());
  // Second slice is a new xsurface slice.
  EXPECT_NE("", update.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:2",
            update.updated_slices(1).slice().xsurface_slice().xsurface_frame());
}

TEST_F(FeedStreamTest, SurfaceReceivesSecondUpdatedContent) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  // Add #2.
  stream_->ExecuteOperations({
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  });

  // Clear the last update and add #3.
  surface.update = {};
  stream_->ExecuteOperations({
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  });

  // The last update should have only one new piece of content.
  // This verifies the current content set is tracked properly.
  ASSERT_TRUE(surface.update);

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedStreamTest, DetachSurface) {
  {
    auto model = std::make_unique<StreamModel>();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(std::move(model));
  }
  TestSurface surface;
  stream_->AttachSurface(&surface);
  EXPECT_TRUE(surface.initial_state);
  stream_->DetachSurface(&surface);

  // Arbitrary stream change. Surface should not see the update.
  stream_->ExecuteOperations({
      MakeOperation(MakeRemove(MakeClusterId(1))),
  });
  EXPECT_FALSE(surface.update);
}

}  // namespace
}  // namespace feed
