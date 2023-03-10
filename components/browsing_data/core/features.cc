// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/features.h"

#include "build/build_config.h"

namespace browsing_data {
namespace features {

const base::Feature kClearDataOnExit{"ClearDataOnExit", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableRemovingAllThirdPartyCookies{
    "EnableRemovingAllThirdPartyCookies", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableBrowsingDataLifetimeManager{
    "BrowsingDataLifetimeManager", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features
}  // namespace browsing_data
