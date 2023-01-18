// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"

namespace chrome {

const char kAccessCodeCastLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/a/?p=cast_to_class_teacher";

const char kAccessibilityLabelsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=image_descriptions";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_automatic_settings_reset";

const char kAdvancedProtectionDownloadLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/accounts/accounts?p=safe-browsing";

const char kAppNotificationsBrowserSettingsURL[] =
    "chrome://settings/content/notifications";

const char kBluetoothAdapterOffHelpURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=bluetooth";

const char kCastCloudServicesHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromecast/?p=casting_cloud_services";

const char kCastNoDestinationFoundURL[] =
    "https://support.9oo91e.qjz9zk/chromecast/?p=no_cast_destination";

const char kChooserHidOverviewUrl[] =
    "https://support.9oo91e.qjz9zk/chrome?p=webhid";

const char kChooserSerialOverviewUrl[] =
    "https://support.9oo91e.qjz9zk/chrome?p=webserial";

const char kChooserUsbOverviewURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=webusb";

const char kChromeBetaForumURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=beta_forum";

const char kChromeFixUpdateProblems[] =
    "https://support.9oo91e.qjz9zk/chrome?p=fix_chrome_updates";

const char kChromeHelpViaKeyboardURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.9oo91e.qjz9zk/chromebook/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaMenuURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.9oo91e.qjz9zk/chromebook/?p=help&ctx=menu";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=help&ctx=menu";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaWebUIURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=help&ctx=settings";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kChromeOsHelpViaWebUIURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.9oo91e.qjz9zk/chromebook/?p=help&ctx=settings";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kIsolatedAppScheme[] = "isolated-app";

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchLocalNtpHost[] = "local-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kChromeUIUntrustedNewTabPageBackgroundUrl[] =
    "chrome-untrusted://new-tab-page/background.jpg";
const char kChromeUIUntrustedNewTabPageBackgroundFilename[] = "background.jpg";

const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchScheme[] = "chrome-search";

const char kChromeUIUntrustedNewTabPageUrl[] =
    "chrome-untrusted://new-tab-page/";

const char kChromiumProjectURL[] = "https://www.ch40m1um.qjz9zk/";

const char kContentSettingsExceptionsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_manage_exceptions";

const char kCookiesSettingsHelpCenterURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=cpn_cookies";

const char kCrashReasonURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=e_awsnap";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=e_awsnap";
#endif

const char kCrashReasonFeedbackDisplayedURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=e_awsnap_rl";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=e_awsnap_rl";
#endif

const char kDoNotTrackLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_do_not_track";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_do_not_track";
#endif

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_download_errors";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ib_download_blocked";

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_settings_api_extension";

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";

const char kFlashDeprecationLearnMoreURL[] =
    "https://blog.ch40m1um.qjz9zk/2017/07/so-long-and-thanks-for-all-flash.html";

const char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.9oo91e.qjz9zk/activitycontrols/search";

const char kGoogleAccountActivityControlsURLInPrivacyGuide[] =
    "https://myaccount.9oo91e.qjz9zk/activitycontrols/"
    "search&utm_source=chrome&utm_medium=privacy-guide";

const char kGoogleAccountLanguagesURL[] =
    "https://myaccount.9oo91e.qjz9zk/language";

const char kGoogleAccountURL[] = "https://myaccount.9oo91e.qjz9zk";

const char kGoogleAccountChooserURL[] =
    "https://accounts.9oo91e.qjz9zk/AccountChooser";

const char kGoogleAccountDeviceActivityURL[] =
    "https://myaccount.9oo91e.qjz9zk/device-activity?utm_source=chrome";

const char kGooglePasswordManagerURL[] = "https://passwords.9oo91e.qjz9zk";

const char kGooglePhotosURL[] = "https://photos.9oo91e.qjz9zk";

const char kLearnMoreReportingURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_usagestat";

const char kManagedUiLearnMoreUrl[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=is_chrome_managed";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=is_chrome_managed";
#endif

const char kMixedContentDownloadBlockingLearnMoreUrl[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=mixed_content_downloads";

const char kMyActivityUrlInClearBrowsingData[] =
    "https://myactivity.9oo91e.qjz9zk/myactivity?utm_source=chrome_cbd";

const char kOmniboxLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_omnibox";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=ui_security_indicator";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_security_indicator";
#endif

const char kPasswordCheckLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/"
    "?p=settings_password#leak_detection_privacy";
#else
    "https://support.9oo91e.qjz9zk/chrome/"
    "?p=settings_password#leak_detection_privacy";
#endif

const char kPasswordGenerationLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/7570435";

const char kPasswordManagerLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_password";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_password";
#endif

const char kPaymentMethodsURL[] =
    "https://pay.9oo91e.qjz9zk/payments/"
    "home?utm_source=chrome&utm_medium=settings&utm_campaign=chrome-payment#"
    "paymentMethods";

const char kPaymentMethodsLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#else
    "https://support.9oo91e.qjz9zk/chrome/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#endif

const char kPrivacyLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_privacy";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_privacy";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_remove_non_cws_extensions";

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ui_reset_settings";

const char kSafeBrowsingHelpCenterURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=cpn_safe_browsing";

const char kSafetyTipHelpCenterURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=safety_tip";

const char kSearchHistoryUrlInClearBrowsingData[] =
    "https://myactivity.9oo91e.qjz9zk/product/search?utm_source=chrome_cbd";

const char kSeeMoreSecurityTipsURL[] =
    "https://support.9oo91e.qjz9zk/accounts/answer/32040";

const char kSettingsSearchHelpURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_search_help";

const char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=syncgoogleservices";

const char kSyncEncryptionHelpURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_encryption";
#else
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_sync_error";

const char kSyncGoogleDashboardURL[] =
    "https://www.9oo91e.qjz9zk/settings/chrome/sync";

const char kSyncLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=settings_sign_in";

const char kSigninInterceptManagedDisclaimerLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/a/?p=profile_separation";

#if !BUILDFLAG(IS_ANDROID)
const char kSyncTrustedVaultOptInURL[] =
    "https://passwords.9oo91e.qjz9zk/encryption/enroll?"
    "utm_source=chrome&utm_medium=desktop&utm_campaign=encryption_enroll";
#endif

const char kSyncTrustedVaultLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/accounts?p=settings_password_ode";

const char kUpgradeHelpCenterBaseURL[] =
    "https://support.9oo91e.qjz9zk/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

const char kWhoIsMyAdministratorHelpURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=your_administrator";

const char kCwsEnhancedSafeBrowsingLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome?p=cws_enhanced_safe_browsing";

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
const char kEnhancedPlaybackNotificationLearnMoreURL[] =
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.9oo91e.qjz9zk/chromebook/?p=enhanced_playback";
#elif BUILDFLAG(IS_ANDROID)
// Keep in sync with chrome/browser/ui/android/strings/android_chrome_strings.grd
    "https://support.9oo91e.qjz9zk/chrome/?p=mobile_protected_content";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
const char kChromeOSDefaultMailtoHandler[] =
    "https://mail.9oo91e.qjz9zk/mail/?extsrc=mailto&amp;url=%s";
const char kChromeOSDefaultWebcalHandler[] =
    "https://www.9oo91e.qjz9zk/calendar/render?cid=%s";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccountManagerLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=google_accounts";

const char kAccountRecoveryURL[] =
    "https://accounts.9oo91e.qjz9zk/signin/recovery";

const char kAddNewUserURL[] =
    "https://www.9oo91e.qjz9zk/chromebook/howto/add-another-account";

const char kAndroidAppsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=playapps";

const char kArcAdbSideloadingLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=develop_android_apps";

const char kArcExternalStorageLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=open_files";

const char kArcPrivacyPolicyURLPath[] = "arc/privacy_policy";

const char kArcTermsURLPath[] = "arc/terms";

// TODO(crbug.com/1010321): Remove 'm100' prefix from link once Bluetooth Revamp
// has shipped.
const char kBluetoothPairingLearnMoreUrl[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=bluetooth_revamp_m100";

const char kChromeAccessibilityHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/topic/6323347";

const char kChromeOSAssetHost[] = "chromeos-asset";
const char kChromeOSAssetPath[] = "/usr/share/chromeos-assets/";

const char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

// TODO(carpenterr): Have a solution for plink mapping in Help App.
// The magic numbers in this url are the topic and article ids currently
// required to navigate directly to a help article in the Help App.
const char kChromeOSGestureEducationHelpURL[] =
    "chrome://help-app/help/sub/3399710/id/9739838";

const char kChromePaletteHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=stylus_help";

const char kCupsPrintLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=chromebook_printing";

const char kCupsPrintPPDLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=printing_advancedconfigurations";

const char kEasyUnlockLearnMoreUrl[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=smart_lock";

const char kEchoLearnMoreURL[] =
    "chrome://help-app/help/sub/3399709/id/2703646";

const char kArcTermsPathFormat[] = "arc_tos/%s/terms.html";

const char kArcPrivacyPolicyPathFormat[] = "arc_tos/%s/privacy_policy.pdf";

const char kEolNotificationURL[] = "https://www.9oo91e.qjz9zk/chromebook/older/";

const char kAutoUpdatePolicyURL[] =
    "http://support.9oo91e.qjz9zk/chrome/a?p=auto-update-policy";

const char kGoogleNameserversLearnMoreURL[] =
    "https://developers.9oo91e.qjz9zk/speed/public-dns";

const char kInstantTetheringLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=instant_tethering";

const char kKerberosAccountsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=kerberos_accounts";

const char kLanguageSettingsLearnMoreUrl[] =
    "https://support.9oo91e.qjz9zk/chromebook/answer/1059490";

const char kLanguagePacksLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=language_packs";

const char kLearnMoreEnterpriseURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=managed";

const char kLinuxAppsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=chromebook_linuxapps";

const char kNaturalScrollHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=simple_scrolling";

const char kHapticFeedbackHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=haptic_feedback_m100";

const char kOemEulaURLPath[] = "oem";

const char kGoogleEulaOnlineURLPath[] =
    "https://policies.9oo91e.qjz9zk/terms/embedded?hl=%s";

const char kCrosEulaOnlineURLPath[] =
    "https://www.9oo91e.qjz9zk/intl/%s/chrome/terms/";

const char kOsSettingsSearchHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=settings_search_help";

const char kPeripheralDataAccessHelpURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=connect_thblt_usb4_accy";

const char kTPMFirmwareUpdateLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=tpm_update";

const char kTimeZoneSettingsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=chromebook_timezone&hl=%s";

const char kSmartPrivacySettingsLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=screen_privacy_m100";

const char kSmbSharesLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=network_file_shares";

const char kSuggestedContentLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=explorecontent";

const char kTabletModeGesturesLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=tablet_mode_gestures";

const char kWifiSyncLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=wifisync";

const char kWifiHiddenNetworkURL[] =
    "http://support.9oo91e.qjz9zk/chromebook?p=hidden_networks";

const char kNearbyShareLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=nearby_share";

extern const char kNearbyShareManageContactsURL[] =
    "https://contacts.9oo91e.qjz9zk";

extern const char kFingerprintLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook?p=chromebook_fingerprint";

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
const char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/answer/1331549";

const char kMacOsObsoleteURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=unsupported_mac";
#endif

#if BUILDFLAG(IS_WIN)
const char kChromeCleanerLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=chrome_cleanup_tool";

const char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

const char kChromeSyncLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/165139";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kOutdatedPluginLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=ib_outdated_plugin";
#endif

// TODO (b/184137843): Use real link to phone hub notifications and apps access.
const char kPhoneHubPermissionLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chromebook/?p=multidevice";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
const char kChromeAppsDeprecationLearnMoreURL[] =
    "https://support.9oo91e.qjz9zk/chrome/?p=chrome_app_deprecation";
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// TODO(b/1339340): add help center link when help center link is created.
const char kChromeRootStoreSettingsHelpCenterURL[] =
    "https://chromium.9oo91esource.qjz9zk/chromium/src/+/main/net/data/ssl/"
    "chrome_root_store/root_store.md";
#endif

}  // namespace chrome
