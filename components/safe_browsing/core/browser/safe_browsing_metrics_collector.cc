// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"

namespace {

using EventType = safe_browsing::SafeBrowsingMetricsCollector::EventType;
using UserState = safe_browsing::SafeBrowsingMetricsCollector::UserState;

const int kMetricsLoggingIntervalDay = 1;

// The max length of event timestamps stored in pref.
const int kTimestampsMaxLength = 30;
// The quota for ESB disabled metrics. ESB disabled metrics should not be logged
// more than the quota in a week.
const int kEsbDisabledMetricsQuota = 3;
// Events that are older than 30 days are removed from pref.
const int kEventMaxDurationDay = 30;
// The ESB enabled duration is considered short if it's under 1 hour, long if
// it's at least 24 hours, and medium if it's in between.
const int kEsbShortEnabledUpperBoundHours = 1;
const int kEsbLongEnabledLowerBoundHours = 24;

std::string EventTypeToPrefKey(const EventType& type) {
  return base::NumberToString(static_cast<int>(type));
}

std::string UserStateToPrefKey(const UserState& user_state) {
  return base::NumberToString(static_cast<int>(user_state));
}

base::Value TimeToPrefValue(const base::Time& time) {
  return base::Int64ToValue(time.ToDeltaSinceWindowsEpoch().InSeconds());
}

base::Time PrefValueToTime(const base::Value& value) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(base::ValueToInt64(value).value_or(0)));
}

}  // namespace

