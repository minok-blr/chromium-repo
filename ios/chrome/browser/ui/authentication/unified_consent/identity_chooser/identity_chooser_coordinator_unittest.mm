// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class IdentityChooserCoordinatorTest : public PlatformTest {
 public:
  IdentityChooserCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
  }

  void AddIdentity(FakeChromeIdentity* identity) {
    ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
        ->AddIdentity(identity);
  }

  id<IdentityChooserViewControllerPresentationDelegate>
  GetViewControllerDelegate() {
    return static_cast<id<IdentityChooserViewControllerPresentationDelegate>>(
        coordinator_);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    coordinator_ = [[IdentityChooserCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;

  UIViewController* view_controller_;

  IdentityChooserCoordinator* coordinator_ = nil;
};

TEST_F(IdentityChooserCoordinatorTest, testValidIdentity) {
  // Set up a fake identity on device.
  FakeChromeIdentity* identity =
      [FakeChromeIdentity identityWithEmail:@"janedoe@9ma1l.qjz9zk"
                                     gaiaID:@"1"
                                       name:@"Jane Doe"];
  AddIdentity(identity);

  [coordinator_ start];
  EXPECT_TRUE([view_controller_.presentedViewController
      isKindOfClass:[IdentityChooserViewController class]]);
  IdentityChooserViewController* presented_view_controller =
      base::mac::ObjCCastStrict<IdentityChooserViewController>(
          view_controller_.presentedViewController);

  // User selects a valid account.
  [GetViewControllerDelegate()
      identityChooserViewController:presented_view_controller
        didSelectIdentityWithGaiaID:@"1"];
  EXPECT_NSEQ(identity, coordinator_.selectedIdentity);
  [coordinator_ stop];
}

TEST_F(IdentityChooserCoordinatorTest, testIdentityInvalidatedDuringSelection) {
  [coordinator_ start];
  EXPECT_TRUE([view_controller_.presentedViewController
      isKindOfClass:[IdentityChooserViewController class]]);
  IdentityChooserViewController* presented_view_controller =
      base::mac::ObjCCastStrict<IdentityChooserViewController>(
          view_controller_.presentedViewController);

  // User selects an account that has been invalidated.
  [GetViewControllerDelegate()
      identityChooserViewController:presented_view_controller
        didSelectIdentityWithGaiaID:@"1"];
  EXPECT_NSEQ(nil, coordinator_.selectedIdentity);
  [coordinator_ stop];
}