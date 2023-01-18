// Copyright (c) 2020 The ungoogled-chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNGOOGLED_PLATFORM_FLAG_ENTRIES_H_
#define CHROME_BROWSER_UNGOOGLED_PLATFORM_FLAG_ENTRIES_H_
    {"disable-encryption",
     "Disable encryption",
     "Disable encryption of cookies, passwords, and settings which uses a generated machine-specific encryption key.  This is used to enable portable user data directories.  ungoogled-chromium flag.",
     kOsWin, SINGLE_VALUE_TYPE("disable-encryption")},
    {"disable-machine-id",
     "Disable machine ID",
     "Disables use of a generated machine-specific ID to lock the user data directory to that machine.  This is used to enable portable user data directories.  ungoogled-chromium flag.",
     kOsWin, SINGLE_VALUE_TYPE("disable-machine-id")},
#endif  // CHROME_BROWSER_UNGOOGLED_PLATFORM_FLAG_ENTRIES_H_
