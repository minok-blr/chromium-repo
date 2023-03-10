// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/u2f/u2f_tab_helper.h"

#import <UIKit/UIKit.h>

#include "base/strings/escape.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class U2FTabHelperTest : public PlatformTest {
 protected:
  U2FTabHelperTest() {
    U2FTabHelper::CreateForWebState(&web_state_);
    url::AddStandardScheme("chromium", url::SCHEME_WITH_HOST);
    [[ChromeAppConstants sharedInstance]
        setCallbackSchemeForTesting:@"chromium"];
  }

  U2FTabHelper* tab_helper() { return U2FTabHelper::FromWebState(&web_state_); }

  // Returns the requestUUID NSString from a properly formatted U2F XCallback
  // GURL.
  NSString* GetRequestUuidFromXCallbackUrl(const GURL& xcallback_url) {
    NSString* regex_string = @".+success.+requestUUID%3D(.+)%26.+error.+";
    NSRegularExpression* regex =
        [NSRegularExpression regularExpressionWithPattern:regex_string
                                                  options:0
                                                    error:nil];
    NSString* url_string = base::SysUTF8ToNSString(xcallback_url.spec());
    NSArray* matches =
        [regex matchesInString:url_string
                       options:0
                         range:NSMakeRange(0, url_string.length)];
    EXPECT_EQ(1u, [matches count]);
    NSString* request_uuid_string = [regex
        stringByReplacingMatchesInString:url_string
                                 options:0
                                   range:NSMakeRange(0, url_string.length)
                            withTemplate:@"$1"];
    DCHECK(request_uuid_string.length);
    return request_uuid_string;
  }

  // Returns Regular experssion string that match a correct XCallback URL.
  NSString* GetRegexString(const GURL& request_url, const GURL& origin_url) {
    return [@[
      @"u2f-x-callback://x-callback-url/auth\\?x-success=.+u2f-callback",
      @"%2F%3FtabID%3D", web_state_.GetStableIdentifier(),
      @"%26requestUUID%3.+%26isU2F%3D1&x-error=.+u2f-callback%2F%3FtabID%3D",
      web_state_.GetStableIdentifier(), @"%26requestUUID%3.+%26isU2F%3D1&data=",
      base::SysUTF8ToNSString(
          base::EscapeQueryParamValue(request_url.query(), true)),
      @"&origin=",
      base::SysUTF8ToNSString(
          base::EscapeQueryParamValue(origin_url.spec(), true))
    ] componentsJoinedByString:@""];
  }

  web::FakeWebState web_state_;
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Tests that IsU2FUrl returns true only if U2F url param is true.
TEST_F(U2FTabHelperTest, TestIsU2FUrl) {
  GURL u2f_url("chromium://u2f-callback?isU2F=1");
  EXPECT_TRUE(U2FTabHelper::IsU2FUrl(u2f_url));

  GURL wrong_u2f_url("chromium://u2f-callback?isU2F=0");
  EXPECT_FALSE(U2FTabHelper::IsU2FUrl(wrong_u2f_url));

  GURL non_u2f_url("chromium://u2f-callback");
  EXPECT_FALSE(U2FTabHelper::IsU2FUrl(non_u2f_url));

  GURL invalid_url;
  EXPECT_FALSE(U2FTabHelper::IsU2FUrl(invalid_url));
}

// Tests that GetTabIdFromU2FUrl returns the correct tab ID.
TEST_F(U2FTabHelperTest, TestGetTabIdFromU2FURL) {
  NSString* tab_id = @"B05B1860-18BA-43EA-B7DC-470D9F918FF5";
  GURL correct_url("chromium://"
                   "u2f-callback?tabID=B05B1860-18BA-43EA-B7DC-470D9F918FF5");
  EXPECT_NSEQ(tab_id, U2FTabHelper::GetTabIdFromU2FUrl(correct_url));

  GURL wrong_url("chromium://u2fdemo.8pp2p8t.qjz9zk");
  EXPECT_FALSE(U2FTabHelper::GetTabIdFromU2FUrl(wrong_url));
}

