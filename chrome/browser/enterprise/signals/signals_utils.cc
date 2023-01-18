// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_utils.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "components/component_updater/pref_names.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_signals {
namespace utils {

namespace {

bool IsURLBlocked(const GURL& url, PolicyBlocklistService* service) {
  if (!service)
    return false;

  policy::URLBlocklist::URLBlocklistState state =
      service->GetURLBlocklistState(url);

  return state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

}  // namespace

absl::optional<bool> GetThirdPartyBlockingEnabled(PrefService* local_state) {
  DCHECK(local_state);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return local_state->GetBoolean(prefs::kThirdPartyBlockingEnabled);
#else
  return absl::nullopt;
#endif
}

bool GetBuiltInDnsClientEnabled(PrefService* local_state) {
  DCHECK(local_state);
  return local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

absl::optional<bool> GetChromeCleanupEnabled(PrefService* local_state) {
  DCHECK(local_state);
#if BUILDFLAG(IS_WIN)
  return local_state->GetBoolean(prefs::kSwReporterEnabled);
#else
  return absl::nullopt;
#endif
}

bool GetChromeRemoteDesktopAppBlocked(PolicyBlocklistService* service) {
  DCHECK(service);
  return IsURLBlocked(GURL("https://remotedesktop.9oo91e.qjz9zk"), service) ||
         IsURLBlocked(GURL("https://remotedesktop.corp.9oo91e.qjz9zk"), service);
}

}  // namespace utils
}  // namespace enterprise_signals
