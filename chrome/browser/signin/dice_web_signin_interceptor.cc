// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/chrome_colors/generated_colors_info.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/themes/autogenerated_theme_util.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kProfileCreationInterceptionDeclinedPref[] =
    "signin.ProfileCreationInterceptionDeclinedPref";

void RecordSigninInterceptionHeuristicOutcome(
    SigninInterceptionHeuristicOutcome outcome) {
  base::UmaHistogramEnumeration("Signin.Intercept.HeuristicOutcome", outcome);
}

// Helper function to return the primary account info. The returned info is
// empty if there is no primary account, and non-empty otherwise. Extended
// fields may be missing if they are not available.
AccountInfo GetPrimaryAccountInfo(signin::IdentityManager* manager) {
  CoreAccountInfo primary_core_account_info =
      manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_core_account_info.IsEmpty())
    return AccountInfo();

  AccountInfo primary_account_info =
      manager->FindExtendedAccountInfo(primary_core_account_info);

  if (!primary_account_info.IsEmpty())
    return primary_account_info;

  // Return an AccountInfo without extended fields, based on the core info.
  AccountInfo account_info;
  account_info.gaia = primary_core_account_info.gaia;
  account_info.email = primary_core_account_info.email;
  account_info.account_id = primary_core_account_info.account_id;
  return account_info;
}

bool HasNoBrowser(content::WebContents* web_contents) {
  return chrome::FindBrowserWithWebContents(web_contents) == nullptr;
}

}  // namespace

ScopedDiceWebSigninInterceptionBubbleHandle::
    ~ScopedDiceWebSigninInterceptionBubbleHandle() = default;

bool SigninInterceptionHeuristicOutcomeIsSuccess(
    SigninInterceptionHeuristicOutcome outcome) {
  return outcome == SigninInterceptionHeuristicOutcome::kInterceptEnterprise ||
         outcome == SigninInterceptionHeuristicOutcome::kInterceptMultiUser ||
         outcome ==
             SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch ||
         outcome ==
             SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced ||
         outcome == SigninInterceptionHeuristicOutcome::
                        kInterceptEnterpriseForcedProfileSwitch;
}

DiceWebSigninInterceptor::Delegate::BubbleParameters::BubbleParameters(
    SigninInterceptionType interception_type,
    AccountInfo intercepted_account,
    AccountInfo primary_account,
    SkColor profile_highlight_color,
    bool show_guest_option,
    bool show_link_data_option,
    bool show_managed_disclaimer)
    : interception_type(interception_type),
      intercepted_account(intercepted_account),
      primary_account(primary_account),
      profile_highlight_color(profile_highlight_color),
      show_guest_option(show_guest_option),
      show_link_data_option(show_link_data_option),
      show_managed_disclaimer(show_managed_disclaimer) {}

DiceWebSigninInterceptor::Delegate::BubbleParameters::BubbleParameters(
    const BubbleParameters& copy) = default;

DiceWebSigninInterceptor::Delegate::BubbleParameters&
DiceWebSigninInterceptor::Delegate::BubbleParameters::operator=(
    const BubbleParameters&) = default;

DiceWebSigninInterceptor::Delegate::BubbleParameters::~BubbleParameters() =
    default;

DiceWebSigninInterceptor::DiceWebSigninInterceptor(
    Profile* profile,
    std::unique_ptr<Delegate> delegate)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      delegate_(std::move(delegate)) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(delegate_);
}

DiceWebSigninInterceptor::~DiceWebSigninInterceptor() = default;

// static
void DiceWebSigninInterceptor::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kProfileCreationInterceptionDeclinedPref);
  registry->RegisterBooleanPref(prefs::kSigninInterceptionEnabled, true);
  registry->RegisterStringPref(prefs::kManagedAccountsSigninRestriction,
                               std::string());
  registry->RegisterBooleanPref(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
}

