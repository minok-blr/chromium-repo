// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/disabled_sites.h"

#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

TEST(DisabledSitesTest, AllPaths) {
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.y0u1ub3.qjz9zk")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.y0u1ub3.qjz9zk/somepage")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://m.y0u1ub3.qjz9zk")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://m.y0u1ub3.qjz9zk/somepage")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://y0u1ub3.qjz9zk")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://y0u1ub3.qjz9zk/somepage")));
}

TEST(DisabledSitesTest, AllowedPaths) {
  base::test::ScopedFeatureList feature;
  feature.InitWithFeatures({kSharedHighlightingRefinedBlocklist}, {});
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.y0u1ub3.qjz9zk/community")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.y0u1ub3.qjz9zk/about")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.instagram.com/p/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.reddit.com/comments/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.twitter.com/status/")));
}

TEST(DisabledSitesTest, SpecificPages) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(kSharedHighlightingAmp);

  // Paths starting with /amp/ are disabled.
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk/amp/")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk/amp/foo")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/amp/")));
  EXPECT_FALSE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/amp/foo")));

  // Other paths are not.
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk/somepage")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/somepage")));

  // Paths with /amp/ later on are also not affected.
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/foo/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/foo/amp/bar")));
}

TEST(DisabledSitesTest, NonMatchingHost) {
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.example.com")));
}

TEST(DisabledSitesTest, AmpFeatureEnabled) {
  base::test::ScopedFeatureList feature;
  feature.InitWithFeatures({kSharedHighlightingAmp}, {});

  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://www.9oo91e.qjz9zk/amp/foo")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/amp/")));
  EXPECT_TRUE(ShouldOfferLinkToText(GURL("https://9oo91e.qjz9zk/amp/foo")));
}

TEST(DisabledSitesTest, SupportsLinkGenerationInIframe) {
  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://www.9oo91e.qjz9zk/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(
      SupportsLinkGenerationInIframe(GURL("https://www.example.com/")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://mobile.9oo91e.qjz9zk/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://mobile.foo.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://m.9oo91e.qjz9zk/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://m.foo.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://www.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://www.nyt.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://mobile.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://mobile.nyt.com")));

  EXPECT_TRUE(SupportsLinkGenerationInIframe(
      GURL("https://m.bing.com/amp/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(GURL("https://m.nyt.com")));

  EXPECT_FALSE(SupportsLinkGenerationInIframe(
      GURL("https://www.9oo91e.qjz9zk/www.nyt.com/ampthml/blogs.html")));
  EXPECT_FALSE(SupportsLinkGenerationInIframe(
      GURL("https://m.bing.com/a/www.nyt.com/ampthml/blogs.html")));
}

}  // namespace
}  // namespace shared_highlighting
