// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design offline login.
 */

/* #js_imports_placeholder */

const DEFAULT_EMAIL_DOMAIN = '@9ma1l.qjz9zk';
const INPUT_EMAIL_PATTERN = '^[a-zA-Z0-9.!#$%&\'*+=?^_`{|}~-]+(@[^\\s@]+)?$';

const LOGIN_SECTION = {
  EMAIL: 'emailSection',
  PASSWORD: 'passwordSection',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const OfflineLoginBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   emailInput: CrInputElement,
 *   passwordInput: CrInputElement,
 *   dialog: OobeContentDialogElement,
 *   forgotPasswordDlg: CrDialogElement,
 *   onlineRequiredDialog: CrDialogElement,
 * }}
 */
OfflineLoginBase.$;

/**
 * @polymer
 */
class OfflineLogin extends OfflineLoginBase {
  static get is() {
    return 'offline-login-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Domain manager.
       * @type {?string}
       */
      manager: {
        type: String,
        value: '',
      },

      /**
       * E-mail domain including initial '@' sign.
       * @type {?string}
       */
      emailDomain: {
        type: String,
        value: '',
      },

      /**
       * |domain| or empty string, depending on |email_| value.
       */
      displayDomain_: {
        type: String,
        computed: 'computeDomain_(emailDomain, email_)',
      },

      /**
       * Current value of e-mail input field.
       */
      email_: String,

      /**
       * Current value of password input field.
       */
      password_: String,

      /**
       * Proper e-mail with domain, displayed on password page.
       */
      fullEmail_: String,

      activeSection: {
        type: String,
        value: LOGIN_SECTION.EMAIL,
      },

      animationInProgress: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    this.email_ = '';
    this.password_ = '';
    this.fullEmail_ = '';
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['reset',
            'proceedToPasswordPage',
            'showOnlineRequiredDialog',
            'showPasswordMismatchMessage',
          ];
  }
  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('OfflineLoginScreen');
  }

  attached() {
    super.attached();
    if (this.isRTL_()) {
      this.setAttribute('rtl', '');
    }
  }

  focus() {
    if (this.isEmailSectionActive_()) {
      this.$.emailInput.focusInput();
    } else {
      this.$.passwordInput.focusInput();
    }
  }

  back() {
    this.switchToEmailCard(true /* animated */);
  }

  cancel() {
    if (this.disabled) {
      return;
    }
    this.onBackButtonClicked_();
  }

  /**
   *
   * @param {Object} params
   */
  onBeforeShow(params) {
    this.reset();
    if ('enterpriseDomainManager' in params) {
      this.manager = params['enterpriseDomainManager'];
    }
    if ('emailDomain' in params) {
      this.emailDomain = '@' + params['emailDomain'];
    }
    this.$.emailInput.pattern = INPUT_EMAIL_PATTERN;
    if (!this.email_) {
      this.switchToEmailCard(false /* animated */);
    }
  }

  reset() {
    this.animationInProgress = false;
    this.disabled = false;
    this.emailDomain = '';
    this.manager = '';
    this.email_ = '';
    this.fullEmail_ = '';
    this.$.emailInput.invalid = false;
    this.$.passwordInput.invalid = false;
    this.activeSection = LOGIN_SECTION.EMAIL;
  }

  proceedToPasswordPage() {
    this.switchToPasswordCard(true /* animated */);
  }

  showOnlineRequiredDialog() {
    this.disabled = true;
    this.$.onlineRequiredDialog.showModal();
  }

  onForgotPasswordClicked_() {
    this.disabled = true;
    this.$.forgotPasswordDlg.showModal();
  }

  onForgotPasswordCloseTap_() {
    this.$.forgotPasswordDlg.close();
  }

  onOnlineRequiredDialogCloseTap_() {
    this.$.onlineRequiredDialog.close();
    this.userActed('cancel');
  }

  onDialogOverlayClosed_() {
    this.disabled = false;
  }

  isRTL_() {
    return !!document.querySelector('html[dir=rtl]');
  }

  isEmailSectionActive_() {
    return this.activeSection == LOGIN_SECTION.EMAIL;
  }

  /**
   * @param {boolean} animated
   */
  switchToEmailCard(animated) {
    this.$.emailInput.invalid = false;
    this.$.passwordInput.invalid = false;
    this.password_ = '';
    if (this.isEmailSectionActive_()) {
      return;
    }

    this.animationInProgress = animated;
    this.disabled = animated;
    this.activeSection = LOGIN_SECTION.EMAIL;
  }

  /**
   * @param {boolean} animated
   */
  switchToPasswordCard(animated) {
    if (!this.isEmailSectionActive_()) {
      return;
    }

    this.animationInProgress = animated;
    this.disabled = animated;
    this.activeSection = LOGIN_SECTION.PASSWORD;
  }

  onSlideAnimationEnd_() {
    this.animationInProgress = false;
    this.disabled = false;
    this.focus();
  }

  onEmailSubmitted_() {
    if (this.$.emailInput.validate()) {
      this.fullEmail_ = this.computeFullEmail_(this.email_);
      this.userActed(['email-submitted', this.fullEmail_]);
    } else {
      this.$.emailInput.focusInput();
    }
  }

  onPasswordSubmitted_() {
    if (!this.$.passwordInput.validate()) {
      return;
    }
    this.email_ = this.fullEmail_;
    this.userActed(['complete-authentication', this.email_, this.password_]);
    this.disabled = true;
  }

  onBackButtonClicked_() {
    if (!this.isEmailSectionActive_()) {
      this.switchToEmailCard(true);
    } else {
      this.userActed('cancel');
    }
  }

  onNextButtonClicked_() {
    if (this.isEmailSectionActive_()) {
      this.onEmailSubmitted_();
      return;
    }
    this.onPasswordSubmitted_();
  }

  /**
   * @param {string} domain
   * @param {string} email
   */
  computeDomain_(domain, email) {
    if (email && email.indexOf('@') !== -1) {
      return '';
    }
    return domain;
  }

  /**
   * @param {string} email
   */
  computeFullEmail_(email) {
    if (email.indexOf('@') === -1) {
      if (this.emailDomain) {
        email = email + this.emailDomain;
      } else {
        email = email + DEFAULT_EMAIL_DOMAIN;
      }
    }
    return email;
  }

  showPasswordMismatchMessage() {
    this.$.passwordInput.invalid = true;
    this.disabled = false;
    this.$.passwordInput.focusInput();
  }

  /**
   * @param {string} email
   */
  setEmailForTest(email) {
    this.email_ = email;
  }
}

customElements.define(OfflineLogin.is, OfflineLogin);