absl::optional<SigninInterceptionHeuristicOutcome>
DiceWebSigninInterceptor::GetHeuristicOutcome(
    bool is_new_account,
    bool is_sync_signin,
    const std::string& email,
    const ProfileAttributesEntry** entry) const {
  bool signin_interception_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kSigninInterceptionEnabled);

  if (is_sync_signin) {
    // Do not intercept signins from the Sync startup flow.
    // Note: |is_sync_signin| is an approximation, and in rare cases it may be
    // true when in fact the signin was not a sync signin. In this case the
    // interception is missed.
    return SigninInterceptionHeuristicOutcome::kAbortSyncSignin;
  }

  auto enforce_enterprise_separation = EnterpriseSeparationMaybeRequired(
      email, is_new_account,
      /*managed_account_profile_level_signin_restriction=*/absl::nullopt);

  // If we do not have all the information to enforce or not enterprise profile
  // separation, return `absl::nullopt` so that we can try and get more info on
  // the intercepted account.
  if (!enforce_enterprise_separation)
    return absl::nullopt;

  if (!enforce_enterprise_separation.value()) {
    // If interception is disabled abort, unless we need to enforce enterprise
    // profile separation.
    if (!signin_interception_enabled)
      return SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled;
    // Do not intercept reauth.
    if (!is_new_account)
      return SigninInterceptionHeuristicOutcome::kAbortAccountNotNew;
  }

  const ProfileAttributesEntry* switch_to_entry = ShouldShowProfileSwitchBubble(
      email,
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  if (switch_to_entry) {
    if (entry)
      *entry = switch_to_entry;
    if (enforce_enterprise_separation.value()) {
      return SigninInterceptionHeuristicOutcome::
          kInterceptEnterpriseForcedProfileSwitch;
    }
    DCHECK(is_new_account) << "Reauths were already handled above";
    return SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch;
  }

  if (enforce_enterprise_separation.value())
    return SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced;

  DCHECK(signin_interception_enabled && !enforce_enterprise_separation.value());

  // From this point the remaining possible interceptions involve creating a new
  // profile.
  if (!profiles::IsProfileCreationAllowed()) {
    return SigninInterceptionHeuristicOutcome::kAbortProfileCreationDisallowed;
  }

  std::vector<CoreAccountInfo> accounts_in_chrome =
      identity_manager_->GetAccountsWithRefreshTokens();
  if (accounts_in_chrome.size() == 0 ||
      (accounts_in_chrome.size() == 1 &&
       gaia::AreEmailsSame(email, accounts_in_chrome[0].email))) {
    // Enterprise and multi-user bubbles are only shown if there are multiple
    // accounts. The intercepted account may not be added to chrome yet.
    return SigninInterceptionHeuristicOutcome::kAbortSingleAccount;
  }

  if (HasUserDeclinedProfileCreation(email)) {
    return SigninInterceptionHeuristicOutcome::
        kAbortUserDeclinedProfileForAccount;
  }

  return absl::nullopt;
}

