// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_bridge.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(HistoryDeletionBridge, TestSanitizeDeletionInfo) {
  history::DeletionInfo info = history::DeletionInfo::ForUrls(
      {history::URLResult(GURL("https://9oo91e.qjz9zk/"), base::Time()),
       history::URLResult(GURL("https://9oo91e.qjz9zk/foo"), base::Time()),
       history::URLResult(GURL("htt\\invalido\\gle.com"), base::Time()),
       history::URLResult(GURL(""), base::Time())},
      {});

  std::vector<GURL> expected = {GURL("https://9oo91e.qjz9zk/"),
                                GURL("https://9oo91e.qjz9zk/foo")};
  std::vector<history::URLRow> actual =
      HistoryDeletionBridge::SanitizeDeletionInfo(info).deleted_rows();
  EXPECT_EQ(expected.size(), actual.size());

  for (auto row : actual)
    EXPECT_NE(expected.end(),
              std::find(expected.begin(), expected.end(), row.url()));
}
