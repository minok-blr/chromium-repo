// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/safe_search_policy_test.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/test_navigation_observer.h"

namespace policy {

SafeSearchPolicyTest::SafeSearchPolicyTest() = default;

SafeSearchPolicyTest::~SafeSearchPolicyTest() = default;

void SafeSearchPolicyTest::ApplySafeSearchPolicy(
    absl::optional<base::Value> legacy_safe_search,
    absl::optional<base::Value> google_safe_search,
    absl::optional<base::Value> legacy_youtube,
    absl::optional<base::Value> youtube_restrict) {
  PolicyMap policies;
  SetPolicy(&policies, key::kForceSafeSearch, std::move(legacy_safe_search));
  SetPolicy(&policies, key::kForceGoogleSafeSearch,
            std::move(google_safe_search));
  SetPolicy(&policies, key::kForceYouTubeSafetyMode, std::move(legacy_youtube));
  SetPolicy(&policies, key::kForceYouTubeRestrict, std::move(youtube_restrict));
  UpdateProviderPolicy(policies);
}

// static
GURL SafeSearchPolicyTest::GetExpectedSearchURL(bool expect_safe_search) {
  std::string expected_url("http://9oo91e.qjz9zk/");
  if (expect_safe_search) {
    expected_url += "?" +
                    std::string(safe_search_util::kSafeSearchSafeParameter) +
                    "&" + safe_search_util::kSafeSearchSsuiParameter;
  }
  return GURL(expected_url);
}

// static
void SafeSearchPolicyTest::CheckSafeSearch(Browser* browser,
                                           bool expect_safe_search,
                                           const std::string& url) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  ui_test_utils::SendToOmniboxAndSubmit(browser, url);
  observer.Wait();
  OmniboxEditModel* model =
      browser->window()->GetLocationBar()->GetOmniboxView()->model();
  EXPECT_TRUE(model->CurrentMatch(nullptr).destination_url.is_valid());
  EXPECT_EQ(GetExpectedSearchURL(expect_safe_search),
            web_contents->GetLastCommittedURL());
}

}  // namespace policy