void DiceWebSigninInterceptor::MaybeInterceptWebSignin(
    content::WebContents* web_contents,
    CoreAccountId account_id,
    bool is_new_account,
    bool is_sync_signin) {
  if (is_interception_in_progress_) {
    // Multiple concurrent interceptions are not supported.
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortInterceptInProgress);
    return;
  }

  if (!web_contents) {
    // The tab has been closed (typically during the token exchange, which may
    // take some time).
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortTabClosed);
    return;
  }

  if (HasNoBrowser(web_contents)) {
    // Do not intercept from the profile creation flow.
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortNoBrowser);
    return;
  }

  // Do not show the interception UI if a password update is required: both
  // bubbles cannot be shown at the same time and the password update is more
  // important.
  ChromePasswordManagerClient* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(web_contents);
  if (password_manager_client && password_manager_client->GetPasswordManager()
                                     ->IsFormManagerPendingPasswordUpdate()) {
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortPasswordUpdatePending);
    return;
  }

  ManagePasswordsUIController* password_controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  if (password_controller &&
      password_controller->GetState() ==
          password_manager::ui::State::PENDING_PASSWORD_UPDATE_STATE) {
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortPasswordUpdate);
    return;
  }

  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  DCHECK(!account_info.IsEmpty()) << "Intercepting unknown account.";
  const ProfileAttributesEntry* entry = nullptr;
  absl::optional<SigninInterceptionHeuristicOutcome> heuristic_outcome =
      GetHeuristicOutcome(is_new_account, is_sync_signin, account_info.email,
                          &entry);
  account_id_ = account_id;
  is_interception_in_progress_ = true;
  new_account_interception_ = is_new_account;
  web_contents_ = web_contents->GetWeakPtr();

  if (heuristic_outcome &&
      !SigninInterceptionHeuristicOutcomeIsSuccess(*heuristic_outcome)) {
    RecordSigninInterceptionHeuristicOutcome(*heuristic_outcome);
    Reset();
    return;
  }

  account_info_fetch_start_time_ = base::TimeTicks::Now();
  if (account_info.IsValid()) {
    OnExtendedAccountInfoUpdated(account_info);
  } else {
    on_account_info_update_timeout_.Reset(base::BindOnce(
        &DiceWebSigninInterceptor::OnExtendedAccountInfoFetchTimeout,
        base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, on_account_info_update_timeout_.callback(),
        base::Seconds(5));
    account_info_update_observation_.Observe(identity_manager_.get());
  }
}

void DiceWebSigninInterceptor::CreateBrowserAfterSigninInterception(
    CoreAccountId account_id,
    content::WebContents* intercepted_contents,
    std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> bubble_handle,
    bool is_new_profile,
    SigninInterceptionType interception_type) {
  DCHECK(!session_startup_helper_);
  DCHECK(bubble_handle);
  interception_bubble_handle_ = std::move(bubble_handle);
  account_id_ = account_id;
  interception_type_ = interception_type;
  session_startup_helper_ =
      std::make_unique<DiceInterceptedSessionStartupHelper>(
          profile_, is_new_profile, account_id, intercepted_contents);
  session_startup_helper_->Startup(
      base::BindOnce(&DiceWebSigninInterceptor::OnNewBrowserCreated,
                     base::Unretained(this), is_new_profile));
}

void DiceWebSigninInterceptor::Shutdown() {
  if (is_interception_in_progress_ && !was_interception_ui_displayed_) {
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortShutdown);
  }
  Reset();
}

void DiceWebSigninInterceptor::Reset() {
  web_contents_ = nullptr;
  account_info_update_observation_.Reset();
  on_account_info_update_timeout_.Cancel();
  is_interception_in_progress_ = false;
  account_id_ = CoreAccountId();
  new_account_interception_ = false;
  intercepted_account_management_accepted_ = false;
  interception_type_ = absl::nullopt;
  dice_signed_in_profile_creator_.reset();
  was_interception_ui_displayed_ = false;
  account_info_fetch_start_time_ = base::TimeTicks();
  profile_creation_start_time_ = base::TimeTicks();
  interception_bubble_handle_.reset();
  on_intercepted_account_level_policy_value_timeout_.Cancel();
  account_level_signin_restriction_policy_fetcher_.reset();
  intercepted_account_level_policy_value_.reset();
}

const ProfileAttributesEntry*
DiceWebSigninInterceptor::ShouldShowProfileSwitchBubble(
    const std::string& intercepted_email,
    ProfileAttributesStorage* profile_attribute_storage) const {
  // Check if there is already an existing profile with this account.
  base::FilePath profile_path = profile_->GetPath();
  for (const auto* entry :
       profile_attribute_storage->GetAllProfilesAttributes()) {
    if (entry->GetPath() == profile_path)
      continue;
    if (gaia::AreEmailsSame(intercepted_email,
                            base::UTF16ToUTF8(entry->GetUserName()))) {
      return entry;
    }
  }
  return nullptr;
}

