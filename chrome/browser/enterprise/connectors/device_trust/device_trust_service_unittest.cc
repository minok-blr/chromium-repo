// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include <tuple>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/mock_attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/mock_signals_service.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace {

base::Value::List GetOrigins() {
  base::Value::List origins;
  origins.Append("example1.example.com");
  origins.Append("example2.example.com");
  return origins;
}

// A sample VerifiedAccess v2 challenge rerepsented as a JSON string.
constexpr char kJsonChallenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";

std::string GetSerializedSignedChallenge(const std::string& response) {
  absl::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data.value().FindPath("challenge"))
    return std::string();

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(data.value().FindPath("challenge")->GetString(),
                          &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

}  // namespace

namespace enterprise_connectors {

using test::MockAttestationService;
using test::MockSignalsService;
using AttestationCallback = DeviceTrustService::AttestationCallback;

class DeviceTrustServiceTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());

    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                       is_flag_enabled());

    if (is_policy_enabled()) {
      EnableServicePolicy();
    } else {
      DisableServicePolicy();
    }
  }

  void EnableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       base::Value(GetOrigins()));
  }

  void DisableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       base::Value(base::Value::List()));
  }

  std::unique_ptr<DeviceTrustService> CreateService() {
    connector_ = std::make_unique<DeviceTrustConnectorService>(&prefs_);

    auto mock_attestation_service = std::make_unique<MockAttestationService>();
    mock_attestation_service_ = mock_attestation_service.get();

    auto mock_signals_service = std::make_unique<MockSignalsService>();
    mock_signals_service_ = mock_signals_service.get();

    return std::make_unique<DeviceTrustService>(
        std::move(mock_attestation_service), std::move(mock_signals_service),
        connector_.get());
  }

  bool is_attestation_flow_enabled() {
    return is_flag_enabled() && is_policy_enabled();
  }

  bool is_flag_enabled() { return std::get<0>(GetParam()); }
  bool is_policy_enabled() { return std::get<1>(GetParam()); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<DeviceTrustConnectorService> connector_;
  raw_ptr<MockAttestationService> mock_attestation_service_;
  raw_ptr<MockSignalsService> mock_signals_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
};

// Tests that IsEnabled returns true only when the feature flag is enabled and
// the policy has some URLs.
TEST_P(DeviceTrustServiceTest, IsEnabled) {
  auto device_trust_service = CreateService();
  EXPECT_EQ(is_attestation_flow_enabled(), device_trust_service->IsEnabled());
}

// Tests that the service kicks off the attestation flow properly.
TEST_P(DeviceTrustServiceTest, BuildChallengeResponse) {
  auto device_trust_service = CreateService();

  std::string fake_device_id = "fake_device_id";
  EXPECT_CALL(*mock_signals_service_, CollectSignals(_))
      .WillOnce(Invoke(
          [&fake_device_id](
              base::OnceCallback<void(base::Value::Dict)> signals_callback) {
            auto fake_signals = std::make_unique<base::Value::Dict>();
            fake_signals->Set(device_signals::names::kDeviceId, fake_device_id);
            std::move(signals_callback).Run(std::move(*fake_signals));
          }));

  EXPECT_CALL(*mock_attestation_service_,
              BuildChallengeResponseForVAChallenge(
                  GetSerializedSignedChallenge(kJsonChallenge), _, _))
      .WillOnce(Invoke([&fake_device_id](const std::string& challenge,
                                         const base::Value::Dict signals,
                                         AttestationCallback callback) {
        EXPECT_EQ(signals.FindString(device_signals::names::kDeviceId)->c_str(),
                  fake_device_id);
        std::move(callback).Run(challenge);
      }));

  base::RunLoop run_loop;
  device_trust_service->BuildChallengeResponse(
      kJsonChallenge,
      /*callback=*/base::BindLambdaForTesting(
          [&run_loop](const std::string& response) { run_loop.Quit(); }));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceTrustServiceTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace enterprise_connectors