namespace safe_browsing {

SafeBrowsingMetricsCollector::SafeBrowsingMetricsCollector(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
}

void SafeBrowsingMetricsCollector::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

void SafeBrowsingMetricsCollector::StartLogging() {
}

void SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging() {
  LogDailyOptInMetrics();
  LogDailyEventMetrics();
  RemoveOldEventsFromPref();

  ScheduleNextLoggingAfterInterval(base::Days(kMetricsLoggingIntervalDay));
}

void SafeBrowsingMetricsCollector::ScheduleNextLoggingAfterInterval(
    base::TimeDelta interval) {
  metrics_collector_timer_.Stop();
  metrics_collector_timer_.Start(
      FROM_HERE, interval, this,
      &SafeBrowsingMetricsCollector::LogMetricsAndScheduleNextLogging);
}

void SafeBrowsingMetricsCollector::LogDailyOptInMetrics() {
}

void SafeBrowsingMetricsCollector::LogDailyEventMetrics() {
}

void SafeBrowsingMetricsCollector::RemoveOldEventsFromPref() {
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventToPref(
    EventType event_type) {
}

void SafeBrowsingMetricsCollector::AddBypassEventToPref(
    ThreatSource threat_source) {
  EventType event;
  switch (threat_source) {
    case ThreatSource::LOCAL_PVER4:
    case ThreatSource::REMOTE:
      event = EventType::DATABASE_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::CLIENT_SIDE_DETECTION:
      event = EventType::CSD_INTERSTITIAL_BYPASS;
      break;
    case ThreatSource::REAL_TIME_CHECK:
      event = EventType::REAL_TIME_INTERSTITIAL_BYPASS;
      break;
    default:
      NOTREACHED() << "Unexpected threat source.";
      event = EventType::DATABASE_INTERSTITIAL_BYPASS;
  }
  AddSafeBrowsingEventToPref(event);
}

absl::optional<base::Time>
SafeBrowsingMetricsCollector::GetLatestEventTimestamp(EventType event_type) {
  return GetLatestEventTimestamp(base::BindRepeating(
      [](const EventType& target_event_type, const EventType& event_type) {
        return target_event_type == event_type;
      },
      event_type));
}

absl::optional<base::Time>
SafeBrowsingMetricsCollector::GetLatestEventTimestamp(
    EventTypeFilter event_type_filter) {
  // Events are not logged when Safe Browsing is disabled.
    return absl::nullopt;
}

absl::optional<base::Time>
SafeBrowsingMetricsCollector::GetLatestSecuritySensitiveEventTimestamp() {
  return GetLatestEventTimestamp(base::BindRepeating(
      &SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType));
}

void SafeBrowsingMetricsCollector::AddSafeBrowsingEventAndUserStateToPref(
    UserState user_state,
    EventType event_type) {
}

void SafeBrowsingMetricsCollector::OnEnhancedProtectionPrefChanged() {
}

const base::Value::Dict*
SafeBrowsingMetricsCollector::GetSafeBrowsingEventDictionary(
    UserState user_state) {
  return nullptr;
}

absl::optional<SafeBrowsingMetricsCollector::Event>
SafeBrowsingMetricsCollector::GetLatestEventFromEventType(
    UserState user_state,
    EventType event_type) {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(user_state);

  if (!event_dict) {
    return absl::nullopt;
  }

  const base::Value::List* timestamps =
      event_dict->FindList(EventTypeToPrefKey(event_type));

  if (timestamps && timestamps->size() > 0) {
    base::Time time = PrefValueToTime(timestamps->back());
    return Event(event_type, time);
  }

  return absl::nullopt;
}

absl::optional<SafeBrowsingMetricsCollector::Event>
SafeBrowsingMetricsCollector::GetLatestEventFromEventTypeFilter(
    UserState user_state,
    EventTypeFilter event_type_filter) {
  std::vector<Event> bypass_events;
  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (!event_type_filter.Run(event_type)) {
      continue;
    }
    const absl::optional<Event> latest_event =
        GetLatestEventFromEventType(user_state, event_type);
    if (latest_event) {
      bypass_events.emplace_back(latest_event.value());
    }
  }

  const auto latest_event = std::max_element(
      bypass_events.begin(), bypass_events.end(),
      [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });

  return (latest_event != bypass_events.end())
             ? absl::optional<Event>(*latest_event)
             : absl::nullopt;
}

void SafeBrowsingMetricsCollector::LogEnhancedProtectionDisabledMetrics() {
  base::UmaHistogramCounts100(
      "SafeBrowsing.EsbDisabled.TimesDisabledLast28Days." +
          GetTimesDisabledSuffix(),
      GetEventCountSince(UserState::kEnhancedProtection,
                         EventType::USER_STATE_DISABLED,
                         base::Time::Now() - base::Days(28)));

  int disabled_times_last_week = GetEventCountSince(
      UserState::kEnhancedProtection, EventType::USER_STATE_DISABLED,
      base::Time::Now() - base::Days(7));
  if (disabled_times_last_week <= kEsbDisabledMetricsQuota) {
    LogThrottledEnhancedProtectionDisabledMetrics();
  }
}

void SafeBrowsingMetricsCollector::
    LogThrottledEnhancedProtectionDisabledMetrics() {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(UserState::kEnhancedProtection);
  if (!event_dict) {
    return;
  }

  for (int event_type_int = 0; event_type_int <= EventType::kMaxValue;
       event_type_int += 1) {
    EventType event_type = static_cast<EventType>(event_type_int);
    if (IsBypassEventType(event_type)) {
      base::UmaHistogramCounts100(
          "SafeBrowsing.EsbDisabled.BypassCountLast28Days." +
              GetEventTypeMetricSuffix(event_type),
          GetEventCountSince(UserState::kEnhancedProtection, event_type,
                             base::Time::Now() - base::Days(28)));
    }
    if (IsSecuritySensitiveEventType(event_type)) {
      base::UmaHistogramCounts100(
          "SafeBrowsing.EsbDisabled.SecuritySensitiveCountLast28Days." +
              GetEventTypeMetricSuffix(event_type),
          GetEventCountSince(UserState::kEnhancedProtection, event_type,
                             base::Time::Now() - base::Days(28)));
    }
  }

  absl::optional<SafeBrowsingMetricsCollector::Event> latest_bypass_event =
      GetLatestEventFromEventTypeFilter(
          UserState::kEnhancedProtection,
          base::BindRepeating(
              &SafeBrowsingMetricsCollector::IsBypassEventType));
  if (latest_bypass_event) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.EsbDisabled.LastBypassEventType",
        latest_bypass_event->type);
    base::UmaHistogramCustomTimes(
        "SafeBrowsing.EsbDisabled.LastBypassEventInterval." +
            GetEventTypeMetricSuffix(latest_bypass_event->type),
        /* sample */ base::Time::Now() - latest_bypass_event->timestamp,
        /* min */ base::Seconds(1),
        /* max */ base::Days(1), /* buckets */ 50);
  }

  absl::optional<SafeBrowsingMetricsCollector::Event>
      latest_security_sensitive_event = GetLatestEventFromEventTypeFilter(
          UserState::kEnhancedProtection,
          base::BindRepeating(
              &SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType));
  if (latest_security_sensitive_event) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.EsbDisabled.LastSecuritySensitiveEventType",
        latest_security_sensitive_event->type);
    base::UmaHistogramCustomTimes(
        "SafeBrowsing.EsbDisabled.LastSecuritySensitiveEventInterval." +
            GetEventTypeMetricSuffix(latest_security_sensitive_event->type),
        /* sample */ base::Time::Now() -
            latest_security_sensitive_event->timestamp,
        /* min */ base::Seconds(1),
        /* max */ base::Days(1), /* buckets */ 50);
  }

  const absl::optional<Event> latest_enabled_event =
      GetLatestEventFromEventType(UserState::kEnhancedProtection,
                                  EventType::USER_STATE_ENABLED);
  if (latest_enabled_event) {
    const auto days_since_enabled =
        (base::Time::Now() - latest_enabled_event.value().timestamp).InDays();
    base::UmaHistogramCounts100("SafeBrowsing.EsbDisabled.LastEnabledInterval",
                                /* sample */ days_since_enabled);
  }
}