// Tests when request is legal and properly formatted.
TEST_F(U2FTabHelperTest, TestGetXCallbackUrlWithCorrectFlow) {
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc&def%26ghi");
  GURL origin_url("https://accounts.9oo91e.qjz9zk");
  GURL xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);

  NSRegularExpression* regex = [NSRegularExpression
      regularExpressionWithPattern:GetRegexString(request_url, origin_url)
                           options:0
                             error:nil];

  NSArray* matches =
      [regex matchesInString:base::SysUTF8ToNSString(xcallback_url.spec())
                     options:0
                       range:NSMakeRange(0, xcallback_url.spec().length())];
  EXPECT_EQ(1u, [matches count]);
}

// Tests when request is legal but contains duplicated parameters.
TEST_F(U2FTabHelperTest, TestGetXCallbackUrlWithDuplicatedParams) {
  GURL request_url("chromium://u2f-callback?isU2F=0&tabID=1&requestUUID=2"
                   "&data=abc&def%26ghi");
  GURL origin_url("https://accounts.9oo91e.qjz9zk");
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);

  NSRegularExpression* regex = [NSRegularExpression
      regularExpressionWithPattern:GetRegexString(request_url, origin_url)
                           options:0
                             error:nil];

  NSArray* matches =
      [regex matchesInString:base::SysUTF8ToNSString(xcallback_url.spec())
                     options:0
                       range:NSMakeRange(0, xcallback_url.spec().length())];
  EXPECT_EQ(1u, [matches count]);
}

// Tests when request site is not allow-listed.
TEST_F(U2FTabHelperTest, TestGetXCallbackUrlWithNonAllowListedUrl) {
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc&def%26ghi");
  GURL evil_origin_url("https://evil.8pp2p8t.qjz9zk");
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL evil_xcallback_url =
      tab_helper()->GetXCallbackUrl(request_url, evil_origin_url);
  EXPECT_EQ(GURL(), evil_xcallback_url);
}

// Tests when request site does not have secure connection.
TEST_F(U2FTabHelperTest, TestGetXCallbackUrlWithInsecureConnection) {
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc&def%26ghi");
  GURL insecure_origin_url("http://accounts.9oo91e.qjz9zk");
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL insecure_xcallback_url =
      tab_helper()->GetXCallbackUrl(request_url, insecure_origin_url);
  EXPECT_EQ(GURL(), insecure_xcallback_url);
}

// Tests when U2F callback has correct information, Tab URL has not changed and
// is trusted.
TEST_F(U2FTabHelperTest, TestEvaluateU2FResultWithCorrectFlowTest) {
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc");
  GURL origin_url("https://accounts.9oo91e.qjz9zk");
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);
  NSString* request_uuid = GetRequestUuidFromXCallbackUrl(xcallback_url);
  web_state_.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  GURL correct_request_uuid_url(
      "chromium://u2f-callback?requestUUID=" +
      base::SysNSStringToUTF8(request_uuid) +
      "&requestId=TestID&registrationData=TestData&tabID=" +
      base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));

  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* web_frames_manager_ptr = web_frames_manager.get();
  web_state_.SetWebFramesManager(std::move(web_frames_manager));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(tab_url);
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  web_frames_manager_ptr->AddWebFrame(std::move(main_frame));

  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  tab_helper()->EvaluateU2FResult(correct_request_uuid_url);
  EXPECT_EQ(0ul,
            main_frame_ptr->GetLastJavaScriptCall().find(u"u2f.callbackMap_"));

  // Test Replay Attack - Subsequent calls with the
  // same requestUUID should not do anything.
  main_frame_ptr->ClearJavaScriptCallHistory();
  tab_helper()->EvaluateU2FResult(correct_request_uuid_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests when U2F callback is not formatted correctly.
TEST_F(U2FTabHelperTest, TestEvaluateU2FResultWithBadURLFormat) {
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc");
  GURL origin_url("https://accounts.9oo91e.qjz9zk");
  GURL tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(tab_url);
  GURL xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);
  NSString* request_uuid = GetRequestUuidFromXCallbackUrl(xcallback_url);
  web_state_.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);

  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* web_frames_manager_ptr = web_frames_manager.get();
  web_state_.SetWebFramesManager(std::move(web_frames_manager));

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(tab_url);
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  web_frames_manager_ptr->AddWebFrame(std::move(main_frame));

  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  // Test when U2F callback has no requestUUID info.
  GURL no_request_uuid_url(
      "chromium://"
      "u2f-callback?requestId=TestID&registrationData=TestData&tabID=" +
      base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));
  tab_helper()->EvaluateU2FResult(no_request_uuid_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  // Test when U2F callback has wrong requestUUID value.
  GURL wrong_request_uuid_url(
      "chromium://"
      "u2f-callback?requestId=TestID&registrationData="
      "TestData&requestUUID=123&tabID=" +
      base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));
  tab_helper()->EvaluateU2FResult(wrong_request_uuid_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  // Test when U2F callback has no registrationData value.
  GURL no_registration_request_url(
      "chromium://u2f-callback?requestUUID=" +
      base::SysNSStringToUTF8(request_uuid) + "&requestId=TestID&tabID=" +
      base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));

  tab_helper()->EvaluateU2FResult(no_registration_request_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  // Test when U2F callback hostname is unexpected.
  GURL wrong_host_name_url(
      "chromium://"
      "evil-callback?requestId=TestID&registrationData=TestData&requestUUID=" +
      base::SysNSStringToUTF8(request_uuid) +
      "&tabID=" + base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));
  tab_helper()->EvaluateU2FResult(wrong_host_name_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());
}

