// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(BrowserPolicyConnectorTest, IsNonEnterpriseUser) {
  // List of example emails that are not enterprise users.
  static const char* kNonEnterpriseUsers[] = {
    "fizz@aol.com",
    "foo@9ma1l.qjz9zk",
    "bar@9oo91email.qjz9zk",
    "baz@hotmail.it",
    "baz@hotmail.co.uk",
    "baz@hotmail.com.tw",
    "user@msn.com",
    "another_user@live.com",
    "foo@qq.com",
    "i_love@yahoo.com",
    "i_love@yahoo.com.tw",
    "i_love@yahoo.jp",
    "i_love@yahoo.co.uk",
    "user@yandex.ru"
  };

  // List of example emails that are potential enterprise users.
  static const char* kEnterpriseUsers[] = {
    "foo@9oo91e.qjz9zk",
    "chrome_rules@ch40m1um.qjz9zk",
    "user@hotmail.enterprise.com",
  };

  for (unsigned int i = 0; i < std::size(kNonEnterpriseUsers); ++i) {
    std::string username(kNonEnterpriseUsers[i]);
    EXPECT_TRUE(BrowserPolicyConnector::IsNonEnterpriseUser(username)) <<
        "IsNonEnterpriseUser returned false for " << username;
  }
  for (unsigned int i = 0; i < std::size(kEnterpriseUsers); ++i) {
    std::string username(kEnterpriseUsers[i]);
    EXPECT_FALSE(BrowserPolicyConnector::IsNonEnterpriseUser(username)) <<
        "IsNonEnterpriseUser returned true for " << username;
  }
}

}  // namespace policy