bool DiceWebSigninInterceptor::ShouldEnforceEnterpriseProfileSeparation(
    const AccountInfo& intercepted_account_info) const {
  DCHECK(intercepted_account_info.IsValid());

  if (!signin_util::ProfileSeparationEnforcedByPolicy(
          profile_,
          intercepted_account_level_policy_value_.value_or(std::string()))) {
    return false;
  }
  if (new_account_interception_)
    return intercepted_account_info.IsManaged();

  CoreAccountInfo primary_core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  // In case of re-auth, do not show the enterprise separation dialog if the
  // user already consented to enterprise management.
  if (!new_account_interception_ && primary_core_account_info.account_id ==
                                        intercepted_account_info.account_id) {
    return !chrome::enterprise_util::UserAcceptedAccountManagement(profile_);
  }

  return false;
}

bool DiceWebSigninInterceptor::ShouldShowEnterpriseDialog(
    const AccountInfo& intercepted_account_info) const {
  DCHECK(intercepted_account_info.IsValid());

  if (!base::FeatureList::IsEnabled(
          kShowEnterpriseDialogForAllManagedAccountsSignin)) {
    return false;
  }

  // Check if the intercepted account is managed.
  if (!intercepted_account_info.IsManaged())
    return false;

  CoreAccountInfo primary_core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (primary_core_account_info.account_id ==
          intercepted_account_info.account_id &&
      !chrome::enterprise_util::UserAcceptedAccountManagement(profile_)) {
    return true;
  }

  return false;
}

bool DiceWebSigninInterceptor::ShouldShowEnterpriseBubble(
    const AccountInfo& intercepted_account_info) const {
  DCHECK(intercepted_account_info.IsValid());
  // Check if the intercepted account or the primary account is managed.
  CoreAccountInfo primary_core_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (primary_core_account_info.IsEmpty() ||
      primary_core_account_info.account_id ==
          intercepted_account_info.account_id) {
    return false;
  }

  if (intercepted_account_info.IsManaged())
    return true;

  return identity_manager_->FindExtendedAccountInfo(primary_core_account_info)
      .IsManaged();
}

bool DiceWebSigninInterceptor::ShouldShowMultiUserBubble(
    const AccountInfo& intercepted_account_info) const {
  DCHECK(intercepted_account_info.IsValid());
  if (identity_manager_->GetAccountsWithRefreshTokens().size() <= 1u)
    return false;
  // Check if the account has the same name as another account in the profile.
  for (const auto& account_info :
       identity_manager_->GetExtendedAccountInfoForAccountsWithRefreshToken()) {
    if (account_info.account_id == intercepted_account_info.account_id)
      continue;
    // Case-insensitve comparison supporting non-ASCII characters.
    if (base::i18n::FoldCase(base::UTF8ToUTF16(account_info.given_name)) ==
        base::i18n::FoldCase(
            base::UTF8ToUTF16(intercepted_account_info.given_name))) {
      return false;
    }
  }
  return true;
}

void DiceWebSigninInterceptor::ShowSigninInterceptionBubble(
    const Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  interception_bubble_handle_ = delegate_->ShowSigninInterceptionBubble(
      web_contents_.get(), bubble_parameters, std::move(callback));
  was_interception_ui_displayed_ = true;
  interception_type_ = bubble_parameters.interception_type;
}

