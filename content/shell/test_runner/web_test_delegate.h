// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_TEST_DELEGATE_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_TEST_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"

#define WEBTESTRUNNER_NEW_HISTORY_CAPTURE

namespace base {
class DictionaryValue;
}  // namespace base

namespace blink {
class WebLocalFrame;
class WebPlugin;
struct WebPluginParams;
struct WebSize;
class WebView;
}  // namespace blink

namespace content {
struct TestPreferences;

class WebTestDelegate {
 public:
  virtual ~WebTestDelegate() = default;

  // Add a message to stderr (not saved to expected output files, for debugging
  // only).
  virtual void PrintMessageToStderr(const std::string& message) = 0;

  // Add a message to the text dump for the web test.
  virtual void PrintMessage(const std::string& message) = 0;

  virtual void PostTask(base::OnceClosure task) = 0;
  virtual void PostDelayedTask(base::OnceClosure task,
                               base::TimeDelta delay) = 0;

  // Register a new isolated filesystem with the given files, and return the
  // new filesystem id.
  virtual blink::WebString RegisterIsolatedFileSystem(
      const blink::WebVector<blink::WebString>& absolute_filenames) = 0;

  // Gets the current time in milliseconds since the UNIX epoch.
  virtual long long GetCurrentTimeInMillisecond() = 0;

  // Convert the provided relative path into an absolute path.
  virtual blink::WebString GetAbsoluteWebStringFromUTF8Path(
      const std::string& path) = 0;

  // Replaces file:///tmp/web_tests/ with the actual path to the
  // web_tests directory, or rewrite URLs generated from absolute
  // path links in web-platform-tests.
  virtual blink::WebURL RewriteWebTestsURL(const std::string& utf8_url,
                                           bool is_wpt_mode) = 0;

  // Manages the settings to used for web tests.
  virtual TestPreferences* Preferences() = 0;
  virtual void ApplyPreferences() = 0;
  virtual void SetPopupBlockingEnabled(bool block_popups) = 0;

  // Controls auto resize mode.
  virtual void EnableAutoResizeMode(const blink::WebSize& min_size,
                                    const blink::WebSize& max_size) = 0;
  virtual void DisableAutoResizeMode(const blink::WebSize& new_size) = 0;
  // Resets auto resize mode off in between tests, without requiring a size.
  virtual void ResetAutoResizeMode() = 0;

  virtual void NavigateSecondaryWindow(const GURL& url) = 0;
  virtual void InspectSecondaryWindow() = 0;

  // Controls WebSQL databases.
  virtual void ClearAllDatabases() = 0;
  // Setting quota to kDefaultDatabaseQuota will reset it to the default value.
  virtual void SetDatabaseQuota(int quota) = 0;

  // Controls Web Notifications.
  virtual void SimulateWebNotificationClick(
      const std::string& title,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply) = 0;
  virtual void SimulateWebNotificationClose(const std::string& title,
                                            bool by_user) = 0;

  // Controls Content Index entries.
  virtual void SimulateWebContentIndexDelete(const std::string& id) = 0;

  // Controls the device scale factor of the main WebView for hidpi tests.
  virtual void SetDeviceScaleFactor(float factor) = 0;

  // Enable zoom-for-dsf option.
  virtual void EnableUseZoomForDSF() = 0;

  // Returns whether or not the use-zoom-for-dsf flag is enabled.
  virtual bool IsUseZoomForDSFEnabled() = 0;

  // Change the device color space while running a web test.
  virtual void SetDeviceColorSpace(const std::string& name) = 0;

  // Set the bluetooth adapter while running a web test, uses Mojo to
  // communicate with the browser.
  virtual void SetBluetoothFakeAdapter(const std::string& adapter_name,
                                       base::OnceClosure callback) = 0;

  // If |enable| is true makes the Bluetooth chooser record its input and wait
  // for instructions from the test program on how to proceed. Otherwise
  // fall backs to the browser's default chooser.
  virtual void SetBluetoothManualChooser(bool enable) = 0;

  // Returns the events recorded since the last call to this function.
  virtual void GetBluetoothManualChooserEvents(
      base::OnceCallback<void(const std::vector<std::string>& events)>
          callback) = 0;

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  virtual void SendBluetoothManualChooserEvent(const std::string& event,
                                               const std::string& argument) = 0;

  // Controls which WebView should be focused.
  virtual void SetFocus(blink::WebView* web_view, bool focus) = 0;

  // Controls whether all cookies should be accepted or writing cookies in a
  // third-party context is blocked.
  virtual void SetBlockThirdPartyCookies(bool block) = 0;

  // The same as RewriteWebTestsURL unless the resource is a path starting
  // with /tmp/, then return a file URL to a temporary file.
  virtual std::string PathToLocalResource(const std::string& resource) = 0;

  // Sets the POSIX locale of the current process.
  virtual void SetLocale(const std::string& locale) = 0;

  // Returns the absolute path to a directory this test can write data in. This
  // returns the path to a fresh empty directory for each test that calls this
  // method, but repeatedly calling this from the same test will return the same
  // directory.
  virtual base::FilePath GetWritableDirectory() = 0;

  // Sets the path that should be returned when the test shows a file dialog.
  virtual void SetFilePathForMockFileDialog(const base::FilePath& path) = 0;

  // Invoked when web test runtime flags change.
  virtual void OnWebTestRuntimeFlagsChanged(
      const base::DictionaryValue& changed_values) = 0;

  // Invoked when the test finished.
  virtual void TestFinished() = 0;

  // Invoked when the embedder should close all but the main WebView.
  virtual void CloseRemainingWindows() = 0;

  virtual void DeleteAllCookies() = 0;

  // Returns the length of the back/forward history of the main WebView.
  virtual int NavigationEntryCount() = 0;

  // The following trigger navigations on the main WebView.
  virtual void GoToOffset(int offset) = 0;
  virtual void Reload() = 0;
  virtual void LoadURLForFrame(const blink::WebURL& url,
                               const std::string& frame_name) = 0;

  // Returns true if resource requests to external URLs should be permitted.
  virtual bool AllowExternalPages() = 0;

  // Sends a message to the WebTestPermissionManager in order for it to
  // update its database.
  virtual void SetPermission(const std::string& permission_name,
                             const std::string& permission_value,
                             const GURL& origin,
                             const GURL& embedding_origin) = 0;

  // Clear all the permissions set via SetPermission().
  virtual void ResetPermissions() = 0;

  // Causes the beforeinstallprompt event to be sent to the renderer.
  // |event_platforms| are the platforms to be sent with the event. Once the
  // event listener completes, |callback| will be called with a boolean
  // argument. This argument will be true if the event is canceled, and false
  // otherwise.
  virtual void DispatchBeforeInstallPromptEvent(
      const std::vector<std::string>& event_platforms,
      base::OnceCallback<void(bool)> callback) = 0;

  // Resolves the in-flight beforeinstallprompt event userChoice promise with a
  // platform of |platform|.
  virtual void ResolveBeforeInstallPromptPromise(
      const std::string& platform) = 0;

  virtual blink::WebPlugin* CreatePluginPlaceholder(
      const blink::WebPluginParams& params) = 0;

  // Run all pending idle tasks, and then run callback.
  virtual void RunIdleTasks(base::OnceClosure callback) = 0;

  // Forces a text input state update for the client of WebFrameWidget
  // associated with |frame|.
  virtual void ForceTextInputStateUpdate(blink::WebLocalFrame* frame) = 0;

  // Mark the orientation changed for fullscreen layout tests.
  virtual void SetScreenOrientationChanged() = 0;
};

}  // namespace content

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_TEST_DELEGATE_H_
