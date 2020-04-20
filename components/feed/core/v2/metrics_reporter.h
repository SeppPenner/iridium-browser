// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
#define COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_

#include "base/time/time.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"

namespace base {
class TickClock;
}  // namespace base

namespace feed {

namespace internal {
// This enum is used for a UMA histogram. Keep in sync with FeedEngagementType
// in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kFeedScrolled = 3,
  kMaxValue = FeedEngagementType::kFeedScrolled,
};
}  // namespace internal

// Reports UMA metrics for feed.
// Note this is inherited only for testing.
class MetricsReporter {
 public:
  explicit MetricsReporter(const base::TickClock* clock);
  virtual ~MetricsReporter();
  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  // User interactions. See |FeedStreamApi| for definitions.

  virtual void ContentSliceViewed(int index_in_stream);
  void OpenAction();
  void OpenInNewTabAction();
  void SendFeedbackAction();
  void LearnMoreAction();
  void DownloadAction();
  void NavigationStarted();
  void NavigationDone();
  void RemoveAction();
  void NotInterestedInAction();
  void ManageInterestsAction();
  void ContextMenuOpened();
  // Indicates the user scrolled the feed by |distance_dp| and then stopped
  // scrolling.
  void StreamScrolled(int distance_dp);

  // Called when the Feed surface is opened.
  void SurfaceOpened();

  // Network metrics.

  static void NetworkRequestComplete(NetworkRequestType type,
                                     int http_status_code);

  // Stream events.

  virtual void OnLoadStream(LoadStreamStatus load_from_store_status,
                            LoadStreamStatus final_status);
  virtual void OnBackgroundRefresh(LoadStreamStatus final_status);
  virtual void OnLoadMore(LoadStreamStatus final_status);
  virtual void OnClearAll(base::TimeDelta time_since_last_clear);

 private:
  void RecordEngagement(int scroll_distance_dp, bool interacted);
  void RecordInteraction();

  const base::TickClock* clock_;

  base::TimeTicks visit_start_time_;
  bool engaged_simple_reported_ = false;
  bool engaged_reported_ = false;
  bool scrolled_reported_ = false;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