void DiceWebSigninInterceptor::OnInterceptionReadyToBeProcessed(
    const AccountInfo& info) {
  DCHECK_EQ(info.account_id, account_id_);
  DCHECK(info.IsValid());

  absl::optional<SigninInterceptionType> interception_type;

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  SkColor profile_color = GenerateNewProfileColor(entry).color;

  const ProfileAttributesEntry* switch_to_entry = ShouldShowProfileSwitchBubble(
      info.email,
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  bool force_profile_separation =
      ShouldEnforceEnterpriseProfileSeparation(info);

  // This is normally checked in GetHeuristicOutcome() but that's not possible
  // for enterprise accounts where we need to wait for policies, that is why we
  // double check here.
  if (!force_profile_separation && HasUserDeclinedProfileCreation(info.email)) {
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::
            kAbortUserDeclinedProfileForAccount);
    Reset();
    return;
  }

  bool show_link_data_option = false;

  if (force_profile_separation) {
    if (switch_to_entry) {
      interception_type = SigninInterceptionType::kProfileSwitchForced;
      RecordSigninInterceptionHeuristicOutcome(
          SigninInterceptionHeuristicOutcome::
              kInterceptEnterpriseForcedProfileSwitch);
    } else if (!new_account_interception_ &&
               identity_manager_->GetPrimaryAccountId(
                   signin::ConsentLevel::kSync) == info.account_id) {
      // In case of a reauth of an account that already had sync enabled,
      // the user already accepted to use a managed profile. Simply update that
      // fact.
      chrome::enterprise_util::SetUserAcceptedAccountManagement(profile_, true);
      RecordSigninInterceptionHeuristicOutcome(
          SigninInterceptionHeuristicOutcome::kAbortAccountNotNew);
      Reset();
      return;
    } else {
      interception_type = SigninInterceptionType::kEnterpriseForced;
      auto primary_account_id =
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
      show_link_data_option =
          (primary_account_id.empty() ||
           primary_account_id == info.account_id) &&
          signin_util::
              ProfileSeparationAllowsKeepingUnmanagedBrowsingDataInManagedProfile(
                  profile_, intercepted_account_level_policy_value_.value_or(
                                std::string()));
      RecordSigninInterceptionHeuristicOutcome(
          SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
    }
  } else if (ShouldShowEnterpriseDialog(info)) {
    interception_type = SigninInterceptionType::kEnterpriseAcceptManagement;
    show_link_data_option = true;
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
  } else if (!profile_->GetPrefs()->GetBoolean(
                 prefs::kSigninInterceptionEnabled)) {
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
    Reset();
    return;
  } else if (switch_to_entry) {
    // Propose account switching if we skipped in GetHeuristicOutcome because we
    // returned a nullptr to get more information about forced enterprise
    // profile separation.
    interception_type = SigninInterceptionType::kProfileSwitch;
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  } else if (ShouldShowEnterpriseBubble(info)) {
    interception_type = SigninInterceptionType::kEnterprise;
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
  } else if (ShouldShowMultiUserBubble(info)) {
    interception_type = SigninInterceptionType::kMultiUser;
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kInterceptMultiUser);
  }

  if (!interception_type) {
    // Signin should not be intercepted.
    RecordSigninInterceptionHeuristicOutcome(
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
    Reset();
    return;
  }

  bool show_managed_disclaimer =
      *interception_type != SigninInterceptionType::kProfileSwitch &&
      base::FeatureList::IsEnabled(kSigninInterceptBubbleV2) &&
      (info.IsManaged() ||
       policy::ManagementServiceFactory::GetForPlatform()->IsManaged());

  Delegate::BubbleParameters bubble_parameters(
      *interception_type, info, GetPrimaryAccountInfo(identity_manager_),
      GetAutogeneratedThemeColors(profile_color).frame_color,
      /*show_guest_option=*/false, show_link_data_option,
      show_managed_disclaimer);

  base::OnceCallback<void(SigninInterceptionResult)> callback;
  switch (*interception_type) {
    case SigninInterceptionType::kProfileSwitch:
    case SigninInterceptionType::kProfileSwitchForced:
      callback = base::BindOnce(
          &DiceWebSigninInterceptor::OnProfileSwitchChoice,
          base::Unretained(this), info.email, switch_to_entry->GetPath());
      break;
    case SigninInterceptionType::kEnterpriseForced:
    case SigninInterceptionType::kEnterpriseAcceptManagement:
      callback = base::BindOnce(
          &DiceWebSigninInterceptor::OnEnterpriseProfileCreationResult,
          base::Unretained(this), info, profile_color);
      break;
    case SigninInterceptionType::kEnterprise:
    case SigninInterceptionType::kMultiUser:
      callback =
          base::BindOnce(&DiceWebSigninInterceptor::OnProfileCreationChoice,
                         base::Unretained(this), info, profile_color);
      break;
  }
  ShowSigninInterceptionBubble(bubble_parameters, std::move(callback));
}

void DiceWebSigninInterceptor::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id != account_id_)
    return;
  if (!info.IsValid())
    return;

  account_info_update_observation_.Reset();
  on_account_info_update_timeout_.Cancel();
  base::UmaHistogramTimes(
      "Signin.Intercept.AccountInfoFetchDuration",
      base::TimeTicks::Now() - account_info_fetch_start_time_);

  // Fetch the ManagedAccountsSigninRestriction policy value for the intercepted
  // account with a timeout.
  if (!EnterpriseSeparationMaybeRequired(
           info.email, new_account_interception_,
           intercepted_account_level_policy_value_)
           .has_value()) {
    FetchAccountLevelSigninRestrictionForInterceptedAccount(
        info, base::BindOnce(
                  &DiceWebSigninInterceptor::
                      OnAccountLevelManagedAccountsSigninRestrictionReceived,
                  base::Unretained(this), /*timed_out=*/false, info));
    return;
  }

  OnInterceptionReadyToBeProcessed(info);
}

