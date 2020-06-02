// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace {

const char kReauthDonePath[] = "/embedded/xreauth/chrome?done";
const char kReauthPath[] = "/embedded/xreauth/chrome";
const char kChallengePath[] = "/challenge";

std::unique_ptr<net::test_server::BasicHttpResponse> CreateRedirectResponse(
    const GURL& redirect_url) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleReauthURL(
    const GURL& base_url,
    const net::test_server::HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kReauthPath)) {
    return nullptr;
  }

  GURL request_url = request.GetURL();
  std::string parameter =
      net::UnescapeBinaryURLComponent(request_url.query_piece());

  if (parameter.empty()) {
    // Parameterless request redirects to the fake challenge page.
    return CreateRedirectResponse(base_url.Resolve(kChallengePath));
  }

  if (parameter == "done") {
    // On success, the reauth returns HTTP_NO_CONTENT response.
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_NO_CONTENT);
    return http_response;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

// Browser tests for SigninReauthViewController.
class SigninReauthViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  SigninReauthViewControllerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitAndEnableFeature(kSigninReauthPrompt);
  }

  void SetUp() override {
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url().spec());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleReauthURL, base_url()));
    reauth_challenge_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), kChallengePath);
    https_server()->StartAcceptingConnections();

    account_id_ = signin::SetUnconsentedPrimaryAccount(identity_manager(),
                                                       "alice@gmail.com")
                      .account_id;

    reauth_result_loop_ = std::make_unique<base::RunLoop>();
  }

  void ShowReauthPrompt() {
    abort_handle_ = browser()->signin_view_controller()->ShowReauthPrompt(
        account_id_,
        base::BindOnce(&SigninReauthViewControllerBrowserTest::OnReauthResult,
                       base::Unretained(this)));
  }

  // This method must be called only after the reauth dialog has been opened.
  void RedirectGaiaChallengeTo(const GURL& redirect_url) {
    reauth_challenge_response_->WaitForRequest();
    auto redirect_response = CreateRedirectResponse(redirect_url);
    reauth_challenge_response_->Send(redirect_response->ToResponseString());
    reauth_challenge_response_->Done();
  }

  void OnReauthResult(signin::ReauthResult reauth_result) {
    reauth_result_ = reauth_result;
    reauth_result_loop_->Quit();
  }

  signin::ReauthResult WaitForReauthResult() {
    reauth_result_loop_->Run();
    return reauth_result_;
  }

  void ResetAbortHandle() { abort_handle_.reset(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  GURL base_url() { return https_server()->base_url(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  SigninReauthViewController* signin_reauth_view_controller() {
    SigninViewController* signin_view_controller =
        browser()->signin_view_controller();
    DCHECK(signin_view_controller->ShowsModalDialog());
    return static_cast<SigninReauthViewController*>(
        signin_view_controller->GetModalDialogDelegateForTesting());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      reauth_challenge_response_;
  CoreAccountId account_id_;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle_;

  std::unique_ptr<base::RunLoop> reauth_result_loop_;
  signin::ReauthResult reauth_result_;
};

// Tests that the abort handle cancels an ongoing reauth flow.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_AbortHandle) {
  ShowReauthPrompt();
  ResetAbortHandle();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kCancelled);
}

// Tests canceling the reauth dialog through CloseModalSignin().
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_CloseModalSignin) {
  ShowReauthPrompt();
  browser()->signin_view_controller()->CloseModalSignin();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kCancelled);
}

// Tests closing the reauth dialog through by clicking on the close button (the
// X).
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CloseReauthDialog) {
  ShowReauthPrompt();
  // The test cannot depend on Views implementation so it simulates clicking on
  // the close button through calling the close event.
  signin_reauth_view_controller()->OnModalSigninClosed();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
}

// Tests clicking on the cancel button in the reauth dialog.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CancelReauthDialog) {
  ShowReauthPrompt();
  login_ui_test_utils::CancelReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
}

// Tests the reauth result in case Gaia page failed to load.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       GaiaChallengeLoadFailed) {
  ShowReauthPrompt();
  login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(5));
  RedirectGaiaChallengeTo(https_server()->GetURL("/close-socket"));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kLoadFailed);
}

// Tests clicking on the confirm button in the reauth dialog. Reauth completes
// before the confirmation.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       ConfirmReauthDialog_AfterReauthSuccess) {
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(https_server()->GetURL(kReauthDonePath));
  login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);
}

// Tests clicking on the confirm button in the reauth dialog. Reauth completes
// after the confirmation.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       ConfirmReauthDialog_BeforeReauthSuccess) {
  ShowReauthPrompt();
  login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), base::TimeDelta::FromSeconds(5));
  RedirectGaiaChallengeTo(https_server()->GetURL(kReauthDonePath));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);
}
