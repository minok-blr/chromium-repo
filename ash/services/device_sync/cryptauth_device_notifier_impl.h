// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_
#define ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "ash/services/device_sync/cryptauth_device_notifier.h"
#include "ash/services/device_sync/cryptauth_feature_type.h"
#include "ash/services/device_sync/network_request_error.h"
#include "ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;

// An implementation of CryptAuthDeviceNotifier, using instances of
// CryptAuthClient to make the BatchNotifyGroupDevices API calls to CryptAuth.
// The requests made via NotifyDevices() are queued and processed sequentially.
// This implementation handles timeouts internally, so a callback passed to
// NotifyDevices() is always guaranteed to be invoked.
class CryptAuthDeviceNotifierImpl : public CryptAuthDeviceNotifier {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthDeviceNotifier> Create(
        const std::string& instance_id,
        const std::string& instance_id_token,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceNotifier> CreateInstance(
        const std::string& instance_id,
        const std::string& instance_id_token,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthDeviceNotifierImpl(const CryptAuthDeviceNotifierImpl&) = delete;
  CryptAuthDeviceNotifierImpl& operator=(const CryptAuthDeviceNotifierImpl&) =
      delete;

  ~CryptAuthDeviceNotifierImpl() override;

 private:
  enum class State { kIdle, kWaitingForBatchNotifyGroupDevicesResponse };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static absl::optional<base::TimeDelta> GetTimeoutForState(State state);

  struct Request {
    Request(const base::flat_set<std::string>& device_ids,
            cryptauthv2::TargetService target_service,
            CryptAuthFeatureType feature_type,
            base::OnceClosure success_callback,
            base::OnceCallback<void(NetworkRequestError)> error_callback);

    Request(Request&& request);

    ~Request();

    base::flat_set<std::string> device_ids;
    cryptauthv2::TargetService target_service;
    CryptAuthFeatureType feature_type;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  CryptAuthDeviceNotifierImpl(const std::string& instance_id,
                              const std::string& instance_id_token,
                              CryptAuthClientFactory* client_factory,
                              std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthDeviceNotifier:
  void NotifyDevices(
      const base::flat_set<std::string>& device_ids,
      cryptauthv2::TargetService target_service,
      CryptAuthFeatureType feature_type,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

  void SetState(State state);
  void OnTimeout();

  void ProcessRequestQueue();
  void OnBatchNotifyGroupDevicesSuccess(
      const cryptauthv2::BatchNotifyGroupDevicesResponse& response);
  void OnBatchNotifyGroupDevicesFailure(NetworkRequestError error);
  void FinishAttempt(absl::optional<NetworkRequestError> error);

  State state_ = State::kIdle;
  base::TimeTicks last_state_change_timestamp_;
  base::queue<Request> pending_requests_;

  std::string instance_id_;
  std::string instance_id_token_;
  CryptAuthClientFactory* client_factory_ = nullptr;
  std::unique_ptr<CryptAuthClient> cryptauth_client_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::WeakPtrFactory<CryptAuthDeviceNotifierImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  //  ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_