void DiceWebSigninInterceptor::OnExtendedAccountInfoFetchTimeout() {
  RecordSigninInterceptionHeuristicOutcome(
      SigninInterceptionHeuristicOutcome::kAbortAccountInfoTimeout);
  Reset();
}

void DiceWebSigninInterceptor::OnProfileCreationChoice(
    const AccountInfo& account_info,
    SkColor profile_color,
    SigninInterceptionResult create) {
  if (create != SigninInterceptionResult::kAccepted &&
      create != SigninInterceptionResult::kAcceptedWithGuest) {
    if (create == SigninInterceptionResult::kDeclined)
      RecordProfileCreationDeclined(account_info.email);
    Reset();
    return;
  }

  DCHECK(interception_bubble_handle_);
  profile_creation_start_time_ = base::TimeTicks::Now();
  std::u16string profile_name;
  profile_name = profiles::GetDefaultNameForNewSignedInProfile(account_info);

  DCHECK(!dice_signed_in_profile_creator_);
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_id_, profile_name,
          profiles::GetPlaceholderAvatarIndex(),
          create == SigninInterceptionResult::kAcceptedWithGuest,
          base::BindOnce(&DiceWebSigninInterceptor::OnNewSignedInProfileCreated,
                         base::Unretained(this), profile_color));
}

void DiceWebSigninInterceptor::OnProfileSwitchChoice(
    const std::string& email,
    const base::FilePath& profile_path,
    SigninInterceptionResult switch_profile) {
  if (switch_profile != SigninInterceptionResult::kAccepted) {
    Reset();
    return;
  }

  DCHECK(interception_bubble_handle_);
  DCHECK(!dice_signed_in_profile_creator_);
  profile_creation_start_time_ = base::TimeTicks::Now();
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_id_, profile_path,
          base::BindOnce(&DiceWebSigninInterceptor::OnNewSignedInProfileCreated,
                         base::Unretained(this), absl::nullopt));
}

