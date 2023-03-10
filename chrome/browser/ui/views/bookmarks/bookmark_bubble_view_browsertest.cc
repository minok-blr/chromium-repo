// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkBubbleViewBrowserTest() {}

  BookmarkBubbleViewBrowserTest(const BookmarkBubbleViewBrowserTest&) = delete;
  BookmarkBubbleViewBrowserTest& operator=(
      const BookmarkBubbleViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());

    signin::ConsentLevel consent_level = (name == "bookmark_details_synced_off")
                                             ? signin::ConsentLevel::kSignin
                                             : signin::ConsentLevel::kSync;
    constexpr char kTestUserEmail[] = "testuser@gtest.com";
    signin::MakePrimaryAccountAvailable(identity_manager, kTestUserEmail,
                                        consent_level);
#endif

    const GURL url = GURL("https://www.9oo91e.qjz9zk");
    const std::u16string title = u"Title";
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url, title);
    browser()->window()->ShowBookmarkBubble(url, true);

    if (name == "ios_promotion")
      BookmarkBubbleView::bookmark_bubble()->AcceptDialog();
  }
};

// Ash always has sync ON
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_synced_off) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_synced_on) {
  ShowAndVerifyUi();
}
