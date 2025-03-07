// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/download/confirm_download_closing_overlay.h"
#import "ios/chrome/browser/download/confirm_download_replacing_overlay.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#include "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/external_app_util.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/fakes/fake_contained_presenter.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";
const int64_t kTestTotalBytes = 10;
const int64_t kTestReceivedBytes = 0;
NSString* const kTestSuggestedFileName = @"file.zip";

// Creates a fake download task for testing.
std::unique_ptr<web::FakeDownloadTask> CreateTestTask() {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
  task->SetTotalBytes(kTestTotalBytes);
  task->SetReceivedBytes(kTestReceivedBytes);
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  return task;
}

// Substitutes real TabHelper for testing.
class StubTabHelper : public DownloadManagerTabHelper {
 public:
  StubTabHelper(web::WebState* web_state)
      : DownloadManagerTabHelper(web_state, /*delegate=*/nullptr) {}
  DISALLOW_COPY_AND_ASSIGN(StubTabHelper);
};

}  // namespace

// Test fixture for testing DownloadManagerCoordinator class.
class DownloadManagerCoordinatorTest : public PlatformTest {
 protected:
  DownloadManagerCoordinatorTest()
      : presenter_([[FakeContainedPresenter alloc] init]),
        base_view_controller_([[UIViewController alloc] init]),
        browser_(std::make_unique<TestBrowser>()),
        document_interaction_controller_class_(
            OCMClassMock([UIDocumentInteractionController class])),
        tab_helper_(&web_state_),
        coordinator_([[DownloadManagerCoordinator alloc]
            initWithBaseViewController:base_view_controller_
                               browser:browser_.get()]) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_.presenter = presenter_;
  }
  ~DownloadManagerCoordinatorTest() override {
    // Stop to avoid holding a dangling pointer to destroyed task.
    @autoreleasepool {
      // task_environment_ has to outlive the coordinator. Dismissing
      // coordinator retains are autoreleases it.
      [coordinator_ stop];
    }

    [document_interaction_controller_class_ stopMocking];
    [application_ stopMocking];
  }

  web::WebTaskEnvironment task_environment_;
  FakeContainedPresenter* presenter_;
  UIViewController* base_view_controller_;
  std::unique_ptr<Browser> browser_;
  ScopedKeyWindow scoped_key_window_;
  web::TestWebState web_state_;
  id document_interaction_controller_class_;
  StubTabHelper tab_helper_;
  // Application can be lazily created by tests, but it has to be OCMock.
  // Destructor will call -stopMocking on this object to make sure that
  // UIApplication is not mocked after these test finish running.
  id application_;
  DownloadManagerCoordinator* coordinator_;
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;
};

// Tests starting the coordinator. Verifies that view controller is presented
// without animation (default configuration) and that
// DownloadManagerViewController is propertly configured and presented.
TEST_F(DownloadManagerCoordinatorTest, Start) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  // By default coordinator presents without animation.
  EXPECT_FALSE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches download
  // task.
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);
}

// Tests stopping coordinator. Verifies that hiding web states dismisses the
// presented view controller and download task is reset to null (to prevent a
// stale raw pointer).
TEST_F(DownloadManagerCoordinatorTest, Stop) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  coordinator_.downloadTask = &task;
  [coordinator_ start];
  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ stop];
  }

  // Verify that child view controller is removed and download task is set to
  // null.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
}

// Tests destroying coordinator during the download.
TEST_F(DownloadManagerCoordinatorTest, DestructionDuringDownload) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task.Start(std::make_unique<net::URLFetcherFileWriter>(
      base::ThreadTaskRunnerHandle::Get(), path));

  @autoreleasepool {
    // These calls will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];

    // Destroy coordinator before destroying the download task.
    coordinator_ = nil;
  }

  // Verify that child view controller is removed.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
}

