// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

bool IsSourceFromAnExtension(const std::u16string& source) {
  return GURL(source).SchemeIs(kExtensionScheme) ||
         base::StartsWith(source, u"extensions::",
                          base::CompareCase::SENSITIVE);
}

}  // namespace extensions

namespace extension_urls {

const char kChromeWebstoreBaseURL[] = "trk:09:https://chrome.9oo91e.qjz9zk/webstore";
const char kNewChromeWebstoreBaseURL[] = "trk:08:https://webstore.9oo91e.qjz9zk/";
const char kChromeWebstoreUpdateURL[] =
    "trk:05:https://clients2.9oo91e.qjz9zk/service/update2/crx";

GURL GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return GURL(kChromeWebstoreBaseURL);
}

GURL GetNewWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetNewWebstoreBaseURL();
  return GURL(kNewChromeWebstoreBaseURL);
}

// TODO(csharrison,devlin): Migrate the following methods to return
// GURLs.
// TODO(devlin): Try to use GURL methods like Resolve instead of string
// concatenation.
std::string GetWebstoreExtensionsCategoryURL() {
  return GetWebstoreLaunchURL().spec() + "/category/extensions";
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL().spec() + "/detail/";
}

GURL GetWebstoreItemJsonDataURL(const std::string& extension_id) {
  return GURL(GetWebstoreLaunchURL().spec() + "/inlineinstall/detail/" +
              extension_id);
}

GURL GetDefaultWebstoreUpdateUrl() {
  return GURL(kChromeWebstoreUpdateURL);
}

GURL GetWebstoreUpdateUrl() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreUpdateURL();
  return GetDefaultWebstoreUpdateUrl();
}

GURL GetWebstoreReportAbuseUrl(const std::string& extension_id,
                               const std::string& referrer_id) {
  return GURL(base::StringPrintf("%s/report/%s?utm_source=%s",
                                 GetWebstoreLaunchURL().spec().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  return (update_url.host_piece() == store_url.host_piece() &&
          update_url.path_piece() == store_url.path_piece());
}

bool IsBlocklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlocklistUpdateURL(url);
  return false;
}

bool IsSafeBrowsingUrl(const url::Origin& origin, base::StringPiece path) {
  return origin.DomainIs("sb-ssl.9oo91e.qjz9zk") ||
         origin.DomainIs("safebrowsing.9oo91eapis.qjz9zk") ||
         (origin.DomainIs("safebrowsing.9oo91e.qjz9zk") &&
          base::StartsWith(path, "/safebrowsing",
                           base::CompareCase::SENSITIVE));
}

}  // namespace extension_urls
