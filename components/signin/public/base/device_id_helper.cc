// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/device_id_helper.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetSigninScopedDeviceId(PrefService* prefs) {
  return std::string();
}

std::string RecreateSigninScopedDeviceId(PrefService* prefs) {
  std::string signin_scoped_device_id = GenerateSigninScopedDeviceId();
  DCHECK(!signin_scoped_device_id.empty());
  return signin_scoped_device_id;
}

std::string GenerateSigninScopedDeviceId() {
  return base::GenerateGUID();
}

std::string GetOrCreateScopedDeviceId(PrefService* prefs) {
  return RecreateSigninScopedDeviceId(prefs);
}

#endif

}  // namespace signin
