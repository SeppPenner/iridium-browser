// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_workers_observer.h"
#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";
}  // namespace

class TestIsolatedPrerenderTabHelper : public IsolatedPrerenderTabHelper {
 public:
  explicit TestIsolatedPrerenderTabHelper(content::WebContents* web_contents)
      : IsolatedPrerenderTabHelper(web_contents) {}

  void SetURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = url_loader_factory;
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override {
    return url_loader_factory_.get();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

class IsolatedPrerenderTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  IsolatedPrerenderTabHelperTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~IsolatedPrerenderTabHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    IsolatedPrerenderService* isolated_prerender_service =
        IsolatedPrerenderServiceFactory::GetForProfile(profile());
    isolated_prerender_service->service_workers_observer()
        ->CallOnHasUsageInfoForTesting({});

    tab_helper_ =
        std::make_unique<TestIsolatedPrerenderTabHelper>(web_contents());
    tab_helper_->SetURLLoaderFactory(test_shared_loader_factory_);

    SetDataSaverEnabled(true);
  }

  void TearDown() override {
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(profile()->GetPrefs(), enabled);
  }

  void MakeNavigationPrediction(const content::WebContents* web_contents,
                                const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(profile())
        ->OnPredictionUpdated(web_contents, doc_url, predicted_urls);
    task_environment()->RunUntilIdle();
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  IsolatedPrerenderTabHelper* tab_helper() const { return tab_helper_.get(); }

  size_t PrefetchedResponseSize() const {
    return tab_helper_->prefetched_responses_size_for_testing();
  }

  void VerifyNIK(const net::NetworkIsolationKey& key) {
    EXPECT_FALSE(key.IsEmpty());
    EXPECT_FALSE(key.IsTransient());
    EXPECT_TRUE(key.IsFullyPopulated());
    EXPECT_TRUE(base::StartsWith(key.ToString(), "opaque non-transient ",
                                 base::CompareCase::SENSITIVE));
  }

  network::ResourceRequest VerifyCommonRequestState(const GURL& url) {
    SCOPED_TRACE(url.spec());
    EXPECT_EQ(RequestCount(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_EQ(request->request.load_flags,
              net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kOmit);

    EXPECT_TRUE(request->request.trusted_params.has_value());
    VerifyNIK(request->request.trusted_params.value().network_isolation_key);

    return request->request;
  }

  std::string RequestHeader(const std::string& key) {
    if (test_url_loader_factory_.NumPending() != 1)
      return std::string();

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    std::string value;
    if (request->request.headers.GetHeader(key, &value))
      return value;

    return std::string();
  }

  void MakeResponseAndWait(net::HttpStatusCode http_status,
                           net::Error net_error,
                           const std::string& mime_type,
                           std::vector<std::string> headers,
                           const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = network::CreateURLResponseHead(http_status);
    head->mime_type = mime_type;
    for (const std::string& header : headers) {
      head->headers->AddHeader(header);
    }
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void ClearResponses() { test_url_loader_factory_.ClearResponses(); }

  bool SetCookie(content::BrowserContext* browser_context,
                 const GURL& url,
                 const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(browser_context)
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), base::nullopt /* server_time */));
    EXPECT_TRUE(cc.get());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    cookie_manager->SetCanonicalCookie(
        *cc.get(), url.scheme(), options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CanonicalCookie::CookieInclusionStatus set_cookie_status) {
              *result = set_cookie_status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

 private:
  std::unique_ptr<TestIsolatedPrerenderTabHelper> tab_helper_;
};

TEST_F(IsolatedPrerenderTabHelperTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DataSaverDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  SetDataSaverEnabled(false);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, GoogleSRPOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, SRPOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/photos?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, HTTPSPredictionsOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.not-google.com/search?q=cats");
  GURL prediction_url("http://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchGoogleLinks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("http://www.google.com/user");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, DontFetchIPAddresses) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://123.234.123.234/meow");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, WrongWebContents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(nullptr, doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, HasPurposePrefetchHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  EXPECT_EQ(RequestHeader("Purpose"), "prefetch");
}

TEST_F(IsolatedPrerenderTabHelperTest, NoCookies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  ASSERT_TRUE(SetCookie(profile(), prediction_url, "testing"));

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, 2XXOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);
  EXPECT_EQ(PrefetchedResponseSize(), 0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, NetErrorOKOnly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*headers=*/{}, kHTMLBody);
  EXPECT_EQ(PrefetchedResponseSize(), 0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, NonHTML) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/javascript",
                      /*headers=*/{},
                      /*body=*/"console.log('Hello world');");
  EXPECT_EQ(PrefetchedResponseSize(), 0U);
}

TEST_F(IsolatedPrerenderTabHelperTest, UserSettingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, SuccessCase) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  network::ResourceRequest request = VerifyCommonRequestState(prediction_url);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      {"X-Testing: Hello World"}, kHTMLBody);

