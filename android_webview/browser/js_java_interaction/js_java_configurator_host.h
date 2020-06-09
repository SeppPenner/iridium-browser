// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_

#include <memory>
#include <vector>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace android_webview {

class AwOriginMatcher;
struct DocumentStartJavascript;
struct JsObject;
class JsToJavaMessaging;
class WebMessageHostFactory;

// This class is 1:1 with WebContents, when AddWebMessageListener() is called,
// it stores the information in this class and send them to renderer side
// JsJavaConfigurator if there is any. When RenderFrameCreated() gets called, it
// needs to configure that new RenderFrame with the information stores in this
// class.
class JsJavaConfiguratorHost : public content::WebContentsObserver {
 public:
  explicit JsJavaConfiguratorHost(content::WebContents* web_contents);
  ~JsJavaConfiguratorHost() override;

  // Captures the result of adding script. There are two possibilities when
  // adding script: there was an error, in which case |error_message| is set,
  // otherwise the add was successful and |script_id| is set.
  struct AddScriptResult {
    AddScriptResult();
    AddScriptResult(const AddScriptResult&);
    AddScriptResult& operator=(const AddScriptResult&);
    ~AddScriptResult();

    base::Optional<std::string> error_message;
    base::Optional<int> script_id;
  };

  // Native side AddDocumentStartJavascript, returns an error message if the
  // parameters didn't pass necessary checks.
  AddScriptResult AddDocumentStartJavascript(
      const base::string16& script,
      const std::vector<std::string>& allowed_origin_rules);

  bool RemoveDocumentStartJavascript(int script_id);

  // Adds a new WebMessageHostFactory. For any urls that match
  // |allowed_origin_rules|, |js_object_name| is registered as a JS object that
  // can be used by script on the page to send and receive messages. Returns
  // an empty string on success. On failure, the return string gives the error
  // message.
  base::string16 AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const base::string16& js_object_name,
      const std::vector<std::string>& allowed_origin_rules);

  // Returns the factory previously registered under the specified name.
  void RemoveWebMessageHostFactory(const base::string16& js_object_name);

  struct RegisteredFactory {
    base::string16 js_name;
    AwOriginMatcher allowed_origin_rules;
    WebMessageHostFactory* factory = nullptr;
  };

  // Returns the registered factories.
  std::vector<RegisteredFactory> GetWebMessageHostFactories();

  // content::WebContentsObserver implementations
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  void NotifyFrameForWebMessageListener(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAllDocumentStartJavascripts(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAddDocumentStartJavascript(
      const DocumentStartJavascript* script,
      content::RenderFrameHost* render_frame_host);

  void NotifyFrameForRemoveDocumentStartJavascript(
      int32_t script_id,
      content::RenderFrameHost* render_frame_host);

  int32_t next_script_id_ = 0;
  std::vector<DocumentStartJavascript> scripts_;
  std::vector<std::unique_ptr<JsObject>> js_objects_;
  std::map<content::RenderFrameHost*,
           std::vector<std::unique_ptr<JsToJavaMessaging>>>
      js_to_java_messagings_;

  DISALLOW_COPY_AND_ASSIGN(JsJavaConfiguratorHost);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_JAVA_CONFIGURATOR_HOST_H_
