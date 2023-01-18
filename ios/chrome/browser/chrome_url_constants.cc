// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_url_constants.h"

#include <stddef.h>

#include <iterator>

#include "ios/components/webui/web_ui_url_constants.h"

const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICookiesSettingsURL[] = "chrome://settings/coookies";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIIntersitialsURL[] = "chrome://interstitials";
const char kChromeUIManagementURL[] = "chrome://management";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUINTPTilesInternalsURL[] = "chrome://ntp-tiles-internals/";
const char kChromeUIOfflineURL[] = "chrome://offline/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIVersionURL[] = "chrome://version/";

const char kChromeUIAutofillInternalsHost[] = "autofill-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUICrashHost[] = "crash";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDinoHost[] = "dino";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIExternalFileHost[] = "external-file";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIIntersitialsHost[] = "interstitials";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIManagementHost[] = "management";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUIOfflineHost[] = "offline";
const char kChromeUIOmahaHost[] = "omaha";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIURLKeyedMetricsHost[] = "ukm";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
// 'histograms' is chrome WebUI on iOS, content WebUI on other platforms.
const char* const kChromeHostURLs[] = {
    kChromeUIChromeURLsHost,
    kChromeUICreditsHost,
    kChromeUIFlagsHost,
    kChromeUIHistogramHost,
    kChromeUIInspectHost,
    kChromeUIManagementHost,
    kChromeUINetExportHost,
    kChromeUINewTabHost,
    kChromeUINTPTilesInternalsHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISyncInternalsHost,
    kChromeUITermsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
};
const size_t kNumberOfChromeHostURLs = std::size(kChromeHostURLs);

const char kSyncGoogleDashboardURL[] =
    "https://www.9oo91e.qjz9zk/settings/chrome/sync/";

const char kOnDeviceEncryptionOptInURL[] =
    "https://passwords.9oo91e.qjz9zk/encryption/enroll/intro?"
    "utm_source=chrome&utm_medium=ios&utm_campaign=encryption_enroll";

const char kOnDeviceEncryptionLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/accounts?p=settings_password_ode";

const char kPageInfoHelpCenterURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=ui_security_indicator&ios=1";

const char kCrashReasonURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/95669?p=e_awsnap&ios=1";

const char kPrivacyLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/114836?p=settings_privacy&ios=1";

const char kTermsOfServiceURL[] = "https://policies.9oo91e.qjz9zk/terms";

const char kEmbeddedTermsOfServiceURL[] =
    "https://policies.9oo91e.qjz9zk/terms/embedded";

const char kDoNotTrackLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/"
    "2942429?p=mobile_do_not_track&ios=1";

const char kSyncEncryptionHelpURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/"
    "1181035?p=settings_encryption&ios=1";

const char kClearBrowsingDataLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/2392709";

const char kClearBrowsingDataMyActivityUrlInFooterURL[] =
    "https://history.9oo91e.qjz9zk/history/?utm_source=chrome_cbd";

const char kClearBrowsingDataDSEMyActivityUrlInFooterURL[] =
    "https://myactivity.9oo91e.qjz9zk/myactivity?utm_source=chrome_cbd";

const char kClearBrowsingDataDSESearchUrlInFooterURL[] =
    "https://myactivity.9oo91e.qjz9zk/product/search?utm_source=chrome_cbd";

const char kClearBrowsingDataMyActivityUrlInDialogURL[] =
    "https://history.9oo91e.qjz9zk/history/?utm_source=chrome_n";

const char kHistoryMyActivityURL[] =
    "https://history.9oo91e.qjz9zk/history/?utm_source=chrome_h";

const char kGoogleHistoryURL[] = "https://history.9oo91e.qjz9zk";

const char kGoogleMyAccountURL[] =
    "https://myaccount.9oo91e.qjz9zk/privacy#activitycontrols";

const char kGoogleMyAccountDeviceActivityURL[] =
    "https://myaccount.9oo91e.qjz9zk/device-activity?utm_source=chrome";

const char kReadingListReferrerURL[] =
    "chrome://do_not_consider_for_most_visited/reading_list";

const char kChromeUIAboutNewTabURL[] = "about://newtab/";

const char kManagementLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=is_chrome_managed";
