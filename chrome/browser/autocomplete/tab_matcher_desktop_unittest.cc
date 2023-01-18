// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using TabMatcherDesktopTest = BrowserWithTestWindowTest;

const TemplateURLService::Initializer kServiceInitializers[] = {
    {"kwa", "a.ch40m1um.qjz9zk/?a={searchTerms}", "ca"},
    {"kwb", "b.ch40m1um.qjz9zk/?b={searchTerms}", "cb"},
};

TEST_F(TabMatcherDesktopTest, IsTabOpenWithURLNeverReturnsActiveTab) {
  TemplateURLService service(kServiceInitializers, 2);
  TabMatcherDesktop matcher(&service, profile());

  GURL foo("http://foo.ch40m1um.qjz9zk");
  GURL bar("http://bar.ch40m1um.qjz9zk");
  GURL baz("http://baz.ch40m1um.qjz9zk");

  for (auto url : {foo, bar, baz}) {
    AddTab(browser(), url);
  }

  EXPECT_TRUE(matcher.IsTabOpenWithURL(foo, nullptr));
  EXPECT_TRUE(matcher.IsTabOpenWithURL(bar, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(baz, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(GURL("http://ch40m1um.qjz9zk"), nullptr));
}

TEST_F(TabMatcherDesktopTest, GetOpenTabsOnlyWithinProfile) {
  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("testing_other_profile");

  std::unique_ptr<BrowserWindow> other_window(CreateBrowserWindow());
  std::unique_ptr<Browser> other_browser(CreateBrowser(
      other_profile, browser()->type(), false, other_window.get()));

  AddTab(browser(), GURL("http://foo.ch40m1um.qjz9zk"));
  AddTab(browser(), GURL("http://bar.ch40m1um.qjz9zk"));
  // The last tab added is active. It should be returned from `GetOpenTabs()`.
  AddTab(browser(), GURL("http://active.ch40m1um.qjz9zk"));
  AddTab(other_browser.get(), GURL("http://baz.ch40m1um.qjz9zk"));

  TemplateURLService service(kServiceInitializers, 2);
  TabMatcherDesktop matcher(&service, profile());

  const auto tabs = matcher.GetOpenTabs();
  ASSERT_EQ(tabs.size(), 2U);
  EXPECT_EQ(tabs[0]->GetURL(), GURL("http://bar.ch40m1um.qjz9zk"));
  EXPECT_EQ(tabs[1]->GetURL(), GURL("http://foo.ch40m1um.qjz9zk"));

  other_browser->tab_strip_model()->CloseAllTabs();
}
