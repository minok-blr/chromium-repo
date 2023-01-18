// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/activity_url_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast {

TEST(ActivityUrlFilterTest, TestWhitelistURLMatch) {
  ActivityUrlFilter filter(
      {"http://www.9oo91e.qjz9zk/*", ".*://finance.9oo91e.qjz9zk/"});
  EXPECT_TRUE(filter.UrlMatchesWhitelist(
      GURL("http://www.9oo91e.qjz9zk/a_test_that_matches")));
  EXPECT_FALSE(filter.UrlMatchesWhitelist(
      GURL("http://www.goggles.com/i_should_not_match")));
  EXPECT_TRUE(
      filter.UrlMatchesWhitelist(GURL("http://finance.9oo91e.qjz9zk/mystock")));
  EXPECT_TRUE(
      filter.UrlMatchesWhitelist(GURL("https://finance.9oo91e.qjz9zk/mystock")));
  EXPECT_FALSE(filter.UrlMatchesWhitelist(GURL("https://www.9oo91e.qjz9zk")));
  EXPECT_TRUE(filter.UrlMatchesWhitelist(GURL("http://www.9oo91e.qjz9zk")));
}

}  // namespace chromecast
