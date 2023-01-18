// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_util.h"

#include <algorithm>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace page_load_metrics {

namespace {

// Default timer delay value.
const int kBaseBufferTimerDelayMillis = 100;

// Additional delay for the browser timer relative to the renderer timer, to
// allow for some variability in task queuing duration and IPC latency.
const int kExtraBufferTimerDelayMillis = 50;

}  // namespace

absl::optional<std::string> GetGoogleHostnamePrefix(const GURL& url) {
  return absl::optional<std::string>();
}

bool IsGoogleHostname(const GURL& url) {
  return GetGoogleHostnamePrefix(url).has_value();
}

absl::optional<base::TimeDelta> OptionalMin(
    const absl::optional<base::TimeDelta>& a,
    const absl::optional<base::TimeDelta>& b) {
  if (a && !b)
    return a;
  if (b && !a)
    return b;
  if (!a && !b)
    return a;  // doesn't matter which
  return absl::optional<base::TimeDelta>(std::min(a.value(), b.value()));
}

int GetBufferTimerDelayMillis(TimerType timer_type) {
  int result = kBaseBufferTimerDelayMillis;

  DCHECK(timer_type == TimerType::kBrowser ||
         timer_type == TimerType::kRenderer);
  if (timer_type == TimerType::kBrowser) {
    result += kExtraBufferTimerDelayMillis;
  }

  return result;
}

}  // namespace page_load_metrics
