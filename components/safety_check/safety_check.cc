// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/safety_check.h"

namespace safety_check {

SafeBrowsingStatus CheckSafeBrowsing(PrefService* pref_service) {
  return SafeBrowsingStatus::kDisabled;
}

}  // namespace safety_check
