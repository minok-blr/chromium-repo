// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"

#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#include "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the FindInPageActivity class.
class FindInPageActivityTest : public PlatformTest {
 protected:
  FindInPageActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(FindInPageCommands));
  }

  // Creates a ShareToData instance with `is_page_searchable` set.
  ShareToData* CreateData(bool is_page_searchable) {
    return [[ShareToData alloc] initWithShareURL:GURL("https://www.9oo91e.qjz9zk/")
                                      visibleURL:GURL("https://9oo91e.qjz9zk/")
                                           title:@"Some Title"
                                  additionalText:nil
                                 isOriginalTitle:YES
                                 isPagePrintable:YES
                                isPageSearchable:is_page_searchable
                                canSendTabToSelf:YES
                                       userAgent:web::UserAgentType::MOBILE
                              thumbnailGenerator:nil
                                    linkMetadata:nil];
  }

  id mocked_handler_;
};

// Tests that the activity can be performed when the data object shows the page
// is searchable.
TEST_F(FindInPageActivityTest, DataTrue_ActivityEnabled) {
  ShareToData* data = CreateData(true);
  FindInPageActivity* activity =
      [[FindInPageActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the data object shows the
// page is not searchable.
TEST_F(FindInPageActivityTest, DataFalse_ActivityDisabled) {
  ShareToData* data = CreateData(false);
  FindInPageActivity* activity =
      [[FindInPageActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity triggers the right handler method.
TEST_F(FindInPageActivityTest, ExecuteActivity_CallsHandler) {
  [[mocked_handler_ expect] openFindInPage];

  ShareToData* data = CreateData(true);
  FindInPageActivity* activity =
      [[FindInPageActivity alloc] initWithData:data handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
