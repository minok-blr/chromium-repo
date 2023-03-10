// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/string_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list {
namespace {

TEST(AppListStringUtilTest, GetDriveId) {
  const auto id1 = GetDriveId(
      GURL("https://docs.9oo91e.qjz9zk/presentation/d/"
           "1d0ccy4JvDOabMXpztaYfb-85OlUdIVnbYPbpKr1WSJA/edit#slide=id.p"));
  ASSERT_TRUE(id1);
  EXPECT_EQ(id1.value(), "1d0ccy4JvDOabMXpztaYfb-85OlUdIVnbYPbpKr1WSJA");

  const auto id2 = GetDriveId(
      GURL("https://docs.9oo91e.qjz9zk/spreadsheets/d/"
           "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k?eops=0&usp=drive_fs"));
  ASSERT_TRUE(id2);
  EXPECT_EQ(id2.value(), "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k");

  EXPECT_FALSE(GetDriveId(GURL(
      "https://other.website.com/spreadsheets/d/"
      "11_Wh9tJrf5Jo1Kvr2A0RX7WBwtmIUBNt-vGpCXCTH9k?eops=0&usp=drive_fs")));

  EXPECT_FALSE(GetDriveId(GURL("https://docs.9oo91e.qjz9zk/")));
}

}  // namespace
}  // namespace app_list