void DiceWebSigninInterceptor::OnNewSignedInProfileCreated(
    absl::optional<SkColor> profile_color,
    Profile* new_profile) {
  DCHECK(dice_signed_in_profile_creator_);
  dice_signed_in_profile_creator_.reset();

  if (!new_profile) {
    Reset();
    return;
  }

  // The profile color is defined only when the profile has just been created
  // (with interception type kMultiUser or kEnterprise). If the profile is not
  // new (kProfileSwitch) or if it is a guest profile, then the color is not
  // updated.
  bool is_new_profile = profile_color.has_value();
  if (is_new_profile) {
    base::UmaHistogramTimes(
        "Signin.Intercept.ProfileCreationDuration",
        base::TimeTicks::Now() - profile_creation_start_time_);
    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_USER_SIGNIN_INTERCEPTION);
    // TODO(https://crbug.com/1225171): Remove the condition if Guest mode
    // option is removed.
    if (!new_profile->IsGuestSession()) {
      // Apply the new color to the profile.
      ThemeServiceFactory::GetForProfile(new_profile)
          ->BuildAutogeneratedThemeFromColor(*profile_color);
    }
  } else {
    base::UmaHistogramTimes(
        "Signin.Intercept.ProfileSwitchDuration",
        base::TimeTicks::Now() - profile_creation_start_time_);
  }

  chrome::enterprise_util::SetUserAcceptedAccountManagement(
      new_profile, intercepted_account_management_accepted_);

  // Work is done in this profile, the flow continues in the
  // DiceWebSigninInterceptor that is attached to the new profile.
  // We pass relevant parameters from this instance to the new one.
  DiceWebSigninInterceptorFactory::GetForProfile(new_profile)
      ->CreateBrowserAfterSigninInterception(
          account_id_, web_contents_.get(),
          std::move(interception_bubble_handle_), is_new_profile,
          *interception_type_);
  Reset();
}

void DiceWebSigninInterceptor::OnEnterpriseProfileCreationResult(
    const AccountInfo& account_info,
    SkColor profile_color,
    SigninInterceptionResult create) {
  signin_util::RecordEnterpriseProfileCreationUserChoice(
      /*enforced_by_policy=*/signin_util::ProfileSeparationEnforcedByPolicy(
          profile_,
          intercepted_account_level_policy_value_.value_or(std::string())),
      /*created=*/create == SigninInterceptionResult::kAccepted);

  // Make sure existing account is a non-signed in profile.
  if (create == SigninInterceptionResult::kAccepted) {
    intercepted_account_management_accepted_ = true;
    // In case of a reauth if there was no consent for management, do not create
    // a new profile.
    if (!new_account_interception_ &&
        GetPrimaryAccountInfo(identity_manager_).account_id ==
            account_info.account_id) {
      chrome::enterprise_util::SetUserAcceptedAccountManagement(
          profile_, intercepted_account_management_accepted_);
      Reset();
    } else {
      OnProfileCreationChoice(account_info, profile_color,
                              SigninInterceptionResult::kAccepted);
    }
  } else if (create == SigninInterceptionResult::kAcceptedWithExistingProfile) {
    intercepted_account_management_accepted_ = true;
    DCHECK_EQ(GetPrimaryAccountInfo(identity_manager_).account_id,
              account_info.account_id);
    chrome::enterprise_util::SetUserAcceptedAccountManagement(
        profile_, intercepted_account_management_accepted_);
    Reset();
  } else {
    DCHECK_EQ(SigninInterceptionResult::kDeclined, create)
        << "The user can only accept or decline";
    OnProfileCreationChoice(account_info, profile_color,
                            SigninInterceptionResult::kDeclined);
    auto* accounts_mutator = identity_manager_->GetAccountsMutator();
    accounts_mutator->RemoveAccount(
        account_info.account_id,
        signin_metrics::SourceForRefreshTokenOperation::
            kTurnOnSyncHelper_Abort);
  }
}

void DiceWebSigninInterceptor::OnNewBrowserCreated(bool is_new_profile) {
  DCHECK(interception_bubble_handle_);
  interception_bubble_handle_.reset();  // Close the bubble now.
  session_startup_helper_.reset();

  // TODO(https://crbug.com/1225171): Remove |IsGuestSession| if Guest option is
  // no more supported.
  if (!is_new_profile || profile_->IsGuestSession())
    return;

  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  DCHECK(browser);
  delegate_->ShowFirstRunExperienceInNewProfile(browser, account_id_,
                                                *interception_type_);
}

// static
std::string DiceWebSigninInterceptor::GetPersistentEmailHash(
    const std::string& email) {
  int hash = base::PersistentHash(
                 gaia::CanonicalizeEmail(gaia::SanitizeEmail(email))) &
             0xFF;
  return base::StringPrintf("email_%i", hash);
}