  EXPECT_EQ(PrefetchedResponseSize(), 1U);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(prediction_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));

  EXPECT_EQ(resp->network_isolation_key(),
            request.trusted_params.value().network_isolation_key);
  VerifyNIK(resp->network_isolation_key());
}

TEST_F(IsolatedPrerenderTabHelperTest, LimitedNumberOfPrefetches_Zero) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "0"}});

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest,
       NumberOfPrefetches_UnlimitedByExperiment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "-1"}});

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, NumberOfPrefetches_UnlimitedByCmdLine) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_3);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, LimitedNumberOfPrefetches) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_srp_prefetches", "2"}});

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");
  GURL prediction_url_3("https://www.catz-rule.com/");
  MakeNavigationPrediction(
      web_contents(), doc_url,
      {prediction_url_1, prediction_url_2, prediction_url_3});

  VerifyCommonRequestState(prediction_url_1);
  // Failed responses do not retry or attempt more requests in the list.
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType, {},
                      kHTMLBody);
  VerifyCommonRequestState(prediction_url_2);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingNotStartedWhileInvisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  web_contents()->WasHidden();

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingPausedWhenInvisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url_1("https://www.cat-food.com/");
  GURL prediction_url_2("https://www.dogs-r-dumb.com/");

  MakeNavigationPrediction(web_contents(), doc_url,
                           {prediction_url_1, prediction_url_2});
  VerifyCommonRequestState(prediction_url_1);

  // When hidden, the current prefetch is allowed to finish.
  web_contents()->WasHidden();
  VerifyCommonRequestState(prediction_url_1);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType, {}, kHTMLBody);

  // But no more prefetches should start when hidden.
  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, PrefetchingRestartedWhenVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  web_contents()->WasHidden();

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);

  web_contents()->WasShown();

  VerifyCommonRequestState(prediction_url);
}