int SafeBrowsingMetricsCollector::GetEventCountSince(UserState user_state,
                                                     EventType event_type,
                                                     base::Time since_time) {
  const base::Value::Dict* event_dict =
      GetSafeBrowsingEventDictionary(user_state);
  if (!event_dict) {
    return 0;
  }
  const base::Value::List* timestamps =
      event_dict->FindList(EventTypeToPrefKey(event_type));
  if (!timestamps) {
    return 0;
  }

  return std::count_if(timestamps->begin(), timestamps->end(),
                       [&](const base::Value& timestamp) {
                         return PrefValueToTime(timestamp) > since_time;
                       });
}

UserState SafeBrowsingMetricsCollector::GetUserState() {
  NOTREACHED() << "Unexpected Safe Browsing state.";
}

bool SafeBrowsingMetricsCollector::IsBypassEventType(const EventType& type) {
  switch (type) {
    case EventType::USER_STATE_DISABLED:
    case EventType::USER_STATE_ENABLED:
    case EventType::SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_SSL_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_PASSWORD_PROTECTION:
    case EventType::SECURITY_SENSITIVE_DOWNLOAD:
      return false;
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
    case EventType::CSD_INTERSTITIAL_BYPASS:
    case EventType::REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
      return true;
  }
}

bool SafeBrowsingMetricsCollector::IsSecuritySensitiveEventType(
    const EventType& type) {
  switch (type) {
    case EventType::USER_STATE_DISABLED:
    case EventType::USER_STATE_ENABLED:
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
    case EventType::CSD_INTERSTITIAL_BYPASS:
    case EventType::REAL_TIME_INTERSTITIAL_BYPASS:
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
      return false;
    case EventType::SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_SSL_INTERSTITIAL:
    case EventType::SECURITY_SENSITIVE_PASSWORD_PROTECTION:
    case EventType::SECURITY_SENSITIVE_DOWNLOAD:
      return true;
  }
}

std::string SafeBrowsingMetricsCollector::GetUserStateMetricSuffix(
    const UserState& user_state) {
  switch (user_state) {
    case UserState::kStandardProtection:
      return "StandardProtection";
    case UserState::kEnhancedProtection:
      return "EnhancedProtection";
    case UserState::kManaged:
      return "Managed";
  }
}

std::string SafeBrowsingMetricsCollector::GetEventTypeMetricSuffix(
    const EventType& event_type) {
  switch (event_type) {
    case EventType::USER_STATE_DISABLED:
      return "UserStateDisabled";
    case EventType::USER_STATE_ENABLED:
      return "UserStateEnabled";
    case EventType::DATABASE_INTERSTITIAL_BYPASS:
      return "DatabaseInterstitialBypass";
    case EventType::CSD_INTERSTITIAL_BYPASS:
      return "CsdInterstitialBypass";
    case EventType::REAL_TIME_INTERSTITIAL_BYPASS:
      return "RealTimeInterstitialBypass";
    case EventType::DANGEROUS_DOWNLOAD_BYPASS:
      return "DangerousDownloadBypass";
    case EventType::PASSWORD_REUSE_MODAL_BYPASS:
      return "PasswordReuseModalBypass";
    case EventType::EXTENSION_ALLOWLIST_INSTALL_BYPASS:
      return "ExtensionAllowlistInstallBypass";
    case EventType::NON_ALLOWLISTED_EXTENSION_RE_ENABLED:
      return "NonAllowlistedExtensionReEnabled";
    case EventType::SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL:
      return "SafeBrowsingInterstitial";
    case EventType::SECURITY_SENSITIVE_SSL_INTERSTITIAL:
      return "SSLInterstitial";
    case EventType::SECURITY_SENSITIVE_PASSWORD_PROTECTION:
      return "PasswordProtection";
    case EventType::SECURITY_SENSITIVE_DOWNLOAD:
      return "Download";
  }
}

std::string SafeBrowsingMetricsCollector::GetTimesDisabledSuffix() {
  const absl::optional<Event> latest_enabled_event =
      GetLatestEventFromEventType(UserState::kEnhancedProtection,
                                  EventType::USER_STATE_ENABLED);

  if (!latest_enabled_event) {
    // This code path could be possible if ESB was enabled via policy but
    // later disabled by the user, since policy enables/disables are not
    // tracked. It's also possible if it's been longer than kEventMaxDurationDay
    // days since the latest enabled event.
    return "NeverEnabled";
  }
  const auto hours_since_enabled =
      (base::Time::Now() - latest_enabled_event.value().timestamp).InHours();
  return hours_since_enabled < kEsbShortEnabledUpperBoundHours
             ? "ShortEnabled"
             : hours_since_enabled < kEsbLongEnabledLowerBoundHours
                   ? "MediumEnabled"
                   : "LongEnabled";
}

SafeBrowsingMetricsCollector::Event::Event(EventType type, base::Time timestamp)
    : type(type), timestamp(timestamp) {}

}  // namespace safe_browsing
