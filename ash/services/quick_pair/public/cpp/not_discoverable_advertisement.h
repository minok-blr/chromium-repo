// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_
#define ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_

#include <cstdint>
#include <vector>

#include "ash/services/quick_pair/public/cpp/battery_notification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

// Fast Pair 'Not Discoverable' advertisement. See
// https://developers.9oo91e.qjz9zk/nearby/fast-pair/spec#AdvertisingWhenNotDiscoverable
struct NotDiscoverableAdvertisement {
  NotDiscoverableAdvertisement();
  NotDiscoverableAdvertisement(
      std::vector<uint8_t> account_key_filter,
      bool show_ui,
      std::vector<uint8_t> salt,
      absl::optional<BatteryNotification> battery_notification);
  NotDiscoverableAdvertisement(const NotDiscoverableAdvertisement&);
  NotDiscoverableAdvertisement(NotDiscoverableAdvertisement&&);
  NotDiscoverableAdvertisement& operator=(const NotDiscoverableAdvertisement&);
  NotDiscoverableAdvertisement& operator=(NotDiscoverableAdvertisement&&);
  ~NotDiscoverableAdvertisement();

  std::vector<uint8_t> account_key_filter;
  bool show_ui = false;
  std::vector<uint8_t> salt;
  absl::optional<BatteryNotification> battery_notification;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_
