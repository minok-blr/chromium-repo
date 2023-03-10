// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/chrome_prefetch_service_delegate.h"

#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

ChromePrefetchServiceDelegate::ChromePrefetchServiceDelegate(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)),
      origin_decider_(
          std::make_unique<PrefetchProxyOriginDecider>(profile_->GetPrefs())) {}

ChromePrefetchServiceDelegate::~ChromePrefetchServiceDelegate() = default;

std::string ChromePrefetchServiceDelegate::GetMajorVersionNumber() {
  return version_info::GetMajorVersionNumber();
}

std::string ChromePrefetchServiceDelegate::GetAcceptLanguageHeader() {
  return net::HttpUtil::GenerateAcceptLanguageHeader(
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
}

GURL ChromePrefetchServiceDelegate::GetDefaultPrefetchProxyHost() {
  return GURL("https://tunnel.9oo91e21p.qjz9zk/");
}

std::string ChromePrefetchServiceDelegate::GetAPIKey() {
  return google_apis::GetAPIKey();
}

GURL ChromePrefetchServiceDelegate::GetDefaultDNSCanaryCheckURL() {
  return GURL("http://dns-tunnel-check.9oo91e21p.qjz9zk/connect");
}

GURL ChromePrefetchServiceDelegate::GetDefaultTLSCanaryCheckURL() {
  return GURL("http://tls-tunnel-check.9oo91e21p.qjz9zk/connect");
}

void ChromePrefetchServiceDelegate::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  return origin_decider_->ReportOriginRetryAfter(url, retry_after);
}

bool ChromePrefetchServiceDelegate::IsOriginOutsideRetryAfterWindow(
    const GURL& url) {
  return origin_decider_->IsOriginOutsideRetryAfterWindow(url);
}

void ChromePrefetchServiceDelegate::ClearData() {
  origin_decider_->OnBrowsingDataCleared();
}

bool ChromePrefetchServiceDelegate::DisableDecoysBasedOnUserSettings() {
  // If the user has opted-in to Make Search and Browsing Better, then there is
  // no need to send decoy requests.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile_->GetPrefs());
  return helper->IsEnabled();
}

bool ChromePrefetchServiceDelegate::IsSomePreloadingEnabled() {
  return prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs());
}

bool ChromePrefetchServiceDelegate::IsExtendedPreloadingEnabled() {
  return prefetch::GetPreloadPagesState(*profile_->GetPrefs()) ==
         prefetch::PreloadPagesState::kExtendedPreloading;
}

bool ChromePrefetchServiceDelegate::IsDomainInPrefetchAllowList(
    const GURL& referring_url) {
  return IsGoogleDomainUrl(referring_url, google_util::ALLOW_SUBDOMAIN,
                           google_util::ALLOW_NON_STANDARD_PORTS) ||
         IsYoutubeDomainUrl(referring_url, google_util::ALLOW_SUBDOMAIN,
                            google_util::ALLOW_NON_STANDARD_PORTS);
}
