// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace password_manager {

namespace {
// Minimum number of characters in a password for finding it as password reuse.
// It does not make sense to consider short strings for password reuse, since it
// is quite likely that they are parts of common words.
constexpr size_t kMinPasswordLengthToCheck = 8;

// Returns true iff |suffix_candidate| is a suffix of |str|.
bool IsSuffix(const std::u16string& str,
              const std::u16string& suffix_candidate) {
  if (str.size() < suffix_candidate.size())
    return false;
  return std::equal(suffix_candidate.rbegin(), suffix_candidate.rend(),
                    str.rbegin());
}

// Helper function to returns matching PasswordHashData from a list that has
// the longest password length.
absl::optional<PasswordHashData> FindPasswordReuse(
    const std::u16string& input,
    const std::vector<PasswordHashData>& password_hash_list) {
  absl::optional<PasswordHashData> longest_match = absl::nullopt;
  size_t longest_match_size = 0;
  for (const PasswordHashData& hash_data : password_hash_list) {
    if (input.size() < hash_data.length)
      continue;
    size_t offset = input.size() - hash_data.length;
    std::u16string reuse_candidate = input.substr(offset);
    // It is possible that input matches multiple passwords in the list,
    // we only return the first match due to simplicity.
    if (CalculatePasswordHash(reuse_candidate, hash_data.salt) ==
            hash_data.hash &&
        hash_data.length > longest_match_size) {
      longest_match_size = hash_data.length;
      longest_match = hash_data;
    }
  }
  return longest_match;
}

}  // namespace

bool ReverseStringLess::operator()(const std::u16string& lhs,
                                   const std::u16string& rhs) const {
  return std::lexicographical_compare(lhs.rbegin(), lhs.rend(), rhs.rbegin(),
                                      rhs.rend());
}

bool MatchingReusedCredential::operator<(
    const MatchingReusedCredential& other) const {
  return std::tie(signon_realm, username, in_store) <
         std::tie(other.signon_realm, other.username, other.in_store);
}

bool MatchingReusedCredential::operator==(
    const MatchingReusedCredential& other) const {
  return signon_realm == other.signon_realm && username == other.username &&
         in_store == other.in_store;
}

PasswordReuseDetector::PasswordReuseDetector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PasswordReuseDetector::~PasswordReuseDetector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PasswordReuseDetector::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& form : results)
    AddPassword(*form);
}

void PasswordReuseDetector::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& change : changes) {
    if (change.type() == PasswordStoreChange::ADD ||
        change.type() == PasswordStoreChange::UPDATE)
      AddPassword(change.form());
    if (change.type() == PasswordStoreChange::REMOVE)
      RemovePassword(change.form());
  }
}

void PasswordReuseDetector::OnLoginsRetained(
    const std::vector<PasswordForm>& retained_passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  passwords_with_matching_reused_credentials_.clear();
  // |retained_passwords| contains also blacklisted entities, but since they
  // don't have password value they will be skipped inside AddPassword().
  for (const auto& form : retained_passwords)
    AddPassword(form);
}

void PasswordReuseDetector::ClearCachedAccountStorePasswords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& key_value : passwords_with_matching_reused_credentials_) {
    // Delete account stored credentials.
    base::EraseIf(key_value.second, [](const auto& credential) {
      return credential.in_store == PasswordForm::Store::kAccountStore;
    });
  }
  // Delete all passwords with no matching credentials.
  base::EraseIf(passwords_with_matching_reused_credentials_,
                [](const auto& pair) { return pair.second.empty(); });
}

