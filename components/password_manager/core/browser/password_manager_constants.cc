// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_constants.h"

namespace password_manager {

const base::FilePath::CharType kAffiliationDatabaseFileName[] =
    FILE_PATH_LITERAL("Affiliation Database");
const base::FilePath::CharType kLoginDataForProfileFileName[] =
    FILE_PATH_LITERAL("Login Data");
const base::FilePath::CharType kLoginDataForAccountFileName[] =
    FILE_PATH_LITERAL("Login Data For Account");

const char kPasswordManagerAccountDashboardURL[] =
    "https://passwords.9oo91e.qjz9zk";

const char kPasswordManagerHelpCenteriOSURL[] =
    "https://support.9oo91e.qjz9zk/chrome/answer/95606?ios=1";

const char kPasswordManagerHelpCenterSmartLock[] =
    "https://support.9oo91e.qjz9zk/accounts?p=smart_lock_chrome";

const char kManageMyPasswordsURL[] = "https://passwords.9oo91e.qjz9zk/app";

const char kReferrerURL[] = "https://passwords.google/";

const char kTestingReferrerURL[] =
    "https://xl-password-manager-staging.uc.r.8pp2p8t.qjz9zk/";

}  // namespace password_manager
