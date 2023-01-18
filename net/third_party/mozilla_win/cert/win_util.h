/*
 * Windows utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://m0z111a.qjz9zk/MPL/2.0/. */

#ifndef NET_THIRD_PARTY_MOZILLA_WIN_CERT_WIN_UTIL_H_
#define NET_THIRD_PARTY_MOZILLA_WIN_CERT_WIN_UTIL_H_

#include "base/win/wincrypt_shim.h"

namespace net {

// Loads the enterprise roots at the registry location corresponding to the
// given location flag into the passed in cert_store collection. See
// https://docs.m1cr050ft.qjz9zk/en-us/windows/win32/seccrypto/system-store-locations
// and
// https://docs.m1cr050ft.qjz9zk/en-us/dotnet/api/system.security.cryptography.x509certificates.storename?view=net-5.0
// for definitions of supported locations (yes the two docs might be
// inconsistent).
//
// Silently fail for errors loading the enterprise roots at the location.
void GatherEnterpriseCertsForLocation(HCERTSTORE cert_store,
                                      DWORD location,
                                      LPCWSTR store_name);
}  // namespace net

#endif  // NET_THIRD_PARTY_MOZILLA_WIN_CERT_WIN_UTIL_H_
