// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/device_id_helper.h"

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST(DeviceIdHelper, GenerateSigninScopedDeviceId) {
  EXPECT_FALSE(GenerateSigninScopedDeviceId().empty());
  EXPECT_NE(GenerateSigninScopedDeviceId(), GenerateSigninScopedDeviceId());
}

TEST(DeviceIdHelper, RecreateSigninScopedDeviceId) {
  sync_preferences::TestingPrefServiceSyncable prefs;

  std::string device_id_1 = RecreateSigninScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_1.empty());

  std::string device_id_2 = RecreateSigninScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_2.empty());
  EXPECT_NE(device_id_1, device_id_2);
}

TEST(DeviceIdHelper, GetOrCreateScopedDeviceId) {
  sync_preferences::TestingPrefServiceSyncable prefs;

  std::string device_id_1 = GetOrCreateScopedDeviceId(&prefs);
  EXPECT_FALSE(device_id_1.empty());

  std::string device_id_2 = GetOrCreateScopedDeviceId(&prefs);
  EXPECT_EQ(device_id_1, device_id_2);
}

#endif

}  // namespace
}  // namespace signin