// Tests downloadManagerTabHelper:didCreateDownload:webStateIsVisible: callback
// for visible web state. Verifies that coordinator's properties are set up and
// that DownloadManagerViewController is properly configured and presented with
// animation.
TEST_F(DownloadManagerCoordinatorTest, DelegateCreatedDownload) {
  auto task = CreateTestTask();
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUI", 0);

  [coordinator_ downloadManagerTabHelper:&tab_helper_
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];

  // Verify that coordinator's properties are set up.
  EXPECT_EQ(task.get(), coordinator_.downloadTask);
  EXPECT_TRUE(coordinator_.animatesPresentation);

  // First presentation of Download Manager UI should be animated.
  EXPECT_TRUE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches download
  // task.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);

  // Verify that UMA action was logged.
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUI", 1);
}

// Tests calling downloadManagerTabHelper:didCreateDownload:webStateIsVisible:
// callback twice. Second call should replace the old download task with the new
// one.
TEST_F(DownloadManagerCoordinatorTest, DelegateReplacedDownload) {
  auto task = CreateTestTask();
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task->Start(std::make_unique<net::URLFetcherFileWriter>(
      base::ThreadTaskRunnerHandle::Get(), path));
  task->SetDone(true);

  [coordinator_ downloadManagerTabHelper:&tab_helper_
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];

  // Verify that presented view controller is DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches download
  // task.
  EXPECT_NSEQ(@"file.zip", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Open in…",
              [viewController.actionButton titleForState:UIControlStateNormal]);

  // Replace download task with a new one.
  auto new_task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                       didCreateDownload:new_task.get()
                       webStateIsVisible:YES];

  // Verify that DownloadManagerViewController configuration matches new
  // download task.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);
}

// Tests downloadManagerTabHelper:didCreateDownload:webStateIsVisible: callback
// for hidden web state. Verifies that coordinator ignores callback from
// a background tab.
TEST_F(DownloadManagerCoordinatorTest,
       DelegateCreatedDownloadForHiddenWebState) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                       didCreateDownload:task.get()
                       webStateIsVisible:NO];

  // Background tab should not present Download Manager UI.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests downloadManagerTabHelper:didHideDownload: callback. Verifies that
// hiding web states dismisses the presented view controller and download task
// is reset to null (to prevent a stale raw pointer).
TEST_F(DownloadManagerCoordinatorTest, DelegateHideDownload) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];
  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ downloadManagerTabHelper:&tab_helper_
                           didHideDownload:task.get()];
  }

  // Verify that child view controller is removed and download task is set to
  // null.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
}

// Tests downloadManagerTabHelper:didShowDownload: callback. Verifies that
// showing web state presents download manager UI for that web state.
TEST_F(DownloadManagerCoordinatorTest, DelegateShowDownload) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                         didShowDownload:task.get()];

  // Only first presentation is animated. Switching between tab should create
  // the impression that UI was never dismissed.
  EXPECT_FALSE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches download
  // task for shown web state.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
}

// Tests closing view controller. Coordinator should be stopped and task
// cancelled.
TEST_F(DownloadManagerCoordinatorTest, Close) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadClose"));
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidClose:viewController];
  }

  // Verify that child view controller is removed, download task is set to null
  // and download task is cancelled.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
  EXPECT_EQ(web::DownloadTask::State::kCancelled, task.GetState());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::NotStarted),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadClose"));
}

// Tests presenting Install Google Drive dialog. Coordinator presents StoreKit
// dialog and hides Install Google Drive button.
TEST_F(DownloadManagerCoordinatorTest, InstallDrive) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Make Install Google Drive UI visible.
  [viewController setInstallDriveButtonVisible:YES animated:NO];
  // The button itself is never hidden, but the superview which contains the
  // button changes it's alpha.
  ASSERT_EQ(1.0f, viewController.installDriveButton.superview.alpha);

  ASSERT_EQ(
      0, user_action_tester_.GetActionCount("IOSDownloadInstallGoogleDrive"));
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        installDriveForDownloadManagerViewController:viewController];
  }
  // Verify that Store Kit dialog was presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [SKStoreProductViewController class];
  }));

  // Verify that Install Google Drive UI is hidden.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return viewController.installDriveButton.superview.alpha == 0.0f;
  }));

  // Simulate Google Drive app installation and verify that expected histograms
  // have been recorded.
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount("IOSDownloadInstallGoogleDrive"));
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
  // SKStoreProductViewController uses UIApplication, so it's not possible to
  // install the mock before the test run.
  application_ = OCMClassMock([UIApplication class]);
  OCMStub([application_ sharedApplication]).andReturn(application_);
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);
}

