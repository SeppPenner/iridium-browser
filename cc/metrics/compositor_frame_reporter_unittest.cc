// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/input/scroll_input_type.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

MATCHER(IsWhitelisted,
        base::StrCat({negation ? "isn't" : "is", " whitelisted"})) {
  return arg.IsWhitelisted();
}

class CompositorFrameReporterTest : public testing::Test {
 public:
  CompositorFrameReporterTest()
      : pipeline_reporter_(std::make_unique<CompositorFrameReporter>(
            CompositorFrameReporter::ActiveTrackers(),
            viz::BeginFrameId(),
            base::TimeTicks() + base::TimeDelta::FromMilliseconds(16),
            nullptr,
            /*should_report_metrics=*/true)) {
    pipeline_reporter_->set_tick_clock(&test_tick_clock_);
    AdvanceNowByMs(1);
  }

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::TimeDelta::FromMicroseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }

  base::TimeTicks Now() { return test_tick_clock_.NowTicks(); }

  viz::FrameTimingDetails BuildFrameTimingDetails() {
    viz::FrameTimingDetails frame_timing_details;
    frame_timing_details.received_compositor_frame_timestamp =
        AdvanceNowByMs(1);
    frame_timing_details.draw_start_timestamp = AdvanceNowByMs(1);
    frame_timing_details.swap_timings.swap_start = AdvanceNowByMs(1);
    frame_timing_details.swap_timings.swap_end = AdvanceNowByMs(1);
    frame_timing_details.presentation_feedback.timestamp = AdvanceNowByMs(1);
    return frame_timing_details;
  }

  // This should be defined before |pipeline_reporter_| so it is created before
  // and destroyed after that.
  base::SimpleTestTickClock test_tick_clock_;

  std::unique_ptr<CompositorFrameReporter> pipeline_reporter_;
};

TEST_F(CompositorFrameReporterTest, MainFrameAbortedReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  EXPECT_EQ(3, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(4, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.BeginImplFrameToSendBeginMainFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SubmitCompositorFrameToPresentationCompositorFrame",
      1);
}

TEST_F(CompositorFrameReporterTest, ReplacedByNewReporterReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation, Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kReplacedByNewReporter,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.EndCommitToActivation",
                                    0);
}

TEST_F(CompositorFrameReporterTest, SubmittedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame, Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount("CompositorLatency.Activation", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Activation",
                                    0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.EndActivateToSubmitCompositorFrame", 0);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 0);

  histogram_tester.ExpectBucketCount("CompositorLatency.Activation", 3, 1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.EndActivateToSubmitCompositorFrame", 2, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.TotalLatency", 5, 1);
}

TEST_F(CompositorFrameReporterTest, SubmittedDroppedFrameReportingTest) {
  base::HistogramTester histogram_tester;

  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit, Now());
  EXPECT_EQ(0, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 Now());
  EXPECT_EQ(1, pipeline_reporter_->StageHistorySizeForTesting());

  AdvanceNowByMs(2);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());
  EXPECT_EQ(2, pipeline_reporter_->StageHistorySizeForTesting());

  pipeline_reporter_ = nullptr;
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 1);
  histogram_tester.ExpectTotalCount("CompositorLatency.DroppedFrame.Commit", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "CompositorLatency.SendBeginMainFrameToCommit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.Commit", 0);
  histogram_tester.ExpectTotalCount("CompositorLatency.TotalLatency", 0);

  histogram_tester.ExpectBucketCount(
      "CompositorLatency.DroppedFrame.SendBeginMainFrameToCommit", 3, 1);
  histogram_tester.ExpectBucketCount("CompositorLatency.DroppedFrame.Commit", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "CompositorLatency.DroppedFrame.TotalLatency", 5, 1);
}

// Tests that when a frame is presented to the user, event latency metrics are
// reported properly.
TEST_F(CompositorFrameReporterTest, EventLatencyForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_TOUCH_PRESSED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  const base::TimeTicks presentation_time = Now();
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      presentation_time);

  pipeline_reporter_ = nullptr;

  const int latency_ms = (presentation_time - event_time).InMicroseconds();
  histogram_tester.ExpectTotalCount("EventLatency.TouchPressed.TotalLatency",
                                    1);
  histogram_tester.ExpectTotalCount("EventLatency.TouchMoved.TotalLatency", 2);
  histogram_tester.ExpectBucketCount("EventLatency.TouchPressed.TotalLatency",
                                     latency_ms, 1);
  histogram_tester.ExpectBucketCount("EventLatency.TouchMoved.TotalLatency",
                                     latency_ms, 2);
}

