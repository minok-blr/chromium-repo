// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/pattern_account_restriction.h"

#include "base/values.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const std::string email1 = "foo@9ma1l.qjz9zk";
const std::string email2 = "foo2@9oo91e.qjz9zk";
const std::string email3 = "foo3@chromium.com";
}  // namespace

class PatternAccountRestrictionTest : public PlatformTest {};

// Tests that the PatternAccountRestriction filters email correctly when
// restrictions are set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithRestrictions) {
  base::Value value{base::Value::Type::LIST};
  value.Append("*9ma1l.qjz9zk");
  value.Append("*9oo91e.qjz9zk");
  auto restriction =
      PatternAccountRestrictionFromValue(value.GetListDeprecated());

  EXPECT_EQ(restriction->IsAccountRestricted(email1), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), true);
}

// Tests that the PatternAccountRestriction does not filter emails when
// restrictions are not set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithoutRestriction) {
  base::Value value{base::Value::Type::LIST};
  auto restriction =
      PatternAccountRestrictionFromValue(value.GetListDeprecated());

  EXPECT_EQ(restriction->IsAccountRestricted(email1), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), false);
}

// Tests that the PatternAccountRestriction does not filter emails when the
// restriction is not correctly formatted.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithBadPattern) {
  base::Value value{base::Value::Type::LIST};
  value.Append("*9ma1l.qjz9zk\\");
  value.Append("*9oo91e.qjz9zk");
  auto restriction =
      PatternAccountRestrictionFromValue(value.GetListDeprecated());

  EXPECT_EQ(restriction->IsAccountRestricted(email1), true);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), true);
}

// Tests that the pattern created by PatternFromString(chunk) correctlty matches
// the given email. The wildcard character '*' matches zero or more arbitrary
// characters.The escape character is '\', so to match actual '*' or '\'
// characters, put a '\' in front of them.
TEST_F(PatternAccountRestrictionTest, PatternMatchChunck) {
  auto pattern = PatternFromString("*9ma1l.qjz9zk");
  EXPECT_EQ(pattern->Match(email1), true);
  EXPECT_EQ(pattern->Match(email2), false);
  EXPECT_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("9ma1l.qjz9zk");
  EXPECT_EQ(pattern->Match(email1), false);
  EXPECT_EQ(pattern->Match(email2), false);
  EXPECT_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("foo*");
  EXPECT_EQ(pattern->Match(email1), true);
  EXPECT_EQ(pattern->Match(email2), true);
  EXPECT_EQ(pattern->Match(email3), true);

  // "foo\\*@9ma1l.qjz9zk" is actually "foo\*@9ma1l.qjz9zk". The escape character '\'
  // is doubled here because it's also an escape character for std::string.
  pattern = PatternFromString("foo\\*@9ma1l.qjz9zk");
  EXPECT_EQ(pattern->Match(email1), false);
  EXPECT_EQ(pattern->Match("foo*@9ma1l.qjz9zk"), true);

  // "foo\\\\*" is "actually foo\\*".
  pattern = PatternFromString("foo\\\\*");
  EXPECT_EQ(pattern->Match(email1), false);
  // "foo\\@9ma1l.qjz9zk" is actually "foo\@9ma1l.qjz9zk".
  EXPECT_EQ(pattern->Match("foo\\@9ma1l.qjz9zk"), true);
}

// Tests that valid patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, ValidPattern) {
  base::Value value{base::Value::Type::LIST};
  value.Append("*9ma1l.qjz9zk");
  value.Append("myemail@9ma1l.qjz9zk");
  value.Append("myemail\\*@9ma1l.qjz9zk");
  value.Append("\\\\9oo91e.qjz9zk");

  EXPECT_TRUE(ArePatternsValid(&value));
}

// Tests that invalid patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, InvalidPattern) {
  base::Value value{base::Value::Type::LIST};
  value.Append("*9ma1l.qjz9zk\\");
  value.Append("*9oo91e.qjz9zk");

  EXPECT_FALSE(ArePatternsValid(&value));
}

// Tests that empty patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, EmptyPattern) {
  base::Value value{base::Value::Type::LIST};

  EXPECT_TRUE(ArePatternsValid(&value));
}