// Tests presenting Open In... menu without actually opening the download.
TEST_F(DownloadManagerCoordinatorTest, OpenIn) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
  task->SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [view_controller class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task->Start(std::make_unique<net::URLFetcherFileWriter>(
      base::ThreadTaskRunnerHandle::Get(), path));

  id download_view_controller_mock = OCMPartialMock(view_controller);

  // Stub UIActivityViewController.
  OCMStub([download_view_controller_mock presentViewController:[OCMArg any]
                                                      animated:YES
                                                    completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak id object;
        [invocation getArgument:&object atIndex:2];
        EXPECT_EQ([UIActivityViewController class], [object class]);
        UIActivityViewController* open_in_controller =
            base::mac::ObjCCastStrict<UIActivityViewController>(object);
        EXPECT_EQ(open_in_controller.excludedActivityTypes.count, 2.0);
      });

  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadOpenIn"));

  // Present Open In... menu.
  @autoreleasepool {
    // These calls will retain coordinator, which should outlive thread bundle.
    [view_controller.delegate
        downloadManagerViewControllerDidStartDownload:view_controller];

    // Complete the download before presenting Open In... menu.
    task->SetDone(true);

    [view_controller.delegate
        presentOpenInForDownloadManagerViewController:view_controller];
  }

  // Download task is destroyed without opening the file.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Completed),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample>(
          DownloadedFileAction::NoActionOrOpenedViaExtension),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::SucceededWithoutBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadOpenIn"));
}

// Tests destroying download task for in progress download.
TEST_F(DownloadManagerCoordinatorTest, DestroyInProgressDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  web::DownloadTask* task_ptr = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start and the download.
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download task is destroyed before the download is complete.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileInBackground", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests quitting the app during in-progress download.
TEST_F(DownloadManagerCoordinatorTest, QuitDuringInProgressDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  web::DownloadTask* task_ptr = task.get();
  auto web_state = std::make_unique<web::TestWebState>();
  browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start and the download.
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Web States are closed without user action only during app termination.
  browser_->GetWebStateList()->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);

  // Download task is destroyed before the download is complete.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::CanceledAfterAppQuit),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests closing view controller while the download is in progress. Coordinator
// should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest, CloseInProgressDownload) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetWebState(&web_state_);
  task.Start(std::make_unique<net::URLFetcherStringWriter>());
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount(
                   "IOSDownloadTryCloseWhenInProgress"));

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidClose:viewController];
  }
  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Stop Download?", config->title());
  EXPECT_FALSE(config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  EXPECT_NSEQ(@"Stop", config->button_configs()[0].title);
  EXPECT_EQ(kDownloadCloseActionName,
            config->button_configs()[0].user_action_name);
  EXPECT_NSEQ(@"Continue", config->button_configs()[1].title);
  EXPECT_EQ(kDownloadDoNotCloseActionName,
            config->button_configs()[1].user_action_name);

  // Stop to avoid holding a dangling pointer to destroyed task.
  queue->CancelAllRequests();
  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "IOSDownloadTryCloseWhenInProgress"));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kDownloadCloseActionName));
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount(kDownloadDoNotCloseActionName));
}

