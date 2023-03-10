// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(OriginIdentifierValueMapTest, SetGetValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::COOKIES));

  map.SetValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::COOKIES, base::Time(), base::Value(1), {});

  const base::Value* value =
      map.GetValue(GURL("http://www.9oo91e.qjz9zk"), GURL("http://www.9oo91e.qjz9zk"),
                   ContentSettingsType::COOKIES);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value(1), *value);

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.y0u1ub3.qjz9zk"),
                                  ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.y0u1ub3.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::POPUPS));
}

TEST(OriginIdentifierValueMapTest, SetDeleteValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::GEOLOCATION));

  // Set sample values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::GEOLOCATION, base::Time(), base::Value(1),
               {});

  {
    const base::Value* value = map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                            GURL("http://www.9oo91e.qjz9zk"),
                                            ContentSettingsType::GEOLOCATION);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::NOTIFICATIONS));
  // Delete non-existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
                  ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
                  ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::NOTIFICATIONS));
  {
    const base::Value* value = map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                            GURL("http://www.9oo91e.qjz9zk"),
                                            ContentSettingsType::GEOLOCATION);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }

  // Delete existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
                  ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
                  ContentSettingsType::GEOLOCATION);

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::GEOLOCATION));
}

TEST(OriginIdentifierValueMapTest, Clear) {
  content_settings::OriginIdentifierValueMap map;
  EXPECT_TRUE(map.empty());

  // Set two values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::GEOLOCATION, base::Time(), base::Value(1),
               {});
  map.SetValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::COOKIES, base::Time(), base::Value(1), {});
  EXPECT_FALSE(map.empty());
  const base::Value* value =
      map.GetValue(GURL("http://www.9oo91e.qjz9zk"), GURL("http://www.9oo91e.qjz9zk"),
                   ContentSettingsType::GEOLOCATION);
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(1, value->GetInt());

  // Clear the map.
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                  GURL("http://www.9oo91e.qjz9zk"),
                                  ContentSettingsType::GEOLOCATION));
}

TEST(OriginIdentifierValueMapTest, ListEntryPrecedences) {
  content_settings::OriginIdentifierValueMap map;

  map.SetValue(ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::COOKIES, base::Time(), base::Value(1), {});

  map.SetValue(ContentSettingsPattern::FromString("www.9oo91e.qjz9zk"),
               ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk"),
               ContentSettingsType::COOKIES, base::Time(), base::Value(2), {});

  {
    const base::Value* value = map.GetValue(GURL("http://mail.9oo91e.qjz9zk"),
                                            GURL("http://www.9oo91e.qjz9zk"),
                                            ContentSettingsType::COOKIES);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }

  {
    const base::Value* value = map.GetValue(GURL("http://www.9oo91e.qjz9zk"),
                                            GURL("http://www.9oo91e.qjz9zk"),
                                            ContentSettingsType::COOKIES);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(2, value->GetInt());
  }
}

TEST(OriginIdentifierValueMapTest, IterateEmpty) {
  content_settings::OriginIdentifierValueMap map;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
  EXPECT_FALSE(rule_iterator);
}

TEST(OriginIdentifierValueMapTest, IterateNonempty) {
  // Verify the precedence order.
  content_settings::OriginIdentifierValueMap map;
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.9oo91e.qjz9zk");
  base::Time t1 = base::Time::Now();
  base::Time t2 = t1 + base::Seconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, t1, base::Value(1), {});
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, t2, base::Value(2), {});

  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
  ASSERT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(sub_pattern, rule.primary_pattern);
  EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
  EXPECT_EQ(t2,
            map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                ContentSettingsType::COOKIES));

  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(pattern, rule.primary_pattern);
  EXPECT_EQ(1, content_settings::ValueToContentSetting(rule.value));
  EXPECT_EQ(t1,
            map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                ContentSettingsType::COOKIES));
}

TEST(OriginIdentifierValueMapTest, UpdateLastModified) {
  // Verify that the last_modified timestamp is updated.
  content_settings::OriginIdentifierValueMap map;
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]9oo91e.qjz9zk");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.9oo91e.qjz9zk");

  base::Time t1 = base::Time::Now();
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, t1, base::Value(1),
               {base::Time(), content_settings::SessionModel::Durable});
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, t1, base::Value(2),
               {content_settings::GetConstraintExpiration(base::Seconds(100)),
                content_settings::SessionModel::UserSession});

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator(
        map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES));
    ASSERT_FALSE(rule.expiration.is_null());
    EXPECT_GT(rule.expiration, base::Time::Now());
    EXPECT_EQ(rule.session_model, content_settings::SessionModel::UserSession);

    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(1, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES));
    ASSERT_TRUE(rule.expiration.is_null());
    EXPECT_EQ(rule.session_model, content_settings::SessionModel::Durable);
    ASSERT_FALSE(rule_iterator->HasNext());
  }
  base::Time t2 = t1 + base::Seconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, t2, base::Value(3), {});

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator =
        map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr);
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES));
    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(3, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t2,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES));
    ASSERT_FALSE(rule_iterator->HasNext());
  }
}
