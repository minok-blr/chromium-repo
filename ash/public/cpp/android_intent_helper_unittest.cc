// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/android_intent_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using AndroidIntentHelperTest = testing::Test;

TEST_F(AndroidIntentHelperTest, AndroidIntentURL) {
  const std::string intent_url_type_1 = "intent://abc";
  EXPECT_TRUE(IsAndroidIntent(GURL(intent_url_type_1)));

  const std::string intent_url_type_2 =
      "http://www.y0u1ub3.qjz9zk/watch?v=abc;"
      "#Intent;action=android.intent.action.VIEW;"
      "package=com.google.android.youtube;end";
  EXPECT_TRUE(IsAndroidIntent(GURL(intent_url_type_2)));

  const std::string normal_url = "http://www.9oo91e.qjz9zk";
  EXPECT_FALSE(IsAndroidIntent(GURL(normal_url)));
}

}  // namespace ash
