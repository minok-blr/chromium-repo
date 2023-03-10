// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/commerce/core/subscriptions/subscriptions_storage.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const int64_t kMockTimestamp = 123456;
const std::string kMockId1 = "111";
const std::string kMockId2 = "222";
const std::string kMockOfferId = "333";
const long kMockPrice = 100;
const std::string kMockCountry = "us";

const char kGetHttpMethod[] = "GET";
const char kPostHttpMethod[] = "POST";
const char kEmptyPostData[] = "";
const char kServiceUrl[] =
    "https://memex-pa.9oo91eapis.qjz9zk/v1/shopping/subscriptions";
const char kServiceUrlForGet[] =
    "https://memex-pa.9oo91eapis.qjz9zk/v1/shopping/subscriptions"
    "?requestParams.subscriptionType=PRICE_TRACK";

const std::string kExpectedPostDataForCreate =
    "{\"createShoppingSubscriptionsParams\":{\"subscriptions\":[{"
    "\"identifier\":\"111\",\"identifierType\":2,\"managementType\":2,\"type\":"
    "1},{\"identifier\":\"222\",\"identifierType\":2,\"managementType\":2,"
    "\"type\":1,\"userSeenOffer\":{\"countryCode\":\"us\",\"offerId\":\"333\","
    "\"seenPriceMicros\":\"100\"}}]}}";
const std::string kExpectedPostDataForDelete =
    "{\"removeShoppingSubscriptionsParams\":{\"eventTimestampMicros\":["
    "\"123456\"]}}";
const std::string kResponseSucceeded = "{ \"status\": { \"code\": 0 } }";
const std::string kResponseFailed = "{ \"status\": { \"code\": 1 } }";
const std::string kValidGetResponse =
    "{\"subscriptions\":[{"
    "\"identifier\":\"111\",\"identifierType\":2,\"managementType\":2,\"type\":"
    "1,\"eventTimestampMicros\":\"123456\"}]}";

// Build a subscription list consisting of two subscriptions.
std::unique_ptr<std::vector<commerce::CommerceSubscription>>
BuildValidSubscriptions() {
  auto subscriptions =
      std::make_unique<std::vector<commerce::CommerceSubscription>>();
  // The first one has a valid timestamp but doesn't contain a UserSeenOffer.
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId1,
      commerce::ManagementType::kUserManaged, kMockTimestamp));
  // The second one contains a UserSeenOffer but doesn't have a valid timestamp.
  subscriptions->push_back(commerce::CommerceSubscription(
      commerce::SubscriptionType::kPriceTrack,
      commerce::IdentifierType::kProductClusterId, kMockId2,
      commerce::ManagementType::kUserManaged,
      commerce::kUnknownSubscriptionTimestamp,
      absl::make_optional<commerce::UserSeenOffer>(kMockOfferId, kMockPrice,
                                                   kMockCountry)));
  return subscriptions;
}

// Build an empty subscription list.
std::unique_ptr<std::vector<commerce::CommerceSubscription>>
BuildEmptySubscriptions() {
  return std::make_unique<std::vector<commerce::CommerceSubscription>>();
}

// TODO(crbug.com/1351599): Move this to the endpoint_fetcher component.
class MockEndpointFetcher : public EndpointFetcher {
 public:
  explicit MockEndpointFetcher(
      const net::NetworkTrafficAnnotationTag& annotation_tag)
      : EndpointFetcher(annotation_tag) {}
  ~MockEndpointFetcher() override = default;

  MOCK_METHOD(void, Fetch, (EndpointFetcherCallback callback), (override));

  void MockFetchResponse(std::string response_string) {
    ON_CALL(*this, Fetch)
        .WillByDefault([response_string](EndpointFetcherCallback callback) {
          auto response = std::make_unique<EndpointResponse>();
          response->response = std::move(response_string);
          std::move(callback).Run(std::move(response));
        });
  }
};

}  // namespace

