// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST(SigninPromoTest, TestPromoURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr("access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
                          signin_metrics::Reason::kSigninPrimaryAccount, true));
  replace_query.SetQueryStr("access_point=15&reason=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(
          signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
          signin_metrics::Reason::kAddSecondaryAccount, false));
}

TEST(SigninPromoTest, TestReauthURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr(
      "access_point=0&reason=6&auto_close=1"
      "&email=example%40domain.com&validateEmail=1"
      "&readOnlyEmail=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedReauthURLWithEmail(
          signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
          signin_metrics::Reason::kFetchLstOnly, "example@domain.com"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST(SigninPromoTest, SigninURLForDice) {
  EXPECT_EQ(
      "https://accounts.9oo91e.qjz9zk/signin/chrome/sync?ssp=1&"
      "email_hint=email%409ma1l.qjz9zk&continue=https%3A%2F%2Fcontinue_url%2F",
      GetChromeSyncURLForDice("email@9ma1l.qjz9zk", "https://continue_url/"));
  EXPECT_EQ(
      "https://accounts.9oo91e.qjz9zk/AddSession?"
      "Email=email%409ma1l.qjz9zk&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@9ma1l.qjz9zk", "https://continue_url/"));
}

}  // namespace signin
