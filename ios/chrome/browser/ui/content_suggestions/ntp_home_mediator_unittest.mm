// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"

#include <memory>

#import "base/test/metrics/histogram_tester.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/voice/fake_voice_search_availability.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class NTPHomeMediatorTest : public PlatformTest {
 public:
  NTPHomeMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    NewTabPageTabHelper::CreateForWebState(fake_web_state_.get());
    logo_vendor_ = OCMProtocolMock(@protocol(LogoVendor));
    voice_availability_.SetVoiceProviderEnabled(true);

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    auth_service_ = static_cast<AuthenticationServiceFake*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(chrome_browser_state_.get());
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    mediator_ = [[NTPHomeMediator alloc]
               initWithWebState:fake_web_state_.get()
             templateURLService:ios::TemplateURLServiceFactory::
                                    GetForBrowserState(
                                        chrome_browser_state_.get())
                      URLLoader:url_loader_
                    authService:auth_service_
                identityManager:identity_manager_
          accountManagerService:accountManagerService
                     logoVendor:logo_vendor_
        voiceSearchAvailability:&voice_availability_];
    consumer_ = OCMProtocolMock(@protocol(NTPHomeConsumer));
    mediator_.consumer = consumer_;
    histogram_tester_.reset(new base::HistogramTester());
  }

  // Explicitly disconnect the mediator.
  ~NTPHomeMediatorTest() override { [mediator_ shutdown]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  id consumer_;
  id logo_vendor_;
  FakeVoiceSearchAvailability voice_availability_;
  NTPHomeMediator* mediator_;
  ToolbarTestNavigationManager* navigation_manager_;
  FakeUrlLoadingBrowserAgent* url_loader_;
  AuthenticationServiceFake* auth_service_;
  signin::IdentityManager* identity_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests that the consumer has the right value set up.
TEST_F(NTPHomeMediatorTest, TestConsumerSetup) {
  // Setup.
  OCMExpect([consumer_ setLogoVendor:logo_vendor_]);
  OCMExpect([consumer_ setLogoIsShowing:YES]);

  // Action.
  [mediator_ setUp];

  // Tests.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is focused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationFocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarBecomesFirstResponder]);

  // Action.
  [mediator_ locationBarDidBecomeFirstResponder];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is notified when the location bar is unfocused.
TEST_F(NTPHomeMediatorTest, TestConsumerNotificationUnfocus) {
  // Setup.
  [mediator_ setUp];

  OCMExpect([consumer_ locationBarResignsFirstResponder]);

  // Action.
  [mediator_ locationBarDidResignFirstResponder];

  // Test.
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the voice search button is disabled when VoiceOver is turned on
// and off.
TEST_F(NTPHomeMediatorTest, DisableVoiceSearch) {
  [mediator_ setUp];

  // Enable VoiceOver and verify that voice search is disabled for the consumer.
  OCMExpect([consumer_ setVoiceSearchIsEnabled:NO]);
  voice_availability_.SetVoiceOverEnabled(true);

  // Disable VoiceOVer and verify that voice search is enabled again.
  OCMExpect([consumer_ setVoiceSearchIsEnabled:YES]);
  voice_availability_.SetVoiceOverEnabled(false);

  EXPECT_OCMOCK_VERIFY(consumer_);
}
