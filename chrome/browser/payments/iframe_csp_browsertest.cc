// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class IframeCspTest : public PaymentRequestPlatformBrowserTestBase,
                      public ::testing::WithParamInterface<bool> {
 public:
  IframeCspTest() {
    if (WebPaymentAPICSPEnabled()) {
      features_.InitAndEnableFeature(::features::kWebPaymentAPICSP);
    } else {
      features_.InitAndDisableFeature(::features::kWebPaymentAPICSP);
    }
  }

  ~IframeCspTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    // kylepay.com is a payment app that supports just-in-time installation.
    app_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.com");
    ASSERT_TRUE(app_server_.Start());
  }

  bool WebPaymentAPICSPEnabled() const { return GetParam(); }

 protected:
  net::EmbeddedTestServer app_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(IframeCspTest, Show) {
  NavigateTo("/csp_test_main.html");

  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  // Filter for console messages related to the CSP failure. There should be
  // none.
  console_observer.SetPattern(
      "Refused to load the image 'https://kylepay.com:*/icon.png *");

  GURL iframe_url =
      https_server()->GetURL("other.example", "/csp_test_iframe.html");
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      GetActiveWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  EXPECT_EQ(iframe_url, iframe->GetLastCommittedURL());

  // Set up test manifest downloader that knows how to fake origin.
  const std::string method_name = "kylepay.com";
  SetDownloaderAndIgnorePortInOriginComparisonForTestingInFrame(
      {{method_name, &app_server_}}, iframe);

  if (WebPaymentAPICSPEnabled()) {
    EXPECT_EQ(
        "RangeError: Failed to construct 'PaymentRequest': "
        "https://kylepay.com/webpay payment method identifier violates Content "
        "Security Policy.",
        content::EvalJs(iframe, "checkCanMakePayment()"));
  } else {
    // CSP is disabled.
    EXPECT_EQ(true, content::EvalJs(iframe, "checkCanMakePayment()"));
  }

  EXPECT_TRUE(console_observer.messages().empty());
}

INSTANTIATE_TEST_SUITE_P(All, IframeCspTest, ::testing::Bool());

}  // namespace payments