// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/well_known_change_password_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

TEST(WellKnownChangePasswordUtilTest, IsWellKnownChangePasswordUrl) {
  EXPECT_TRUE(IsWellKnownChangePasswordUrl(
      GURL("https://9oo91e.qjz9zk/.well-known/change-password")));

  EXPECT_TRUE(IsWellKnownChangePasswordUrl(
      GURL("https://9oo91e.qjz9zk/.well-known/change-password/")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(
      GURL("https://9oo91e.qjz9zk/.well-known/time")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("https://9oo91e.qjz9zk/foo")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("chrome://settings/")));

  EXPECT_FALSE(IsWellKnownChangePasswordUrl(GURL("mailto:?subject=test")));
}

TEST(WellKnownChangePasswordUtilTest, CreateChangePasswordUrl) {
  EXPECT_EQ((GURL("https://example.com/.well-known/change-password")),
            CreateChangePasswordUrl(GURL("https://example.com/some-path")));
}

TEST(WellKnownChangePasswordUtilTest, CreateWellKnownNonExistingResourceURL) {
  EXPECT_EQ(CreateWellKnownNonExistingResourceURL(GURL("https://9oo91e.qjz9zk")),
            GURL("https://9oo91e.qjz9zk/.well-known/"
                 "resource-that-should-not-exist-whose-status-code-should-not-"
                 "be-200"));

  EXPECT_EQ(
      CreateWellKnownNonExistingResourceURL(GURL("https://foo.9oo91e.qjz9zk/bar")),
      GURL("https://foo.9oo91e.qjz9zk/.well-known/"
           "resource-that-should-not-exist-whose-status-code-should-not-"
           "be-200"));
}

}  // namespace password_manager