// Tests downloadManagerTabHelper:decidePolicyForDownload:completionHandler:.
// Coordinator should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest, DecidePolicyForDownload) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetWebState(&web_state_);
  coordinator_.downloadTask = &task;

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                 decidePolicyForDownload:&task
                       completionHandler:^(NewDownloadPolicy){
                       }];

  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Start New Download?", config->title());
  EXPECT_NSEQ(@"This will stop all progress for your current download.",
              config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  EXPECT_NSEQ(@"OK", config->button_configs()[0].title);
  EXPECT_EQ(kDownloadReplaceActionName,
            config->button_configs()[0].user_action_name);
  EXPECT_NSEQ(@"Cancel", config->button_configs()[1].title);
  EXPECT_EQ(kDownloadDoNotReplaceActionName,
            config->button_configs()[1].user_action_name);

  queue->CancelAllRequests();
  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
}

// Tests downloadManagerTabHelper:decidePolicyForDownload:completionHandler:.
// Coordinator should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest,
       DecidePolicyForDownloadFromBackgroundTab) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetWebState(&web_state_);
  coordinator_.downloadTask = nullptr;  // Current Tab does not have task.

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  [coordinator_ downloadManagerTabHelper:&tab_helper_
                 decidePolicyForDownload:&task
                       completionHandler:^(NewDownloadPolicy){
                       }];

  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Start New Download?", config->title());
  EXPECT_NSEQ(@"This will stop all progress for your current download.",
              config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  EXPECT_NSEQ(@"OK", config->button_configs()[0].title);
  EXPECT_EQ(kDownloadReplaceActionName,
            config->button_configs()[0].user_action_name);
  EXPECT_NSEQ(@"Cancel", config->button_configs()[1].title);
  EXPECT_EQ(kDownloadDoNotReplaceActionName,
            config->button_configs()[1].user_action_name);

  queue->CancelAllRequests();
  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
}

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into download directory.
TEST_F(DownloadManagerCoordinatorTest, StartDownload) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::DownloadTask* task_ptr = &task;
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download file should be located in download directory.
  base::FilePath file = task.GetResponseWriter()->AsFileWriter()->file_path();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileInBackground", 0);
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount("MobileDownloadRetryDownload"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));
}

// Tests retrying the download. Verifies that kDownloadManagerRetryDownload UMA
// metric is logged.
TEST_F(DownloadManagerCoordinatorTest, RetryingDownload) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  web::DownloadTask* task_ptr = &task;
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  // First download is a failure.
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task.SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task.SetDone(true);
  ASSERT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));

  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  histogram_tester_.ExpectUniqueSample("Download.IOSDownloadedFileNetError",
                                       -net::ERR_INTERNET_DISCONNECTED, 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Failure), 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::FailedWithoutBackgrounding),
      1);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("MobileDownloadRetryDownload"));
}

// Tests download failure in background.
TEST_F(DownloadManagerCoordinatorTest, FailingInBackground) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  // Start and immediately fail the download.
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task.SetPerformedBackgroundDownload(true);
  task.SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task.SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Failure), 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::FailedWithBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests successful download in background.
TEST_F(DownloadManagerCoordinatorTest, SucceedingInBackground) {
  web::FakeDownloadTask task(GURL(kTestUrl), kTestMimeType);
  task.SetSuggestedFilename(base::SysNSStringToUTF16(kTestSuggestedFileName));
  coordinator_.downloadTask = &task;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task.Start(std::make_unique<net::URLFetcherFileWriter>(
      base::ThreadTaskRunnerHandle::Get(), path));

  // Start the download.
  @autoreleasepool {
    // This call will retain coordinator, which should outlive thread bundle.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Complete the download to log UMA.
  task.SetPerformedBackgroundDownload(true);
  task.SetDone(true);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::SucceededWithBackgrounding),
      1);
}

// Tests that viewController returns correct view controller if coordinator is
// started and nil when stopped.
TEST_F(DownloadManagerCoordinatorTest, ViewController) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  ASSERT_FALSE(coordinator_.viewController);
  [coordinator_ start];

  // Verify that presented view controller is DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify view controller property.
  EXPECT_NSEQ(viewController, coordinator_.viewController);

  @autoreleasepool {
    // task_environment_ has to outlive the coordinator. Dismissing coordinator
    // retains are autoreleases it.
    [coordinator_ stop];
  }
  EXPECT_FALSE(coordinator_.viewController);
}
