// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/prefs/pref_service.h"

namespace settings_api = extensions::api::settings_private;

namespace safe_browsing {

const char kGeneratedSafeBrowsingPref[] = "generated.safe_browsing";

GeneratedSafeBrowsingPref::GeneratedSafeBrowsingPref(Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
}

extensions::settings_private::SetPrefResult GeneratedSafeBrowsingPref::SetPref(
    const base::Value* value) {
  if (!value->is_int())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  auto selection = static_cast<SafeBrowsingSetting>(value->GetInt());

  if (selection != SafeBrowsingSetting::DISABLED &&
      selection != SafeBrowsingSetting::STANDARD &&
      selection != SafeBrowsingSetting::ENHANCED)
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  // If SBER is forcefully disabled, Enhanced cannot be selected by the user.
  const bool reporting_on = false;
  const bool reporting_enforced = false;

  if (reporting_enforced && !reporting_on &&
      selection == SafeBrowsingSetting::ENHANCED) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

std::unique_ptr<extensions::api::settings_private::PrefObject>
GeneratedSafeBrowsingPref::GetPrefObject() const {
  auto pref_object =
      std::make_unique<extensions::api::settings_private::PrefObject>();
  pref_object->key = kGeneratedSafeBrowsingPref;
  pref_object->type = extensions::api::settings_private::PREF_TYPE_NUMBER;

  auto safe_browsing_enabled = false;
  auto safe_browsing_enhanced_enabled = false;

  if (safe_browsing_enhanced_enabled && safe_browsing_enabled) {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(SafeBrowsingSetting::ENHANCED));
  } else if (safe_browsing_enabled) {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(SafeBrowsingSetting::STANDARD));
  } else {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(SafeBrowsingSetting::DISABLED));
  }

  ApplySafeBrowsingManagementState(profile_, pref_object.get());

  return pref_object;
}

void GeneratedSafeBrowsingPref::OnSafeBrowsingPreferencesChanged() {
  NotifyObservers(kGeneratedSafeBrowsingPref);
}

/* static */
void GeneratedSafeBrowsingPref::ApplySafeBrowsingManagementState(
    const Profile* profile,
    settings_api::PrefObject* pref_object) {
  // Computing the effective Safe Browsing managed state requires inspecting
  // three different preferences. It is possible that these may be in
  // temporarily conflicting managed states. The enabled preference is always
  // taken as the canonical source of management.
  const bool enabled_enforced = false;
  const bool enabled_recommended = false;
  const bool enabled_recommended_on = false;

  // The enhanced preference may have a recommended setting. This only takes
  // effect if the enabled preference also has a recommended setting.
  const bool enhanced_recommended_on = false;

  // A forcefully disabled reporting preference will disallow enhanced from
  // being selected and thus it must also be considered.
  const bool reporting_on = false;
  const bool reporting_enforced = false;

  if (!enabled_enforced && !enabled_recommended && !reporting_enforced) {
    // No relevant policies are applied.
    return;
  }

  if (enabled_enforced) {
    // Preference is fully controlled.
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    return;
  }

  if (enabled_recommended) {
    // Set enforcement to recommended. This may be upgraded to enforced later
    // in this function.
    pref_object->enforcement =
        settings_api::Enforcement::ENFORCEMENT_RECOMMENDED;
    if (enhanced_recommended_on) {
      pref_object->recommended_value = std::make_unique<base::Value>(
          static_cast<int>(SafeBrowsingSetting::ENHANCED));
    } else if (enabled_recommended_on) {
      pref_object->recommended_value = std::make_unique<base::Value>(
          static_cast<int>(SafeBrowsingSetting::STANDARD));
    } else {
      pref_object->recommended_value = std::make_unique<base::Value>(
          static_cast<int>(SafeBrowsingSetting::DISABLED));
    }
  }

  if (reporting_enforced && !reporting_on) {
    // Reporting has been forcefully disabled by policy. Enhanced protection is
    // thus also implicitly disabled by the same policy.
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;

    pref_object->user_selectable_values =
        std::make_unique<std::vector<std::unique_ptr<base::Value>>>();
    pref_object->user_selectable_values->push_back(
        std::make_unique<base::Value>(
            static_cast<int>(SafeBrowsingSetting::STANDARD)));
    pref_object->user_selectable_values->push_back(
        std::make_unique<base::Value>(
            static_cast<int>(SafeBrowsingSetting::DISABLED)));
  }
}

}  // namespace safe_browsing
