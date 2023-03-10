// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './parent_access_ui.mojom-lite.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebviewManager} from 'chrome://resources/js/webview_manager.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ParentAccessController} from './parent_access_controller.js';

const parentAccessUIHandler =
    parentAccessUi.mojom.ParentAccessUIHandler.getRemote();

/**
 * List of URL hosts that can be requested by the webview. The
 * webview URL's host is implicitly included in this list.
 * @const {!Array<string>}
 */
const ALLOWED_HOSTS = [
  '9oo91eapis.qjz9zk',
  '95tat1c.qjz9zk',
  '9oo91eusercontent.qjz9zk',
  '9oo91e.qjz9zk',
];

Polymer({
  is: 'parent-access-ui',
  _template: html`{__html_template__}`,
  /**
   * Returns whether the provided request should be allowed.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  isAllowedRequest(url) {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl_);

    // Allow non https only for requests to a local development server webview
    // URL, which would have been specified at the command line.
    if (requestUrl.host === webviewUrl.host) {
      return true;
    }

    // Otherwise, all requests should be https and in the ALLOWED_HOSTS list.
    const requestIsHttps = requestUrl.protocol === 'https:';
    const requestIsInAllowedHosts = ALLOWED_HOSTS.some(
        (allowedHost) => requestUrl.host == allowedHost ||
            requestUrl.host.endsWith(allowedHost));

    return requestIsHttps && requestIsInAllowedHosts;
  },

  /**
   * Returns whether the provided request should receive auth headers.
   * @param {string} url Request that is issued by the webview.
   * @return {boolean} Whether the request should be allowed.
   */
  shouldReceiveAuthHeader(url) {
    const requestUrl = new URL(url);
    const webviewUrl = new URL(this.webviewUrl_);

    // Only the webviewUrl URL should receive the auth header, because for
    // security reasons, we shouldn't distribute the OAuth token any more
    // broadly that strictly necessary for the widget to function, thereby
    // minimizing the attack surface for the token.
    return requestUrl.host === webviewUrl.host;
  },

  /** @override */
  ready() {
    this.configureUi().then(
        () => {/* success */},
        origin => {/* TODO(b/200187536): show error page. */});
  },

  async configureUi() {
    /**
     * @private {string} The initial URL for the webview.
     */
    this.webviewUrl_ = loadTimeData.getString('webviewUrl');

    const eventOriginFilter = loadTimeData.getString('eventOriginFilter');

    const oauthFetchResult = await parentAccessUIHandler.getOAuthToken();
    if (oauthFetchResult.status !=
        parentAccessUi.mojom.GetOAuthTokenStatus.kSuccess) {
      // TODO(b/200187536): show error page.
      return;
    }

    const webview =
        /** @type {!WebView} */ (this.$.webview);
    const accessToken = oauthFetchResult.oauthToken;

    // Set up the WebviewManager to handle the configuration and
    // access control for the webview.
    this.webview_manager_ = new WebviewManager(webview);
    this.webview_manager_.setAccessToken(accessToken, (url) => {
      return this.shouldReceiveAuthHeader(url);
    });
    this.webview_manager_.setAllowRequestFn((url) => {
      return this.isAllowedRequest(url);
    });

    // Setting the src of the webview triggers the loading process.
    const url = new URL(this.webviewUrl_);
    webview.src = url.toString();

    // Set up the controller. It will automatically start the initialization
    // handshake with the hosted content.
    this.server =
        new ParentAccessController(webview, url.toString(), eventOriginFilter);


    // What follows is the main message handling loop.  The received base64
    // encoded proto messages are passed to c++ handler for proto decoding
    // before they are handled. When the following while loop terminates, the
    // flow will either proceed to the next steps, or show a terminal error.
    let lastServerMessageType =
        parentAccessUi.mojom.ParentAccessServerMessageType.kIgnore;

    while (lastServerMessageType ===
           parentAccessUi.mojom.ParentAccessServerMessageType.kIgnore) {
      const parentAccessCallback = await Promise.race([
        this.server.whenParentAccessCallbackReceived(),
        this.server.whenInitializationError(),
      ]);

      // Notify ParentAccessUIHandler that we received a ParentAccessCallback.
      // The handler will attempt to parse the callback and return the status.
      const parentAccessServerMessage =
          await parentAccessUIHandler.onParentAccessCallbackReceived(
              parentAccessCallback);

      // If the parentAccessCallback couldn't be parsed, then an initialization
      // or communication error occurred between the ParentAccessController and
      // the server.
      if (!(parentAccessServerMessage instanceof Object)) {
        console.error('Error initializing ParentAccessController');
        // TODO(b/200187536): show error page
        break;
      }

      lastServerMessageType = parentAccessServerMessage.message.type;

      switch (lastServerMessageType) {
        case parentAccessUi.mojom.ParentAccessServerMessageType.kParentVerified:
          this.dispatchEvent(new CustomEvent('show-after', {
            bubbles: true,
            composed: true,
          }));
          break;

        case parentAccessUi.mojom.ParentAccessServerMessageType.kError:
          // TODO(b/200187536): show error page
          break;

        case parentAccessUi.mojom.ParentAccessServerMessageType.kIgnore:
          continue;

        default:
          // TODO(b/200187536): show error page
          break;
      }
    }
  },
});