void DiceWebSigninInterceptor::RecordProfileCreationDeclined(
    const std::string& email) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              kProfileCreationInterceptionDeclinedPref);
  std::string key = GetPersistentEmailHash(email);
  absl::optional<int> declined_count = update->FindIntKey(key);
  update->SetIntKey(key, declined_count.value_or(0) + 1);
}

bool DiceWebSigninInterceptor::HasUserDeclinedProfileCreation(
    const std::string& email) const {
  const base::Value::Dict& pref_data = profile_->GetPrefs()->GetValueDict(
      kProfileCreationInterceptionDeclinedPref);
  absl::optional<int> declined_count =
      pref_data.FindInt(GetPersistentEmailHash(email));
  // Check if the user declined 2 times.
  constexpr int kMaxProfileCreationDeclinedCount = 2;
  return declined_count &&
         declined_count.value() >= kMaxProfileCreationDeclinedCount;
}

void DiceWebSigninInterceptor::
    FetchAccountLevelSigninRestrictionForInterceptedAccount(
        const AccountInfo& account_info,
        base::OnceCallback<void(const std::string&)> callback) {
  if (intercepted_account_level_policy_value_fetch_result_for_testing_
          .has_value()) {
    std::move(callback).Run(
        intercepted_account_level_policy_value_fetch_result_for_testing_
            .value());
    return;
  }

  account_level_signin_restriction_policy_fetcher_ =
      std::make_unique<policy::UserCloudSigninRestrictionPolicyFetcher>(
          g_browser_process->browser_policy_connector(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory());
  account_level_signin_restriction_policy_fetcher_
      ->GetManagedAccountsSigninRestriction(
          identity_manager_, account_info.account_id, std::move(callback));

  on_intercepted_account_level_policy_value_timeout_.Reset(base::BindOnce(
      &DiceWebSigninInterceptor::
          OnAccountLevelManagedAccountsSigninRestrictionReceived,
      base::Unretained(this), /*timed_out=*/true, account_info, std::string()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, on_intercepted_account_level_policy_value_timeout_.callback(),
      base::Seconds(5));
}

void DiceWebSigninInterceptor::
    OnAccountLevelManagedAccountsSigninRestrictionReceived(
        bool timed_out,
        const AccountInfo& account_info,
        const std::string& signin_restriction) {
  if (timed_out) {
    DCHECK(signin_restriction.empty())
        << "There should be no signin restriction at the account level in case "
           "of a timeout";
  } else {
    on_intercepted_account_level_policy_value_timeout_.Cancel();
  }
  intercepted_account_level_policy_value_ = signin_restriction;
  OnInterceptionReadyToBeProcessed(account_info);
}

absl::optional<bool>
DiceWebSigninInterceptor::EnterpriseSeparationMaybeRequired(
    const std::string& email,
    bool is_new_account_interception,
    absl::optional<std::string>
        managed_account_profile_level_signin_restriction) const {
  // No enterprise separation required for consumer accounts.
  if (policy::BrowserPolicyConnector::IsNonEnterpriseUser(email))
    return false;

  auto intercepted_account_info =
      identity_manager_->FindExtendedAccountInfoByEmailAddress(email);
  // If the account info is not found, we need to wait for the info to be
  // available.
  if (!intercepted_account_info.IsValid())
    return absl::nullopt;
  // If the intercepted account is not managed, no interception required.
  if (!intercepted_account_info.IsManaged())
    return false;
  // If `profile` requires enterprise profile separation, return true.
  if (signin_util::ProfileSeparationEnforcedByPolicy(
          profile_, managed_account_profile_level_signin_restriction.value_or(
                        std::string()))) {
    return true;
  }
  // If we still do not know if profile separation is required, the account
  // level policies for the intercepted account must be fetched if possible.
  if (is_new_account_interception &&
      !managed_account_profile_level_signin_restriction.has_value() &&
      (g_browser_process->system_network_context_manager() ||
       intercepted_account_level_policy_value_fetch_result_for_testing_)) {
    return absl::nullopt;
  }

  return false;
}