void PasswordReuseDetector::CheckReuse(
    const std::u16string& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(consumer);
  if (input.size() < kMinPasswordLengthToCheck) {
    consumer->OnReuseCheckDone(false, 0, absl::nullopt, {},
                               SavedPasswordsCount());
    return;
  }

  absl::optional<PasswordHashData> reused_gaia_password_hash =
      CheckGaiaPasswordReuse(input, domain);
  size_t gaia_reused_password_length = reused_gaia_password_hash.has_value()
                                           ? reused_gaia_password_hash->length
                                           : 0;

  absl::optional<PasswordHashData> reused_enterprise_password_hash =
      CheckNonGaiaEnterprisePasswordReuse(input, domain);
  size_t enterprise_reused_password_length =
      reused_enterprise_password_hash.has_value()
          ? reused_enterprise_password_hash->length
          : 0;

  std::vector<MatchingReusedCredential> matching_reused_credentials;
  size_t saved_reused_password_length =
      CheckSavedPasswordReuse(input, domain, &matching_reused_credentials);

  size_t max_reused_password_length =
      std::max({saved_reused_password_length, gaia_reused_password_length,
                enterprise_reused_password_length});

  if (max_reused_password_length == 0) {
    consumer->OnReuseCheckDone(false, 0, absl::nullopt, {},
                               SavedPasswordsCount());
    return;
  }

  absl::optional<PasswordHashData> reused_protected_password_hash =
      absl::nullopt;
  if (gaia_reused_password_length > enterprise_reused_password_length) {
    reused_protected_password_hash = std::move(reused_gaia_password_hash);
  } else if (enterprise_reused_password_length != 0) {
    reused_protected_password_hash = std::move(reused_enterprise_password_hash);
  }
  consumer->OnReuseCheckDone(
      true, max_reused_password_length, reused_protected_password_hash,
      matching_reused_credentials, SavedPasswordsCount());
}

absl::optional<PasswordHashData> PasswordReuseDetector::CheckGaiaPasswordReuse(
    const std::u16string& input,
    const std::string& domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_.has_value() ||
      gaia_password_hash_data_list_->empty()) {
    return absl::nullopt;
  }

  // Skips password reuse check if |domain| matches Gaia origin.
  if (gaia::HasGaiaSchemeHostPort(GURL(domain)))
    return absl::nullopt;

  return FindPasswordReuse(input, gaia_password_hash_data_list_.value());
}

absl::optional<PasswordHashData>
PasswordReuseDetector::CheckNonGaiaEnterprisePasswordReuse(
    const std::u16string& input,
    const std::string& domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enterprise_password_hash_data_list_.has_value() ||
      enterprise_password_hash_data_list_->empty()) {
    return absl::nullopt;
  }

  // Skips password reuse check if |domain| matches enterprise login URL or
  // enterprise change password URL.
  GURL page_url(domain);
  if (enterprise_password_urls_.has_value()) {
    return absl::nullopt;
  }

  return FindPasswordReuse(input, enterprise_password_hash_data_list_.value());
}

size_t PasswordReuseDetector::CheckSavedPasswordReuse(
    const std::u16string& input,
    const std::string& domain,
    std::vector<MatchingReusedCredential>* matching_reused_credentials_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));

  // More than one password call match |input| if they share a common suffix
  // with |input|.  Collect the set of MatchingReusedCredential for all matches.
  std::set<MatchingReusedCredential> matching_reused_credentials_set;

  // The longest password match is kept for metrics.
  size_t longest_match_len = 0;

  for (auto passwords_iterator = FindFirstSavedPassword(input);
       passwords_iterator != passwords_with_matching_reused_credentials_.end();
       passwords_iterator = FindNextSavedPassword(input, passwords_iterator)) {
    const std::set<MatchingReusedCredential>& credentials =
        passwords_iterator->second;
    DCHECK(!credentials.empty());

    std::set<std::string> signon_realms;
    for (const auto& credential : credentials) {
      signon_realms.insert(
          GetRegistryControlledDomain(GURL(credential.signon_realm)));
    }
    // If the page's URL matches a saved domain for this password,
    // this isn't password-reuse.
    if (base::Contains(signon_realms, registry_controlled_domain))
      continue;

    matching_reused_credentials_set.insert(credentials.begin(),
                                           credentials.end());
    DCHECK(!passwords_iterator->first.empty());
    longest_match_len =
        std::max(longest_match_len, passwords_iterator->first.size());
  }

  matching_reused_credentials_out->assign(
      matching_reused_credentials_set.begin(),
      matching_reused_credentials_set.end());

  return longest_match_len;
}

void PasswordReuseDetector::UseGaiaPasswordHash(
    absl::optional<std::vector<PasswordHashData>> password_hash_data_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gaia_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetector::UseNonGaiaEnterprisePasswordHash(
    absl::optional<std::vector<PasswordHashData>> password_hash_data_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_hash_data_list_ = std::move(password_hash_data_list);
}

void PasswordReuseDetector::UseEnterprisePasswordURLs(
    absl::optional<std::vector<GURL>> enterprise_login_urls,
    absl::optional<GURL> enterprise_change_password_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_urls_ = std::move(enterprise_login_urls);
  if (!enterprise_change_password_url.has_value() ||
      !enterprise_change_password_url->is_valid()) {
    return;
  }

  if (!enterprise_password_urls_)
    enterprise_password_urls_ = absl::make_optional<std::vector<GURL>>();
  enterprise_password_urls_->push_back(enterprise_change_password_url.value());
}

void PasswordReuseDetector::ClearGaiaPasswordHash(const std::string& username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_)
    return;

  base::EraseIf(*gaia_password_hash_data_list_,
                [&username](const PasswordHashData& data) {
                  return AreUsernamesSame(username, true, data.username,
                                          data.is_gaia_password);
                });
}

