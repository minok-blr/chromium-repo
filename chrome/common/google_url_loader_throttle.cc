// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/net/safe_search_util.h"
#include "components/google/core/common/google_util.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/x_frame_options.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/device_form_factor.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
const char kCCTClientDataHeader[] = "X-CCT-Client-Data";
const char kRequestDesktopDataHeader[] = "X-Eligible-Tablet";
#endif

}  // namespace

// static
void GoogleURLLoaderThrottle::UpdateCorsExemptHeader(
    network::mojom::NetworkContextParams* params) {
  params->cors_exempt_header_list.push_back(
      safe_search_util::kGoogleAppsAllowedDomains);
  params->cors_exempt_header_list.push_back(
      safe_search_util::kYouTubeRestrictHeaderName);
#if BUILDFLAG(IS_ANDROID)
  params->cors_exempt_header_list.push_back(kCCTClientDataHeader);
#endif
}

GoogleURLLoaderThrottle::GoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
    const std::string& client_data_header,
    bool is_tab_large_enough,
#endif
    chrome::mojom::DynamicParams dynamic_params)
    :
#if BUILDFLAG(IS_ANDROID)
      client_data_header_(client_data_header),
      is_tab_large_enough_(is_tab_large_enough),
#endif
      dynamic_params_(std::move(dynamic_params)) {
}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() = default;

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
}

void GoogleURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GoogleURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
}
#endif