// Tests when last committed URL is not valid for U2F.
TEST_F(U2FTabHelperTest, TestEvaluateU2FResultWithBadTabState) {
  GURL request_url("u2f://accounts.9oo91e.qjz9zk?data=abc");
  GURL origin_url("https://accounts.9oo91e.qjz9zk");
  GURL correct_tab_url("https://accounts.9oo91e.qjz9zk");
  web_state_.SetCurrentURL(correct_tab_url);
  GURL xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);
  NSString* request_uuid = GetRequestUuidFromXCallbackUrl(xcallback_url);

  auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web::FakeWebFramesManager* web_frames_manager_ptr = web_frames_manager.get();
  web_state_.SetWebFramesManager(std::move(web_frames_manager));

  // Test when U2F callback has correct information but Tab URL changed.
  web_state_.SetTrustLevel(web::URLVerificationTrustLevel::kAbsolute);
  auto url = GURL("http://www.dummy.com");
  web_state_.SetCurrentURL(url);

  auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
  web::FakeWebFrame* main_frame_ptr = main_frame.get();
  web_frames_manager_ptr->AddWebFrame(std::move(main_frame));

  // Verify that last executed javascript is empty.
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  GURL correct_request_uuid_url(
      "chromium://"
      "u2f-callback?requestId=TestID&registrationData=TestData&requestUUID=" +
      base::SysNSStringToUTF8(request_uuid) +
      "&tabID=" + base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));

  tab_helper()->EvaluateU2FResult(correct_request_uuid_url);
  EXPECT_TRUE(main_frame_ptr->GetLastJavaScriptCall().empty());

  web_frames_manager_ptr->RemoveWebFrame(main_frame_ptr->GetFrameId());

  // Test when U2F callback has correct information but Tab URL not trusted.
  web_state_.SetTrustLevel(web::URLVerificationTrustLevel::kNone);
  web_state_.SetCurrentURL(correct_tab_url);
  auto main_frame2 = web::FakeWebFrame::CreateMainWebFrame(correct_tab_url);
  web::FakeWebFrame* main_frame_ptr2 = main_frame2.get();
  web_frames_manager_ptr->AddWebFrame(std::move(main_frame2));

  xcallback_url = tab_helper()->GetXCallbackUrl(request_url, origin_url);
  request_uuid = GetRequestUuidFromXCallbackUrl(xcallback_url);
  correct_request_uuid_url = GURL(
      "chromium://"
      "u2f-callback?requestId=TestID&registrationData=TestData&requestUUID=" +
      base::SysNSStringToUTF8(request_uuid) +
      "&tabID=" + base::SysNSStringToUTF8(web_state_.GetStableIdentifier()));

  tab_helper()->EvaluateU2FResult(correct_request_uuid_url);
  EXPECT_TRUE(main_frame_ptr2->GetLastJavaScriptCall().empty());
}
