// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_filter.h"

#include "components/optimization_guide/core/bloom_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

std::unique_ptr<BloomFilter> CreateBloomFilter() {
  std::unique_ptr<BloomFilter> filter = std::make_unique<BloomFilter>(
      7 /* num_hash_functions */, 8191 /* num_bits */);
  return filter;
}

std::unique_ptr<RegexpList> CreateRegexps(
    const std::vector<std::string>& regexps) {
  std::unique_ptr<RegexpList> regexp_list = std::make_unique<RegexpList>();
  for (const std::string& regexp : regexps) {
    regexp_list->emplace_back(std::make_unique<re2::RE2>(regexp));
  }
  return regexp_list;
}

TEST(OptimizationFilterTest, TestMatchesBloomFilter) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("fooco.co.uk");
  OptimizationFilter opt_filter(std::move(bloom_filter), /*regexps=*/nullptr,
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  EXPECT_TRUE(opt_filter.Matches(GURL("http://shopping.fooco.co.uk")));
  EXPECT_TRUE(
      opt_filter.Matches(GURL("https://shopping.fooco.co.uk/somepath")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://fooco.co.uk")));
  EXPECT_FALSE(opt_filter.Matches(GURL("https://nonfooco.co.uk")));
}

TEST(OptimizationFilterTest, TestMatchesBloomFilterChecksRegexpFirst) {
  std::unique_ptr<RegexpList> exclusion_regexps(CreateRegexps({"shopping"}));
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("9oo91e.qjz9zk");
  OptimizationFilter opt_filter(std::move(bloom_filter), /*regexps=*/nullptr,
                                std::move(exclusion_regexps),
                                /*skip_host_suffix_checking=*/false);
  EXPECT_FALSE(opt_filter.Matches(GURL("http://shopping.9oo91e.qjz9zk")));
  EXPECT_TRUE(opt_filter.Matches(GURL("http://www.9oo91e.qjz9zk")));
}

TEST(OptimizationFilterTest, TestMatchesBloomFilterSkipHostSuffixChecking) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("fooco.co.uk");
  OptimizationFilter opt_filter(std::move(bloom_filter), /*regexps=*/nullptr,
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/true);
  EXPECT_TRUE(opt_filter.Matches(GURL("https://fooco.co.uk/somepath")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://fooco.co.uk")));
  EXPECT_FALSE(opt_filter.Matches(GURL("http://shopping.fooco.co.uk")));
  EXPECT_FALSE(opt_filter.Matches(GURL("https://nonfooco.co.uk")));
}

TEST(OptimizationFilterTest, TestMatchesRegexp) {
  std::unique_ptr<RegexpList> regexps(CreateRegexps({"test"}));
  OptimizationFilter opt_filter(/*bloom_filter=*/nullptr, std::move(regexps),
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  EXPECT_TRUE(opt_filter.Matches(GURL("http://test.com")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://shopping.com/test")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://shopping.com/?query=test")));
  EXPECT_FALSE(opt_filter.Matches(GURL("https://shopping.com/")));
}

TEST(OptimizationFilterTest, TestMatchesRegexpFragment) {
  std::unique_ptr<RegexpList> regexps(CreateRegexps({"test"}));
  OptimizationFilter opt_filter(/*bloom_filter=*/nullptr, std::move(regexps),
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  // Fragments are not matched.
  EXPECT_FALSE(opt_filter.Matches(GURL("https://shopping.com/#test")));
}

TEST(OptimizationFilterTest, TestMatchesRegexpInvalid) {
  std::unique_ptr<RegexpList> regexps(CreateRegexps({"test[", "shop"}));
  OptimizationFilter opt_filter(/*bloom_filter=*/nullptr, std::move(regexps),
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  // Invalid regexps are not used
  EXPECT_FALSE(opt_filter.Matches(GURL("https://test.com/")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://shopping.com/")));
}

TEST(OptimizationFilterTest, TestMatchesRegexpInvalidGURL) {
  std::unique_ptr<RegexpList> regexps(CreateRegexps({"test"}));
  OptimizationFilter opt_filter(/*bloom_filter=*/nullptr, std::move(regexps),
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  // Invalid urls are not matched.
  EXPECT_FALSE(opt_filter.Matches(GURL("test")));
}

TEST(OptimizationFilterTest, TestMatchesMaxSuffix) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("one.two.three.four.co.uk");
  bloom_filter->Add("one.two.three.four.five.co.uk");
  OptimizationFilter opt_filter(std::move(bloom_filter), /*regexps=*/nullptr,
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  EXPECT_TRUE(opt_filter.Matches(GURL("http://host.one.two.three.four.co.uk")));
  EXPECT_FALSE(
      opt_filter.Matches(GURL("http://host.one.two.three.four.five.co.uk")));

  // Note: full host will match even if more than 5 elements.
  EXPECT_TRUE(opt_filter.Matches(GURL("http://one.two.three.four.five.co.uk")));
}

TEST(OptimizationFilterTest, TestMatchesMinSuffix) {
  std::unique_ptr<BloomFilter> bloom_filter(CreateBloomFilter());
  bloom_filter->Add("abc.tv");
  bloom_filter->Add("xy.tv");
  OptimizationFilter opt_filter(std::move(bloom_filter), /*regexps=*/nullptr,
                                /*exclusion_regexps=*/nullptr,
                                /*skip_host_suffix_checking=*/false);
  EXPECT_TRUE(opt_filter.Matches(GURL("https://abc.tv")));
  EXPECT_TRUE(opt_filter.Matches(GURL("https://host.abc.tv")));
  EXPECT_FALSE(opt_filter.Matches(GURL("https://host.xy.tv")));

  // Note: full host will match even if less than min size.
  EXPECT_TRUE(opt_filter.Matches(GURL("https://xy.tv")));

  EXPECT_FALSE(opt_filter.Matches(GURL("http://.../foo")));
}

}  // namespace

}  // namespace optimization_guide
