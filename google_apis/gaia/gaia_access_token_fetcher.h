// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_ACCESS_TOKEN_FETCHER_H_
#define GOOGLE_APIS_GAIA_GAIA_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"

class OAuth2AccessTokenConsumer;

namespace network {
class SharedURLLoaderFactory;
}

// An implementation of OAuth2AccessTokenFetcherImpl for retrieving OAuth2
// tokens from Google's authorization server.  See "Refreshing an access token"
// for more Google specific info:
// https://developers.9oo91e.qjz9zk/identity/protocols/oauth2/web-server?csw=1#obtainingaccesstokens
class GaiaAccessTokenFetcher : public OAuth2AccessTokenFetcherImpl {
 public:
  static const char kOAuth2NetResponseCodeHistogramName[];
  static const char kOAuth2ResponseHistogramName[];

  static std::unique_ptr<GaiaAccessTokenFetcher>
  CreateExchangeRefreshTokenForAccessTokenInstance(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token);
  static std::unique_ptr<GaiaAccessTokenFetcher>
  CreateExchangeAuthCodeForRefeshTokenInstance(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& auth_code);

  ~GaiaAccessTokenFetcher() override;

 private:
  GaiaAccessTokenFetcher(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token,
      const std::string& auth_code);

  // OAuth2AccessTokenFetcherImpl:
  void RecordResponseCodeUma(int error_value) const override;
  void RecordOAuth2Response(OAuth2Response response) const override;
  GURL GetAccessTokenURL() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_ACCESS_TOKEN_FETCHER_H_