// Tests that when a frame is presented to the user, event latency breakdown
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyBreakdownsForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_TOUCH_PRESSED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  auto begin_impl_time = AdvanceNowByMs(2);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      begin_impl_time);

  auto begin_main_time = AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kSendBeginMainFrameToCommit,
      begin_main_time);

  auto begin_commit_time = AdvanceNowByMs(4);
  pipeline_reporter_->StartStage(CompositorFrameReporter::StageType::kCommit,
                                 begin_commit_time);

  auto end_commit_time = AdvanceNowByMs(5);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndCommitToActivation,
      end_commit_time);

  auto begin_activation_time = AdvanceNowByMs(6);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kActivation, begin_activation_time);

  auto end_activation_time = AdvanceNowByMs(7);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      end_activation_time);

  auto submit_time = AdvanceNowByMs(8);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      submit_time);
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  auto presentation_time = AdvanceNowByMs(9);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      presentation_time);

  pipeline_reporter_ = nullptr;

  struct {
    const char* name;
    const base::TimeDelta latency;
    const int count;
  } expected_counts[] = {
      {"EventLatency.TouchPressed.BrowserToRendererCompositor",
       begin_impl_time - event_time, 1},
      {"EventLatency.TouchPressed.BeginImplFrameToSendBeginMainFrame",
       begin_main_time - begin_impl_time, 1},
      {"EventLatency.TouchPressed.SendBeginMainFrameToCommit",
       begin_commit_time - begin_main_time, 1},
      {"EventLatency.TouchPressed.Commit", end_commit_time - begin_commit_time,
       1},
      {"EventLatency.TouchPressed.EndCommitToActivation",
       begin_activation_time - end_commit_time, 1},
      {"EventLatency.TouchPressed.Activation",
       end_activation_time - begin_activation_time, 1},
      {"EventLatency.TouchPressed.EndActivateToSubmitCompositorFrame",
       submit_time - end_activation_time, 1},
      {"EventLatency.TouchPressed."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       presentation_time - submit_time, 1},
      {"EventLatency.TouchPressed.TotalLatency", presentation_time - event_time,
       1},
      {"EventLatency.TouchMoved.BrowserToRendererCompositor",
       begin_impl_time - event_time, 2},
      {"EventLatency.TouchMoved.BeginImplFrameToSendBeginMainFrame",
       begin_main_time - begin_impl_time, 2},
      {"EventLatency.TouchMoved.SendBeginMainFrameToCommit",
       begin_commit_time - begin_main_time, 2},
      {"EventLatency.TouchMoved.Commit", end_commit_time - begin_commit_time,
       2},
      {"EventLatency.TouchMoved.EndCommitToActivation",
       begin_activation_time - end_commit_time, 2},
      {"EventLatency.TouchMoved.Activation",
       end_activation_time - begin_activation_time, 2},
      {"EventLatency.TouchMoved.EndActivateToSubmitCompositorFrame",
       submit_time - end_activation_time, 2},
      {"EventLatency.TouchMoved."
       "SubmitCompositorFrameToPresentationCompositorFrame",
       presentation_time - submit_time, 2},
      {"EventLatency.TouchMoved.TotalLatency", presentation_time - event_time,
       2},
  };

  for (const auto& expected_count : expected_counts) {
    histogram_tester.ExpectTotalCount(expected_count.name,
                                      expected_count.count);
    histogram_tester.ExpectBucketCount(expected_count.name,
                                       expected_count.latency.InMicroseconds(),
                                       expected_count.count);
  }
}

// Tests that when a frame is presented to the user, scroll event latency
// metrics are reported properly.
TEST_F(CompositorFrameReporterTest,
       EventLatencyScrollForPresentedFrameReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_GESTURE_SCROLL_BEGIN, event_time, ScrollInputType::kWheel},
      {ui::ET_GESTURE_SCROLL_UPDATE, event_time, ScrollInputType::kWheel},
      {ui::ET_GESTURE_SCROLL_UPDATE, event_time, ScrollInputType::kWheel},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  viz::FrameTimingDetails frame_timing_details = BuildFrameTimingDetails();
  pipeline_reporter_->SetVizBreakdown(frame_timing_details);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kPresentedFrame,
      frame_timing_details.presentation_feedback.timestamp);

  pipeline_reporter_ = nullptr;

  const int total_latency_ms =
      (frame_timing_details.presentation_feedback.timestamp - event_time)
          .InMicroseconds();
  const int swap_end_latency_ms =
      (frame_timing_details.swap_timings.swap_end - event_time)
          .InMicroseconds();
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatency", 1);
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatencyToSwapEnd", 1);
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatency", 2);
  histogram_tester.ExpectTotalCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatencyToSwapEnd", 2);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatency", total_latency_ms,
      1);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollBegin.Wheel.TotalLatencyToSwapEnd",
      swap_end_latency_ms, 1);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatency", total_latency_ms,
      2);
  histogram_tester.ExpectBucketCount(
      "EventLatency.GestureScrollUpdate.Wheel.TotalLatencyToSwapEnd",
      swap_end_latency_ms, 2);
}

// Tests that when the frame is not presented to the user, event latency metrics
// are not reported.
TEST_F(CompositorFrameReporterTest,
       EventLatencyForDidNotPresentFrameNotReported) {
  base::HistogramTester histogram_tester;

  const base::TimeTicks event_time = Now();
  std::vector<EventMetrics> events_metrics = {
      {ui::ET_TOUCH_PRESSED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
      {ui::ET_TOUCH_MOVED, event_time, base::nullopt},
  };
  EXPECT_THAT(events_metrics, ::testing::Each(IsWhitelisted()));

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kBeginImplFrameToSendBeginMainFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::kEndActivateToSubmitCompositorFrame,
      Now());

  AdvanceNowByMs(3);
  pipeline_reporter_->StartStage(
      CompositorFrameReporter::StageType::
          kSubmitCompositorFrameToPresentationCompositorFrame,
      Now());
  pipeline_reporter_->SetEventsMetrics(std::move(events_metrics));

  AdvanceNowByMs(3);
  pipeline_reporter_->TerminateFrame(
      CompositorFrameReporter::FrameTerminationStatus::kDidNotPresentFrame,
      Now());

  pipeline_reporter_ = nullptr;

  histogram_tester.ExpectTotalCount("EventLatency.TouchPressed.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("EventLatency.TouchMoved.TotalLatency", 0);
}

}  // namespace
}  // namespace cc
