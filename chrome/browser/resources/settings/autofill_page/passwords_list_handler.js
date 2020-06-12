// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import './password_edit_dialog.js';
import './password_list_item.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './password_edit_dialog.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {RemovalResult} from './remove_password_behavior.js';

Polymer({
  is: 'passwords-list-handler',

  _template: html`{__html_template__}`,

  properties: {

    /**
     * The model for any active menus or dialogs. The value is reset to null
     * whenever actions from the menus/dialogs are concluded.
     * @private {?PasswordListItemElement}
     */
    activePassword: {
      type: Object,
      value: null,
    },

    /**
     * Whether the edit dialog and removal notification should show information
     * about which location(s) a password is stored.
     */
    shouldShowStorageDetails: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether an option for moving a password to the account should be offered
     * in the overflow menu.
     */
    shouldShowMoveToAccountOption: {
      type: Boolean,
      value: false,
    },

    // <if expr="chromeos">
    /** @type {BlockingRequestManager} */
    tokenRequestManager: Object,
    // </if>

    /** @private */
    enablePasswordCheck_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enablePasswordCheck');
      }
    },

    /** @private */
    showPasswordEditDialog_: {type: Boolean, value: false},

    /**
     * The element to return focus to, when the currently active dialog is
     * closed.
     * @private {?HTMLElement}
     */
    activeDialogAnchorElement_: {type: Object, value: null},

  },

  behaviors: [
    I18nBehavior,
  ],

  listeners: {
    'password-menu-tap': 'onPasswordMenuTap_',
  },

  /** @override */
  detached() {
    if (getToastManager().isToastOpen) {
      getToastManager().hide();
    }
  },

  /**
   * Closes the toast manager.
   */
  onSavedPasswordOrExceptionRemoved() {
    getToastManager().hide();
  },

  /**
   * Opens the password action menu.
   * @param {!Event<!{target: !HTMLElement, listItem:
   *     !PasswordListItemElement}>} event
   * @private
   */
  onPasswordMenuTap_(event) {
    const target = event.detail.target;

    this.activePassword = event.detail.listItem;
    this.$.menu.showAt(target);
    this.activeDialogAnchor_ = target;
  },

  /**
   * Shows the edit password dialog.
   * @param {!Event} e
   * @private
   */
  onMenuEditPasswordTap_(e) {
    e.preventDefault();
    this.$.menu.close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activePassword = null;
  },

  /**
   * Copy selected password to clipboard.
   * @private
   */
  onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.activePassword.entry.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(() => {
          this.activePassword = null;
        })
        .catch(error => {
          // <if expr="chromeos">
          // If no password was found, refresh auth token and retry.
          this.tokenRequestManager.request(
              this.onMenuCopyPasswordButtonTap_.bind(this));
          // </if>});
        });
    this.$.menu.close();
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_() {
    // TODO(crbug.com/1049141): Open removal dialog if password is present in
    // both locations. Add tests for when no password is selected in the dialog.
    /** @type {!RemovalResult} */
    const result = this.activePassword.requestRemove();

    if (!result.removedFromDevice && !result.removedFromAccount) {
      this.$.menu.close();
      this.activePassword = null;
      return;
    }

    let text = this.i18n('passwordDeleted');
    if (this.shouldShowStorageDetails) {
      if (result.removedFromAccount && result.removedFromDevice) {
        text = this.i18n('passwordDeletedFromAccountAndDevice');
      } else if (result.removedFromAccount) {
        text = this.i18n('passwordDeletedFromAccount');
      } else {
        text = this.i18n('passwordDeletedFromDevice');
      }
    }
    getToastManager().show(text);
    this.fire('iron-announce', {text: this.i18n('undoDescription')});

    this.$.menu.close();
    this.activePassword = null;
  },

  /**
   * @private
   */
  onUndoButtonClick_() {
    PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
    this.onSavedPasswordOrExceptionRemoved();
  },

  /**
   * Should only be called when |activePassword| has a device copy.
   * @private
   */
  onMenuMovePasswordToAccountTap_() {
    assert(this.activePassword.entry.isPresentOnDevice());
    PasswordManagerImpl.getInstance()
        .movePasswordToAccount(/** @type {number} */
                               (this.activePassword.entry.deviceId));
    this.$.menu.close();
    this.activePassword = null;
  }

});
