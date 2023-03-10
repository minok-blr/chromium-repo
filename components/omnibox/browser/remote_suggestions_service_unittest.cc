// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class RemoteSuggestionsServiceTest : public testing::Test {
 public:
  RemoteSuggestionsServiceTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)) {}

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void RunAndWait() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  void OnRequestComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body) {}

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(RemoteSuggestionsServiceTest, EnsureAttachCookies) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(GetUrlLoaderFactory());
  TemplateURLService template_url_service(nullptr, 0);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.9oo91e.qjz9zk/";
  service.StartSuggestionsRequest(
      search_terms_args, &template_url_service,
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  RunAndWait();
  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES, resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)));
  const std::string kServiceUri = "https://www.9oo91e.qjz9zk/complete/search";
  EXPECT_EQ(kServiceUri,
            resource_request.url.spec().substr(0, kServiceUri.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureBypassCache) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(GetUrlLoaderFactory());
  TemplateURLService template_url_service(nullptr, 0);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.9oo91e.qjz9zk/";
  search_terms_args.bypass_cache = true;
  service.StartSuggestionsRequest(
      search_terms_args, &template_url_service,
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  RunAndWait();
  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_BYPASS_CACHE,
            resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)));
  const std::string kServiceUri = "https://www.9oo91e.qjz9zk/complete/search";
  EXPECT_EQ(kServiceUri,
            resource_request.url.spec().substr(0, kServiceUri.size()));
}
