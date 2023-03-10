// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/wm_helper.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

const char* kAllowedDomainAndPathsForPersonalInfoSuggester[][2] = {
    {"discord.com", ""},         {"messenger.com", ""},
    {"web.whatsapp.com", ""},    {"web.skype.com", ""},
    {"duo.9oo91e.qjz9zk", ""},      {"hangouts.9oo91e.qjz9zk", ""},
    {"messages.9oo91e.qjz9zk", ""}, {"web.telegram.org", ""},
    {"voice.9oo91e.qjz9zk", ""},    {"mail.9oo91e.qjz9zk", "/chat"},
};

const char* kAllowedDomainAndPathsForEmojiSuggester[][2] = {
    {"discord.com", ""},         {"messenger.com", ""},
    {"web.whatsapp.com", ""},    {"web.skype.com", ""},
    {"duo.9oo91e.qjz9zk", ""},      {"hangouts.9oo91e.qjz9zk", ""},
    {"messages.9oo91e.qjz9zk", ""}, {"web.telegram.org", ""},
    {"voice.9oo91e.qjz9zk", ""},    {"mail.9oo91e.qjz9zk", "/chat"},
};

// TODO(b/3339115): Add web.skype.com back to the list after compatibility
//    issues are solved.
const char* kAllowedDomainAndPathsForMultiWordSuggester[][2] = {
    {"discord.com", ""},          {"messenger.com", ""},
    {"web.whatsapp.com", ""},     {"duo.9oo91e.qjz9zk", ""},
    {"hangouts.9oo91e.qjz9zk", ""},  {"messages.9oo91e.qjz9zk", ""},
    {"web.telegram.org", ""},     {"voice.9oo91e.qjz9zk", ""},
    {"mail.9oo91e.qjz9zk", "/chat"},
};

const char* kTestUrls[] = {
    "e14s-test",
    "simple_textarea.html",
    "test_page.html",
};

// For some internal websites, we do not want to reveal their urls in plain
// text. See map between url and hash code in
// https://docs.9oo91e.qjz9zk/spreadsheets/d/1VELTWiHrUTEyX4HQI5PL_jDVFreM-lRhThVOurUuOk4/edit#gid=0
const uint32_t kHashedInternalUrls[] = {
    1845308025U,
    153302869U,
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForPersonalInfoSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForEmojiSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForMultiWordSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

const char* kDeniedAppsForDiacritics[] = {
    "iodihamcpbpeioajjeobimgagajmlibd"  // SSH app
};

bool IsTestUrl(GURL url) {
  std::string filename = url.ExtractFileName();
  for (const char* test_url : kTestUrls) {
    if (base::CompareCaseInsensitiveASCII(filename, test_url) == 0) {
      return true;
    }
  }
  return false;
}

bool IsInternalWebsite(GURL url) {
  std::string host = url.host();
  for (const size_t hash_code : kHashedInternalUrls) {
    if (hash_code == base::PersistentHash(host)) {
      return true;
    }
  }
  return false;
}

bool AtDomainWithPathPrefix(GURL url,
                            const std::string& domain,
                            const std::string& prefix) {
  return url.DomainIs(domain) && url.has_path() &&
         base::StartsWith(url.path(), prefix);
}

template <size_t N>
bool IsMatchedUrlWithPathPrefix(const char* (&allowedDomainAndPaths)[N][2],
                                GURL url) {
  if (IsTestUrl(url) || IsInternalWebsite(url))
    return true;
  for (size_t i = 0; i < N; i++) {
    auto domain = allowedDomainAndPaths[i][0];
    auto path_prefix = allowedDomainAndPaths[i][1];
    if (AtDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

template <size_t N>
bool IsMatchedApp(const char* (&allowedApps)[N], WindowProperties w) {
  if (!w.arc_package_name.empty() &&
      std::find(allowedApps, allowedApps + N, w.arc_package_name) !=
          allowedApps + N) {
    return true;
  }
  if (!w.app_id.empty() &&
      std::find(allowedApps, allowedApps + N, w.app_id) != allowedApps + N) {
    return true;
  }
  return false;
}

void ReturnEnabledSuggestions(
    AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback,
    WindowProperties window_properties,
    const absl::optional<GURL>& current_url) {
  if (!current_url.has_value()) {
    std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{});
    return;
  }

  // Allow-list (will only allow if matched)
  bool emoji_suggestions_allowed =
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForEmojiSuggester,
                                 *current_url) ||
      IsMatchedApp(kAllowedAppsForEmojiSuggester, window_properties);

  // Allow-list (will only allow if matched)
  bool multi_word_suggestions_allowed =
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForMultiWordSuggester,
                                 *current_url) ||
      IsMatchedApp(kAllowedAppsForMultiWordSuggester, window_properties);

  // Allow-list (will only allow if matched)
  bool personal_info_suggestions_allowed =
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForPersonalInfoSuggester,
                                 *current_url) ||
      IsMatchedApp(kAllowedAppsForPersonalInfoSuggester, window_properties);

  // Deny-list (will block if matched, otherwise allow)
  bool diacritic_suggestions_allowed =
      !IsMatchedApp(kDeniedAppsForDiacritics, window_properties);

  std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
      .emoji_suggestions = emoji_suggestions_allowed,
      .multi_word_suggestions = multi_word_suggestions_allowed,
      .personal_info_suggestions = personal_info_suggestions_allowed,
      .diacritic_suggestions = diacritic_suggestions_allowed,
  });
}

}  // namespace

AssistiveSuggesterClientFilter::AssistiveSuggesterClientFilter(
    GetUrlCallback get_url,
    GetFocusedWindowPropertiesCallback get_window_properties)
    : get_url_(std::move(get_url)),
      get_window_properties_(std::move(get_window_properties)) {}

AssistiveSuggesterClientFilter::~AssistiveSuggesterClientFilter() = default;

void AssistiveSuggesterClientFilter::FetchEnabledSuggestionsThen(
    FetchEnabledSuggestionsCallback callback) {
  WindowProperties window_properties = get_window_properties_.Run();
  get_url_.Run(base::BindOnce(ReturnEnabledSuggestions, std::move(callback),
                              window_properties));
}

}  // namespace input_method
}  // namespace ash