namespace commerce {

class SpySubscriptionsServerProxy : public SubscriptionsServerProxy {
 public:
  SpySubscriptionsServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : SubscriptionsServerProxy(identity_manager,
                                 std::move(url_loader_factory)) {}
  SpySubscriptionsServerProxy(const SpySubscriptionsServerProxy&) = delete;
  SpySubscriptionsServerProxy operator=(const SpySubscriptionsServerProxy&) =
      delete;
  ~SpySubscriptionsServerProxy() override = default;

  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
               const std::string& http_method,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag),
              (override));
};

class SubscriptionsServerProxyTest : public testing::Test {
 public:
  SubscriptionsServerProxyTest() = default;
  ~SubscriptionsServerProxyTest() override = default;

  void SetUp() override {
    fetcher_ =
        std::make_unique<MockEndpointFetcher>(TRAFFIC_ANNOTATION_FOR_TESTS);
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    server_proxy_ = std::make_unique<SpySubscriptionsServerProxy>(
        identity_test_env_.identity_manager(),
        std::move(test_url_loader_factory));
    ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([this]() {
      return std::move(fetcher_);
    });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  std::unique_ptr<SpySubscriptionsServerProxy> server_proxy_;
};

TEST_F(SubscriptionsServerProxyTest, TestCreate) {
  fetcher_->MockFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrl), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  bool callback_executed = false;
  server_proxy_->Create(BuildValidSubscriptions(),
                        base::BindOnce(
                            [](bool* callback_executed, bool succeeded) {
                              ASSERT_EQ(true, succeeded);
                              *callback_executed = true;
                            },
                            &callback_executed));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(true, callback_executed);
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_EmptyList) {
  fetcher_->MockFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  base::RunLoop run_loop;
  server_proxy_->Create(BuildEmptySubscriptions(),
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool succeeded) {
                              ASSERT_EQ(true, succeeded);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestCreate_ServerFailed) {
  fetcher_->MockFetchResponse(kResponseFailed);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrl), kPostHttpMethod,
                                    kExpectedPostDataForCreate, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Create(BuildValidSubscriptions(),
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool succeeded) {
                              ASSERT_EQ(false, succeeded);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestDelete) {
  fetcher_->MockFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrl), kPostHttpMethod,
                                    kExpectedPostDataForDelete, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Delete(BuildValidSubscriptions(),
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool succeeded) {
                              ASSERT_EQ(true, succeeded);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestDelete_EmptyList) {
  fetcher_->MockFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  base::RunLoop run_loop;
  server_proxy_->Delete(BuildEmptySubscriptions(),
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool succeeded) {
                              ASSERT_EQ(true, succeeded);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestDelete_ServerFailed) {
  fetcher_->MockFetchResponse(kResponseFailed);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrl), kPostHttpMethod,
                                    kExpectedPostDataForDelete, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Delete(BuildValidSubscriptions(),
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool succeeded) {
                              ASSERT_EQ(false, succeeded);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet) {
  fetcher_->MockFetchResponse(kValidGetResponse);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(1, static_cast<int>(subscriptions->size()));
            auto subscription = (*subscriptions)[0];
            ASSERT_EQ(SubscriptionType::kPriceTrack, subscription.type);
            ASSERT_EQ(IdentifierType::kProductClusterId, subscription.id_type);
            ASSERT_EQ(ManagementType::kUserManaged,
                      subscription.management_type);
            ASSERT_EQ(kMockId1, subscription.id);
            ASSERT_EQ(kMockTimestamp, subscription.timestamp);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_WrongType) {
  fetcher_->MockFetchResponse(kValidGetResponse);
  EXPECT_CALL(*server_proxy_, CreateEndpointFetcher).Times(0);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kTypeUnspecified,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(SubscriptionsServerProxyTest, TestGet_ServerFailed) {
  fetcher_->MockFetchResponse(kResponseFailed);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kServiceUrlForGet), kGetHttpMethod,
                                    kEmptyPostData, _))
      .Times(1);

  base::RunLoop run_loop;
  server_proxy_->Get(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<std::vector<CommerceSubscription>> subscriptions) {
            ASSERT_EQ(0, static_cast<int>(subscriptions->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace commerce
