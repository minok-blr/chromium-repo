// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_logger.h"

#include <string>

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Return;

constexpr char kSiteOrigin[] = "https://example.com";
constexpr char kFederationOrigin[] = "https://9oo91e.qjz9zk";

class MockLogManager : public autofill::StubLogManager {
 public:
  MockLogManager() = default;
  MockLogManager(const MockLogManager&) = delete;
  MockLogManager& operator=(const MockLogManager&) = delete;
  ~MockLogManager() override = default;

  MOCK_CONST_METHOD1(LogTextMessage, void(const std::string& text));
};

class CredentialManagerLoggerTest : public testing::Test {
 public:
  CredentialManagerLoggerTest();
  CredentialManagerLoggerTest(const CredentialManagerLoggerTest&) = delete;
  CredentialManagerLoggerTest& operator=(const CredentialManagerLoggerTest&) =
      delete;
  ~CredentialManagerLoggerTest() override;

  MockLogManager& log_manager() { return log_manager_; }
  CredentialManagerLogger& logger() { return logger_; }

 private:
  MockLogManager log_manager_;
  CredentialManagerLogger logger_;
};

CredentialManagerLoggerTest::CredentialManagerLoggerTest()
    : logger_(&log_manager_) {}

CredentialManagerLoggerTest::~CredentialManagerLoggerTest() = default;

TEST_F(CredentialManagerLoggerTest, LogRequestCredential) {
  EXPECT_CALL(log_manager(),
              LogTextMessage(
                  AllOf(HasSubstr(kSiteOrigin), HasSubstr(kFederationOrigin))));
  logger().LogRequestCredential(url::Origin::Create(GURL(kSiteOrigin)),
                                CredentialMediationRequirement::kSilent,
                                {GURL(kFederationOrigin)});
}

TEST_F(CredentialManagerLoggerTest, LogSendCredential) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogSendCredential(url::Origin::Create(GURL(kSiteOrigin)),
                             CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogStoreCredential) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogStoreCredential(url::Origin::Create(GURL(kSiteOrigin)),
                              CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerLoggerTest, LogPreventSilentAccess) {
  EXPECT_CALL(log_manager(), LogTextMessage(HasSubstr(kSiteOrigin)));
  logger().LogPreventSilentAccess(url::Origin::Create(GURL(kSiteOrigin)));
}

}  // namespace
}  // namespace password_manager
