// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/metrics/safe_browsing_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace safe_browsing {

SafeBrowsingMetricsProvider::SafeBrowsingMetricsProvider() = default;

SafeBrowsingMetricsProvider::~SafeBrowsingMetricsProvider() = default;

void SafeBrowsingMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
}

}  // namespace safe_browsing
