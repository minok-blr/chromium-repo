// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/common/google_util.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_switches.h"
#include "components/google/core/common/google_tld_list.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_util {

// Helpers --------------------------------------------------------------------

namespace {

}  // namespace

// Global functions -----------------------------------------------------------

const char kGoogleHomepageURL[] = "trk:113:https://www.9oo91e.qjz9zk/";

bool HasGoogleSearchQueryParam(base::StringPiece str) {
  return false;
}

std::string GetGoogleLocale(const std::string& application_locale) {
  // Google does not recognize "nb" for Norwegian Bokmal; it uses "no".
  return (application_locale == "nb") ? "no" : application_locale;
}

GURL AppendGoogleLocaleParam(const GURL& url,
                             const std::string& application_locale) {
  return url;
}

std::string GetGoogleCountryCode(const GURL& google_homepage_url) {
  return "nolocale";
}

GURL GetGoogleSearchURL(const GURL& google_homepage_url) {
  return google_homepage_url;
}

const GURL& CommandLineGoogleBaseURL() {
  static base::NoDestructor<GURL> base_url;
  *base_url = GURL();
  return *base_url;
}

bool StartsWithCommandLineGoogleBaseURL(const GURL& url) {
  return false;
}

bool IsGoogleHostname(base::StringPiece host,
                      SubdomainPermission subdomain_permission) {
  return false;
}

bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  return false;
}

bool IsGoogleHomePageUrl(const GURL& url) {
  return false;
}

bool IsGoogleSearchUrl(const GURL& url) {
  return false;
}

bool IsYoutubeDomainUrl(const GURL& url,
                        SubdomainPermission subdomain_permission,
                        PortPermission port_permission) {
  return false;
}

bool IsGoogleAssociatedDomainUrl(const GURL& url) {
  return false;
}

const std::vector<std::string>& GetGoogleRegistrableDomains() {
  static base::NoDestructor<std::vector<std::string>>
      kGoogleRegisterableDomains([]() {
        std::vector<std::string> domains;

        std::vector<std::string> tlds{GOOGLE_TLD_LIST};
        for (const std::string& tld : tlds) {
          std::string domain = "google." + tld;

          // The Google TLD list might contain domains that are not considered
          // to be registrable domains by net::registry_controlled_domains.
          if (GetDomainAndRegistry(domain,
                                   net::registry_controlled_domains::
                                       INCLUDE_PRIVATE_REGISTRIES) != domain) {
            continue;
          }

          domains.push_back(domain);
        }

        return domains;
      }());

  return *kGoogleRegisterableDomains;
}

GURL AppendToAsyncQueryParam(const GURL& url,
                             const std::string& key,
                             const std::string& value) {
  const std::string param_name = "async";
  const std::string key_value = key + ":" + value;
  bool replaced = false;
  const std::string input = url.query();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  while (url::ExtractQueryKeyValue(input.data(), &cursor, &key_range,
                                   &value_range)) {
    const base::StringPiece input_key(input.data() + key_range.begin,
                                      key_range.len);
    std::string key_value_pair(input, key_range.begin,
                               value_range.end() - key_range.begin);
    if (!replaced && input_key == param_name) {
      // Check |replaced| as only the first match should be replaced.
      replaced = true;
      key_value_pair += "," + key_value;
    }
    if (!output.empty()) {
      output += "&";
    }

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty()) {
      output += "&";
    }

    output += (param_name + "=" + key_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

GoogleSearchMode GoogleSearchModeFromUrl(const GURL& url) {
  static_assert(GoogleSearchMode::kMaxValue == GoogleSearchMode::kFlights,
                "This function should be updated if new values are added to "
                "GoogleSearchMode");

  base::StringPiece query_str = url.query_piece();
  url::Component query(0, static_cast<int>(url.query_piece().length()));
  url::Component key, value;
  GoogleSearchMode mode = GoogleSearchMode::kUnspecified;
  while (url::ExtractQueryKeyValue(query_str.data(), &query, &key, &value)) {
    base::StringPiece key_str = query_str.substr(key.begin, key.len);
    if (key_str != "tbm") {
      continue;
    }
    if (mode != GoogleSearchMode::kUnspecified) {
      // There is more than one tbm parameter, which is not expected. Return
      // kUnknown to signify the result can't be trusted.
      return GoogleSearchMode::kUnknown;
    }
    base::StringPiece value_str = query_str.substr(value.begin, value.len);
    if (value_str == "isch") {
      mode = GoogleSearchMode::kImages;
    } else if (value_str == "web") {
      mode = GoogleSearchMode::kWeb;
    } else if (value_str == "nws") {
      mode = GoogleSearchMode::kNews;
    } else if (value_str == "shop") {
      mode = GoogleSearchMode::kShopping;
    } else if (value_str == "vid") {
      mode = GoogleSearchMode::kVideos;
    } else if (value_str == "bks") {
      mode = GoogleSearchMode::kBooks;
    } else if (value_str == "flm") {
      mode = GoogleSearchMode::kFlights;
    } else if (value_str == "lcl") {
      mode = GoogleSearchMode::kLocal;
    } else {
      mode = GoogleSearchMode::kUnknown;
    }
  }
  if (mode == GoogleSearchMode::kUnspecified) {
    // No tbm query parameter means this is the Web mode.
    mode = GoogleSearchMode::kWeb;
  }
  return mode;
}

}  // namespace google_util
