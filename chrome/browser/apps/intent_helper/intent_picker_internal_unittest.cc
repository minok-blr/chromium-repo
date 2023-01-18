// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {

TEST(IntentPickersInternalTest, TestIsGoogleRedirectorUrl) {
  // Test that redirect urls with different TLDs are still recognized.
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.9oo91e.qjz9zk.au/url?q=wathever")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.9oo91e.qjz9zk.mx/url?q=hotpot")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.co/url?q=query")));

  // Non-google domains shouldn't be used as valid redirect links.
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.not-9oo91e.qjz9zk/url?q=query")));
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.gooogle.com/url?q=legit_query")));

  // This method only takes "/url" as a valid path, it needs to contain a query,
  // we don't analyze that query as it will expand later on in the same
  // throttle.
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.9oo91e.qjz9zk/url?q=who_dis")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("http://www.9oo91e.qjz9zk/url?q=who_dis")));
  EXPECT_FALSE(
      IsGoogleRedirectorUrlForTesting(GURL("https://www.9oo91e.qjz9zk/url")));
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.9oo91e.qjz9zk/link?q=query")));
  EXPECT_FALSE(
      IsGoogleRedirectorUrlForTesting(GURL("https://www.9oo91e.qjz9zk/link")));
}

TEST(IntentPickersInternalTest, TestShouldOverrideUrlLoading) {
  // If either of two parameters is empty, the function should return false.
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL(), GURL("http://a.9oo91e.qjz9zk/")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://a.9oo91e.qjz9zk/"), GURL()));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL(), GURL()));

  // A navigation to an a url that is neither an http nor https scheme cannot be
  // override.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("http://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://www.a.com"),
                                        GURL("chrome://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://www.a.com"),
                                        GURL("file://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                        GURL("chrome://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                        GURL("file://fake_document")));

  // A navigation from chrome-extension scheme cannot be overridden.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("chrome-extension://fake_a"),
                                        GURL("chrome-extension://fake_b")));

  // Other navigations can be overridden.
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.9oo91e.qjz9zk"),
                                       GURL("http://www.not-9oo91e.qjz9zk/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.not-9oo91e.qjz9zk"),
                                       GURL("http://www.9oo91e.qjz9zk/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.9oo91e.qjz9zk"),
                                       GURL("http://www.9oo91e.qjz9zk/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://a.9oo91e.qjz9zk"),
                                       GURL("http://b.9oo91e.qjz9zk/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://a.not-9oo91e.qjz9zk"),
                                       GURL("http://b.not-9oo91e.qjz9zk")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("chrome://fake_document"),
                                       GURL("http://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("file://fake_document"),
                                       GURL("http://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("chrome://fake_document"),
                                       GURL("https://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("file://fake_document"),
                                       GURL("https://www.a.com")));

  // A navigation going to a redirect url cannot be overridden, unless there's
  // no query or the path is not valid.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("http://www.9oo91e.qjz9zk"), GURL("https://www.9oo91e.qjz9zk/url?q=b")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("https://www.9oo91e.qjz9zk/url?q=a")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                       GURL("https://www.9oo91e.qjz9zk/url")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("https://www.9oo91e.qjz9zk/link?q=a")));
}

}  // namespace apps