void PasswordReuseDetector::ClearAllGaiaPasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gaia_password_hash_data_list_.reset();
}

void PasswordReuseDetector::ClearAllEnterprisePasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enterprise_password_hash_data_list_.reset();
}

void PasswordReuseDetector::ClearAllNonGmailPasswordHash() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gaia_password_hash_data_list_)
    return;

  base::EraseIf(
      *gaia_password_hash_data_list_, [](const PasswordHashData& data) {
        std::string email =
            CanonicalizeUsername(data.username, data.is_gaia_password);
        return email.find("@9ma1l.qjz9zk") == std::string::npos;
      });
}

void PasswordReuseDetector::AddPassword(const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (form.password_value.size() < kMinPasswordLengthToCheck)
    return;

  passwords_with_matching_reused_credentials_[form.password_value].insert(
      {form.signon_realm, form.username_value, form.in_store});
}

void PasswordReuseDetector::RemovePassword(const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (form.password_value.size() < kMinPasswordLengthToCheck)
    return;

  MatchingReusedCredential credential_criteria = {
      form.signon_realm, form.username_value, form.in_store};
  auto password_value_iter =
      passwords_with_matching_reused_credentials_.begin();
  while (password_value_iter !=
         passwords_with_matching_reused_credentials_.end()) {
    std::set<MatchingReusedCredential>& stored_credentials_for_password_value =
        password_value_iter->second;
    // Remove only the password for the specific domain and username.
    // Don't remove all passwords from
    // |passwords_with_matching_reused_credentials_| with a given
    // |form.password_value|.
    const auto credential_to_remove =
        stored_credentials_for_password_value.find(credential_criteria);
    if (credential_to_remove != stored_credentials_for_password_value.end()) {
      stored_credentials_for_password_value.erase(credential_to_remove);

      // If all credential values of the password key are deleted, remove the
      // password key from the map.
      if (stored_credentials_for_password_value.empty()) {
        password_value_iter = passwords_with_matching_reused_credentials_.erase(
            password_value_iter);
      }
    } else {
      password_value_iter++;
    }
  }
}

PasswordReuseDetector::passwords_iterator
PasswordReuseDetector::FindFirstSavedPassword(const std::u16string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Keys in |passwords_with_matching_reused_credentials_| are ordered by
  // lexicographical order of reversed strings. In order to check a password
  // reuse a key of |passwords_with_matching_reused_credentials_| that is a
  // suffix of |input| should be found. The longest such key should be the
  // largest key in the |passwords_with_matching_reused_credentials_| keys order
  // that is equal or smaller to |input|. There may be more, shorter, matches as
  // well -- call FindNextSavedPassword(it) to find the next one.
  if (passwords_with_matching_reused_credentials_.empty())
    return passwords_with_matching_reused_credentials_.end();

  // lower_bound returns the first key that is bigger or equal to input.
  passwords_iterator it =
      passwords_with_matching_reused_credentials_.lower_bound(input);
  if (it != passwords_with_matching_reused_credentials_.end() &&
      it->first == input) {
    // If the key is equal then a saved password is found.
    return it;
  }
  // Otherwise the previous key is a candidate for password reuse.
  return FindNextSavedPassword(input, it);
}

PasswordReuseDetector::passwords_iterator
PasswordReuseDetector::FindNextSavedPassword(
    const std::u16string& input,
    PasswordReuseDetector::passwords_iterator it) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (it == passwords_with_matching_reused_credentials_.begin())
    return passwords_with_matching_reused_credentials_.end();
  --it;
  return IsSuffix(input, it->first)
             ? it
             : passwords_with_matching_reused_credentials_.end();
}

size_t PasswordReuseDetector::SavedPasswordsCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  for (const auto& pair : passwords_with_matching_reused_credentials_) {
    count += pair.second.size();
  }
  return count;
}

}  // namespace password_manager
