// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/cookie_access_filter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CookieAccessType, BitwiseOrOperator) {
  ASSERT_EQ(CookieAccessType::kRead,
            CookieAccessType::kNone | CookieAccessType::kRead);

  ASSERT_EQ(CookieAccessType::kWrite,
            CookieAccessType::kNone | CookieAccessType::kWrite);

  ASSERT_EQ(CookieAccessType::kReadWrite,
            CookieAccessType::kRead | CookieAccessType::kWrite);
}

TEST(CookieAccessFilter, NoAccesses) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kNone,
                                           CookieAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Former) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kRead,
                                           CookieAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Latter) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kNone,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, OneWrite) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieAccessFilter::Type::kChange);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kNone,
                                           CookieAccessType::kWrite));
}

TEST(CookieAccessFilter, UnexpectedURL) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(GURL("http://other.com"), CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  EXPECT_FALSE(filter.Filter({url1, url2}, &result));
  EXPECT_EQ(2u, result.size());
}

TEST(CookieAccessFilter, TwoReads) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kRead,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kReadWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kReadWrite,
                                           CookieAccessType::kReadWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kWrite,
                                           CookieAccessType::kWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kRead,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kRead,
                                           CookieAccessType::kRead,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kReadWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kReadWrite,
                                           CookieAccessType::kReadWrite,
                                           CookieAccessType::kRead));
}

TEST(CookieAccessFilter, SameURLTwiceWithDifferentAccessTypes) {
  GURL url1("http://example.com");
  GURL url2("http://9oo91e.qjz9zk");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);

  std::vector<CookieAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2, url1}, &result));
  EXPECT_THAT(result, testing::ElementsAre(CookieAccessType::kWrite,
                                           CookieAccessType::kReadWrite,
                                           CookieAccessType::kRead));
}