TEST_F(IsolatedPrerenderTabHelperTest, ServiceWorkerRegistered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(prediction_url);

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(IsolatedPrerenderTabHelperTest, ServiceWorkerNotRegistered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL service_worker_registration("https://www.service-worker.com/");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(service_worker_registration);

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

  VerifyCommonRequestState(prediction_url);
}

class IsolatedPrerenderTabHelperRedirectTest
    : public IsolatedPrerenderTabHelperTest {
 public:
  IsolatedPrerenderTabHelperRedirectTest() = default;
  ~IsolatedPrerenderTabHelperRedirectTest() override = default;

  void WalkRedirectChainUntilFinalRequest(std::vector<GURL> redirect_chain) {
    ASSERT_GE(redirect_chain.size(), 2U)
        << "redirect_chain must contain the full redirect chain, with the "
           "first element being the first request url, the last element being "
           "the final request url, and any intermediate steps in the middle";

    // Since the prefetches do not follow redirects, but instead have to pause
    // and query the cookie jar every time, each step in the redirect chain
    // needs to be handled like a separate request/response pair.
    for (size_t i = 0; i < redirect_chain.size() - 1; i++) {
      network::TestURLLoaderFactory::Redirects redirects;
      net::RedirectInfo info;
      info.new_url = redirect_chain[i + 1];
      info.status_code = net::HTTP_TEMPORARY_REDIRECT;
      auto head = network::CreateURLResponseHead(net::HTTP_TEMPORARY_REDIRECT);
      redirects.push_back(std::make_pair(info, head->Clone()));

      network::TestURLLoaderFactory::PendingRequest* request =
          test_url_loader_factory_.GetPendingRequest(0);
      ASSERT_TRUE(request);
      EXPECT_EQ(request->request.url, redirect_chain[i]);

      test_url_loader_factory_.AddResponse(
          redirect_chain[i], std::move(head), "unused body during redirect",
          network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

      task_environment()->RunUntilIdle();
    }
    // Clear responses in the network service so we can inspect the next
    // request that comes in before it is responded to.
    ClearResponses();
  }

  void MakeFinalResponse(const GURL& final_url,
                         net::HttpStatusCode final_status,
                         std::vector<std::string> final_headers,
                         const std::string& final_body) {
    auto final_head = network::CreateURLResponseHead(final_status);
    final_head->mime_type = kHTMLMimeType;
    for (const std::string& header : final_headers) {
      final_head->headers->AddHeader(header);
    }
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    EXPECT_EQ(final_url, request->request.url);
    test_url_loader_factory_.AddResponse(
        final_url, std::move(final_head), final_body,
        network::URLLoaderCompletionStatus(net::OK));
    task_environment()->RunUntilIdle();

    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    ClearResponses();
  }

  void RunNoRedirectTest(const GURL& redirect_url) {
    GURL doc_url("https://www.google.com/search?q=cats");
    GURL prediction_url("https://www.cat-food.com/");

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

    MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});

    VerifyCommonRequestState(prediction_url);
    WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
    // Redirect should not be followed.
    EXPECT_EQ(RequestCount(), 0);
    EXPECT_EQ(PrefetchedResponseSize(), 0U);
  }
};

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_Cookies) {
  GURL site_with_cookies("https://cookies.com");
  ASSERT_TRUE(SetCookie(profile(), site_with_cookies, "testing"));
  RunNoRedirectTest(site_with_cookies);
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_Insecure) {
  RunNoRedirectTest(GURL("http://insecure.com"));
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_Google) {
  RunNoRedirectTest(GURL("https://www.google.com"));
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, NoRedirect_ServiceWorker) {
  GURL site_with_worker("https://service-worker.com");

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile());
  content::ServiceWorkerContextObserver* observer =
      isolated_prerender_service->service_workers_observer();
  observer->OnRegistrationCompleted(site_with_worker);

  RunNoRedirectTest(site_with_worker);
}

TEST_F(IsolatedPrerenderTabHelperRedirectTest, SuccessfulRedirect) {
  GURL doc_url("https://www.google.com/search?q=cats");
  GURL prediction_url("https://www.cat-food.com/");
  GURL redirect_url("https://redirect-here.com");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  MakeNavigationPrediction(web_contents(), doc_url, {prediction_url});
  VerifyCommonRequestState(prediction_url);

  WalkRedirectChainUntilFinalRequest({prediction_url, redirect_url});
  MakeFinalResponse(redirect_url, net::HTTP_OK, {"X-Testing: Hello World"},
                    kHTMLBody);

  EXPECT_EQ(PrefetchedResponseSize(), 1U);

  std::unique_ptr<PrefetchedMainframeResponseContainer> resp =
      tab_helper()->TakePrefetchResponse(redirect_url);
  ASSERT_TRUE(resp);
  EXPECT_EQ(*resp->TakeBody(), kHTMLBody);

  network::mojom::URLResponseHeadPtr head = resp->TakeHead();
  EXPECT_TRUE(head->headers->HasHeaderValue("X-Testing", "Hello World"));
}
