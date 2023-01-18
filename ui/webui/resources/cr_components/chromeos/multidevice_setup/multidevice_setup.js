// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @enum {string} */
  /* #export */ const PageName = {
    PASSWORD: 'password-page',
    SUCCESS: 'setup-succeeded-page',
    START: 'start-setup-page',
  };

  const MultiDeviceSetup = Polymer({
    is: 'multidevice-setup',

    behaviors: [WebUIListenerBehavior],

    properties: {
      /**
       * Delegate object which performs differently in OOBE vs. non-OOBE mode.
       * @type {!multidevice_setup.MultiDeviceSetupDelegate}
       */
      delegate: Object,

      /**
       * ID of loadTimeData string to be shown on the forward navigation button.
       * @type {string|undefined}
       */
      forwardButtonTextId: {
        type: String,
        computed: 'getForwardButtonTextId_(visiblePage_)',
        notify: true,
      },

      /** Whether the forward button should be disabled. */
      forwardButtonDisabled: {
        type: Boolean,
        computed: 'shouldForwardButtonBeDisabled_(' +
            'passwordPageForwardButtonDisabled_, visiblePageName)',
        notify: true,
      },

      /**
       * ID of loadTimeData string to be shown on the cancel button.
       * @type {string|undefined}
       */
      cancelButtonTextId: {
        type: String,
        computed: 'getCancelButtonTextId_(visiblePage_)',
        notify: true,
      },

      /**
       * ID of loadTimeData string to be shown on the backward navigation
       * button.
       * @type {string|undefined}
       */
      backwardButtonTextId: {
        type: String,
        computed: 'getBackwardButtonTextId_(visiblePage_)',
        notify: true,
      },

      /**
       * Element name of the currently visible page.
       *
       * @type {!multidevice_setup.PageName}
       */
      visiblePageName: {
        type: String,
        value: PageName.START,
        notify: true,
      },

      /**
       * DOM Element corresponding to the visible page.
       *
       * @private {!PasswordPageElement|!StartSetupPageElement|
       *           !SetupSucceededPageElement}
       */
      visiblePage_: Object,

      /**
       * Authentication token, which is generated by the password page.
       * @private {string}
       */
      authToken_: {
        type: String,
      },

      /**
       * Array of objects representing all potential MultiDevice hosts.
       *
       * @private {!Array<!ash.multideviceSetup.mojom.HostDevice>}
       */
      devices_: Array,

      /**
       * Unique identifier for the currently selected host device. This uses the
       * device's Instance ID if it is available; otherwise, the device's legacy
       * device ID is used.
       * TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
       * use Instance ID since all devices are guaranteed to have one.
       *
       * Undefined if the no list of potential hosts has been received from mojo
       * service.
       *
       * @private {string|undefined}
       */
      selectedInstanceIdOrLegacyDeviceId_: String,

      /**
       * Whether the password page reports that the forward button should be
       * disabled. This field is only relevant when the password page is
       * visible.
       * @private {boolean}
       */
      passwordPageForwardButtonDisabled_: Boolean,

      /**
       * Provider of an interface to the MultiDeviceSetup Mojo service.
       * @private {!multidevice_setup.MojoInterfaceProvider}
       */
      mojoInterfaceProvider_: Object,

      /**
       * Whether a shadow should appear over the button bar; the shadow is
       * intended to appear when the contents are not scrolled to the bottom to
       * indicate that more contents can be viewed below.
       * @private
       */
      isScrolledToBottom_: {
        type: Boolean,
        value: false,
      },
    },

    listeners: {
      'scroll': 'onWindowContentUpdate_',
      'backward-navigation-requested': 'onBackwardNavigationRequested_',
      'cancel-requested': 'onCancelRequested_',
      'forward-navigation-requested': 'onForwardNavigationRequested_',
    },

    /** @override */
    created() {
      this.mojoInterfaceProvider_ =
          multidevice_setup.MojoInterfaceProviderImpl.getInstance();
    },

    /** @override */
    ready() {
      this.addWebUIListener(
          'multidevice_setup.initializeSetupFlow',
          this.initializeSetupFlow.bind(this));
    },

    /** @override */
    attached() {
      window.addEventListener(
          'orientationchange', this.onWindowContentUpdate_.bind(this));
      window.addEventListener('resize', this.onWindowContentUpdate_.bind(this));
    },

    /** @override */
    detached() {
      window.removeEventListener(
          'orientationchange', this.onWindowContentUpdate_.bind(this));
      window.removeEventListener(
          'resize', this.onWindowContentUpdate_.bind(this));
    },

    updateLocalizedContent() {
      this.$.ironPages.querySelectorAll('.ui-page')
          .forEach(page => page.i18nUpdateLocale());
    },

    initializeSetupFlow() {
      this.$$('start-setup-page').setPlayAnimation(true);
      this.mojoInterfaceProvider_.getMojoServiceRemote()
          .getEligibleActiveHostDevices()
          .then((responseParams) => {
            if (responseParams.eligibleHostDevices.length === 0) {
              console.warn('Potential host list is empty.');
              return;
            }

            this.devices_ = responseParams.eligibleHostDevices;
            this.fire('forward-button-focus-requested');
          })
          .catch((error) => {
            console.warn('Mojo service failure: ' + error);
          });
    },

    /** @private */
    onCancelRequested_() {
      this.exitSetupFlow_(false /* didUserCompleteSetup */);
    },

    /**
     * Called when contents are scrolled, the window is resized, or the window's
     * orientation is updated.
     * @private
     */
    onWindowContentUpdate_() {
      // (scrollHeight - scrollTop) represents the visible height of the
      // contents, not including scrollbars.
      const visibleHeight = this.scrollHeight - this.scrollTop;

      // If these two heights are equal, the contents are scrolled to the
      // bottom. Instead of using equality, we check that the difference is
      // sufficiently small to account for fractional values due to browser
      // zoom and/or display density.
      this.isScrolledToBottom_ =
          Math.abs(this.clientHeight - visibleHeight) < 1;
    },

    /** @private */
    onBackwardNavigationRequested_() {
      // The back button is only visible on the password page.
      assert(this.visiblePageName === PageName.PASSWORD);

      this.$$('password-page').clearPasswordTextInput();
      this.visiblePageName = PageName.START;
      this.fire('forward-button-focus-requested');
    },

    /** @private */
    onForwardNavigationRequested_() {
      if (this.forwardButtonDisabled) {
        return;
      }

      this.visiblePage_.getCanNavigateToNextPage().then((canNavigate) => {
        if (!canNavigate) {
          return;
        }
        this.navigateForward_();
      });
    },

    /** @private */
    navigateForward_() {
      switch (this.visiblePageName) {
        case PageName.PASSWORD:
          this.$$('password-page').clearPasswordTextInput();
          this.setHostDevice_();
          return;
        case PageName.SUCCESS:
          this.exitSetupFlow_(true /* didUserCompleteSetup */);
          return;
        case PageName.START:
          if (this.delegate.isPasswordRequiredToSetHost()) {
            this.visiblePageName = PageName.PASSWORD;
            this.$$('password-page').focusPasswordTextInput();
          } else {
            this.setHostDevice_();
          }
          return;
      }
    },

    /** @private */
    setHostDevice_() {
      // An authentication token must be set if a password is required.
      assert(this.delegate.isPasswordRequiredToSetHost() === !!this.authToken_);

      const instanceIdOrLegacyDeviceId =
          /** @type {string} */ (this.selectedInstanceIdOrLegacyDeviceId_);
      this.delegate.setHostDevice(instanceIdOrLegacyDeviceId, this.authToken_)
          .then((responseParams) => {
            if (!responseParams.success) {
              console.warn(
                  'Failure setting host with ID: ' +
                  instanceIdOrLegacyDeviceId);
              return;
            }

            if (this.delegate.shouldExitSetupFlowAfterSettingHost()) {
              this.exitSetupFlow_(true /* didUserCompleteSetup */);
              return;
            }

            this.visiblePageName = PageName.SUCCESS;
            this.fire('forward-button-focus-requested');
          })
          .catch((error) => {
            console.warn('Mojo service failure: ' + error);
          });
    },

    /** @private */
    onUserSubmittedPassword_() {
      this.onForwardNavigationRequested_();
    },

    /**
     * @return {string|undefined} The ID of loadTimeData string for the
     *     forward button text, which is undefined if no button should be
     *     displayed.
     * @private
     */
    getForwardButtonTextId_() {
      if (!this.visiblePage_) {
        return undefined;
      }
      return this.visiblePage_.forwardButtonTextId;
    },

    /**
     * @return {boolean} Whether the forward button should be disabled.
     * @private
     */
    shouldForwardButtonBeDisabled_() {
      return (this.visiblePageName === PageName.PASSWORD) &&
          this.passwordPageForwardButtonDisabled_;
    },

    /**
     * @return {string|undefined} The ID of loadTimeData string for the
     *     cancel button text, which is undefined if no button should be
     *     displayed.
     * @private
     */
    getCancelButtonTextId_() {
      if (!this.visiblePage_) {
        return undefined;
      }
      return this.visiblePage_.cancelButtonTextId;
    },

    /**
     * @return {string|undefined} The ID of loadTimeData string for the
     *     backward button text, which is undefined if no button should be
     *     displayed.
     * @private
     */
    getBackwardButtonTextId_() {
      if (!this.visiblePage_) {
        return undefined;
      }
      return this.visiblePage_.backwardButtonTextId;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldPasswordPageBeIncluded_() {
      return this.delegate.isPasswordRequiredToSetHost();
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldSetupSucceededPageBeIncluded_() {
      return !this.delegate.shouldExitSetupFlowAfterSettingHost();
    },

    /**
     * Notifies observers that the setup flow has completed.
     * @param {boolean} didUserCompleteSetup
     * @private
     */
    exitSetupFlow_(didUserCompleteSetup) {
      this.$$('start-setup-page').setPlayAnimation(false);
      this.fire('setup-exited', {didUserCompleteSetup: didUserCompleteSetup});
    },
  });

  // #cr_define_end
  return {
    MultiDeviceSetup: MultiDeviceSetup,
    PageName: PageName,
  };
});