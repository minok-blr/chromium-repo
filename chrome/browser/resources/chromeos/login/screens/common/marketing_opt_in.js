// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design marketing
 * opt-in screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const MarketingScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
    Polymer.Element);

/**
 * Enum to represent each page in the marketing opt in screen.
 * @enum {string}
 */
const MarketingOptInStep = {
  OVERVIEW: 'overview',
  ACCESSIBILITY: 'accessibility',
};

/**
 * URL of the animation in the light mode.
 * @type {string}
 */
const ANIMATION_LIGHT_URL = 'animations/all_set_light.json';

/**
 * URL of the animation in the dark mode.
 * @type {string}
 */
const ANIMATION_DARK_URL = 'animations/all_set_dark.json';

/**
 * @typedef {{
 *   marketingOptInOverviewDialog:  OobeAdaptiveDialogElement,
 *   chromebookUpdatesOption:  CrToggleElement,
 *   a11yNavButtonToggle:  OobeA11yOption,
 * }}
 */
MarketingScreenElementBase.$;

/**
 * @polymer
 */
class MarketingOptIn extends MarketingScreenElementBase {
  static get is() {
    return 'marketing-opt-in-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Whether the accessibility button is shown. This button is only shown
       * if the gesture EDU screen was shown before the marketing screen.
       */
      isA11ySettingsButtonVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the marketing opt in toggles should be shown, which will be the
       * case only if marketing opt in feature is enabled AND if the current
       * user is a non-managed user. When this is false, the screen will only
       * contain UI related to the tablet mode gestural navigation settings.
       */
      marketingOptInVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether a verbose footer will be shown to the user containing some
       * legal information such as the Google address. Currently shown for
       * Canada only.
       */
      hasLegalFooter_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the marketing opt-in page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the device is cloud gaming device, which will
       * alternate different title, subtitle and animation.
       */
      isCloudGamingDevice_: {
        type: Boolean,
        value: false,
      },
    };
  }

  get UI_STEPS() {
    return MarketingOptInStep;
  }

  defaultUIStep() {
    return MarketingOptInStep.OVERVIEW;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.MARKETING_OPT_IN;
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['updateA11ySettingsButtonVisibility',
            'updateA11yNavigationButtonToggle'];
  }

  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('MarketingOptInScreen');
  }

  /** Shortcut method to control animation */
  setAnimationPlay_(played) {
    this.$.animation.playing = played;
  }

  onBeforeShow(data) {
    this.marketingOptInVisible_ =
        'optInVisibility' in data && data.optInVisibility;
    this.$.chromebookUpdatesOption.checked =
        'optInDefaultState' in data && data.optInDefaultState;
    this.hasLegalFooter_ =
        'legalFooterVisibility' in data && data.legalFooterVisibility;
    this.isCloudGamingDevice_ =
        'cloudGamingDevice' in data && data.cloudGamingDevice;
    this.setAnimationPlay_(true);
    this.$.marketingOptInOverviewDialog.show();
  }

  get defaultControl() {
    return this.$.marketingOptInOverviewDialog;
  }

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onGetStarted_() {
    this.setAnimationPlay_(false);
    this.userActed(['get-started', this.$.chromebookUpdatesOption.checked]);
  }

  /**
   * @param {boolean} shown Whether the A11y Settings button should be shown.
   */
  updateA11ySettingsButtonVisibility(shown) {
    this.isA11ySettingsButtonVisible_ = shown;
  }

  /**
   * @param {boolean} enabled Whether the a11y setting for shownig shelf
   * navigation buttons is enabled.
   */
  updateA11yNavigationButtonToggle(enabled) {
    this.$.a11yNavButtonToggle.checked = enabled;
  }

  /**
   * This is the 'on-tap' event handler for the accessibility settings link and
   * for the back button on the accessibility page.
   * @private
   */
  onToggleAccessibilityPage_() {
    if (this.uiStep == MarketingOptInStep.OVERVIEW) {
      this.setUIStep(MarketingOptInStep.ACCESSIBILITY);
      this.setAnimationPlay_(false);
    } else {
      this.setUIStep(MarketingOptInStep.OVERVIEW);
      this.setAnimationPlay_(true);
    }
  }

  /**
   * The 'on-change' event handler for when the a11y navigation button setting
   * is toggled on or off.
   * @private
   */
  onA11yNavButtonsSettingChanged_() {
    this.userActed(
        ['set-a11y-button-enable', this.$.a11yNavButtonToggle.checked]);
  }

  /**
   * Returns the src of the icon.
   * @private
   */
  getIcon_() {
    return this.isCloudGamingDevice_ ? 'oobe-32:game-controller' :
                                       'oobe-32:checkmark';
  }

  /**
   * Returns the src of the illustration.
   * @private
   */
  getImageSource_() {
    return this.isDarkModeActive_ ? 'images/blazey_dark.svg' :
                                    'images/blazey_light.svg';
  }

  /**
   * Returns the src of the margin for the toggle.
   * @private
   */
  getMarginTop_() {
    return this.isCloudGamingDevice_ ? 'margin-top: 65px;' :
                                       'margin-top: 20px;';
  }



  /**
   * Returns the url of the animation asset.
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? ANIMATION_DARK_URL : ANIMATION_LIGHT_URL;
  }
}

customElements.define(MarketingOptIn.is, MarketingOptIn);