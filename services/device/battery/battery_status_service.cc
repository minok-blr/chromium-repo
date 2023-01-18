// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_manager.h"

namespace device {

BatteryStatusService::BatteryStatusService()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      update_callback_(
          base::BindRepeating(&BatteryStatusService::NotifyConsumers,
                              base::Unretained(this))),
      is_shutdown_(false) {
}

BatteryStatusService::~BatteryStatusService() = default;

BatteryStatusService* BatteryStatusService::GetInstance() {
  static base::NoDestructor<BatteryStatusService> service_wrapper;
  return service_wrapper.get();
}

base::CallbackListSubscription BatteryStatusService::AddCallback(
    const BatteryUpdateCallback& callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_shutdown_);

  // Always pass the default values.
  callback.Run(mojom::BatteryStatus());

  return callback_list_.Add(callback);
}

void BatteryStatusService::NotifyConsumers(const mojom::BatteryStatus& status) {
}

void BatteryStatusService::Shutdown() {
  is_shutdown_ = true;
}

const BatteryStatusService::BatteryUpdateCallback&
BatteryStatusService::GetUpdateCallbackForTesting() const {
  return update_callback_;
}

void BatteryStatusService::SetBatteryManagerForTesting(
    std::unique_ptr<BatteryStatusManager> test_battery_manager) {
  is_shutdown_ = false;
  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

}  // namespace device
