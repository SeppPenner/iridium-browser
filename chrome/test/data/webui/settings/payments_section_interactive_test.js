// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaymentsManagerImpl} from 'chrome://settings/lazy_load.js';
import {createCreditCardEntry, createEmptyCreditCardEntry, TestPaymentsManager} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {eventToPromise, whenAttributeIs} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * Helper function to simulate typing in nickname in the nickname field.
 * @param {!Element} nicknameInput
 * @param {string} nickname
 */
function typeInNickname(nicknameInput, nickname) {
  nicknameInput.value = nickname;
  nicknameInput.fire('input');
}

suite('PaymentsSectionCreditCardEditDialogTest', function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  /**
   * Creates the payments section for the given credit card list.
   * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
   * @return {!Object}
   */
  function createPaymentsSection(creditCards) {
    // Override the PaymentsManagerImpl for testing.
    const paymentsManager = new TestPaymentsManager();
    paymentsManager.data.creditCards = creditCards;
    PaymentsManagerImpl.instance_ = paymentsManager;

    const section = document.createElement('settings-payments-section');
    document.body.appendChild(section);
    flush();
    return section;
  }

  /**
   * Creates the Add Credit Card dialog. Simulate clicking "Add" button in
   * payments section.
   * @return {!Object}
   */
  function createAddCreditCardDialog() {
    const section = createPaymentsSection(/*creditCards=*/[]);
    // Simulate clicking "Add" button in payments section.
    assertTrue(!!section.$$('#addCreditCard'));
    assertFalse(!!section.$$('settings-credit-card-edit-dialog'));
    section.$$('#addCreditCard').click();
    flush();
    assertTrue(!!section.$$('settings-credit-card-edit-dialog'));
    const creditCardDialog = section.$$('settings-credit-card-edit-dialog');
    return creditCardDialog;
  }

  /**
   * Creates the Edit Credit Card dialog for existing local card by simulating
   * clicking three-dots menu button then clicking editing button of the first
   * card in the card list.
   * @param {!Array<!chrome.autofillPrivate.CreditCardEntry>} creditCards
   * @return {!Object}
   */
  function createEditCreditCardDialog(creditCards) {
    const section = createPaymentsSection(creditCards);
    // Simulate clicking three-dots menu button for the first card in the list.
    const rowShadowRoot = section.$$('#paymentsList')
                              .$$('settings-credit-card-list-entry')
                              .shadowRoot;
    assertFalse(!!rowShadowRoot.querySelector('#remoteCreditCardLink'));
    const menuButton = rowShadowRoot.querySelector('#creditCardMenu');
    assertTrue(!!menuButton);
    menuButton.click();
    flush();

    // Simulate clicking the 'Edit' button in the menu.
    const menu = section.$.creditCardSharedMenu;
    const editButton = menu.querySelector('#menuEditCreditCard');
    editButton.click();
    flush();
    assertTrue(!!section.$$('settings-credit-card-edit-dialog'));
    const creditCardDialog = section.$$('settings-credit-card-edit-dialog');
    return creditCardDialog;
  }

  test('add card dialog when nickname management disabled', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: false});
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Verify the nickname input field is not shown when nickname management is
    // disabled.
    assertFalse(!!creditCardDialog.$$('#nicknameInput'));
    assertFalse(!!creditCardDialog.$$('#nameInput'));
    // Legacy cardholder name input field is not hidden.
    assertFalse(creditCardDialog.$$('#legacyNameInput').hidden);

    // Verify the legacy cardholder name field is autofocused when nickname
    // management is enabled.
    assertTrue(
        creditCardDialog.$$('#legacyNameInput').matches(':focus-within'));
  });

  test('add card dialog when nickname management is enabled', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: true});
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Verify the nickname input field is shown when nickname management is
    // enabled.
    assertTrue(!!creditCardDialog.$$('#nicknameInput'));
    assertTrue(!!creditCardDialog.$$('#nameInput'));
    // Legacy cardholder name input field is hidden.
    assertTrue(creditCardDialog.$$('#legacyNameInput').hidden);

    // Verify the card number field is autofocused when nickname management is
    // enabled.
    assertTrue(creditCardDialog.$$('#numberInput').matches(':focus-within'));
  });

  test('save new card when nickname management is disabled', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: false});
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Fill in name to the legacy name input field and card number.
    creditCardDialog.$$('#legacyNameInput').value = 'Jane Doe';
    creditCardDialog.$$('#numberInput').value = '4111111111111111';
    flush();

    assertTrue(creditCardDialog.$.expired.hidden);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is empty when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
  });

  test('save new card when nickname management is enabled', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: true});
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // Fill in name, card number and card nickname, and trigger the on-input
    // handler.
    creditCardDialog.$$('#nameInput').value = 'Jane Doe';
    creditCardDialog.$$('#numberInput').value = '4111111111111111';
    typeInNickname(creditCardDialog.$$('#nicknameInput'), 'Grocery Card');
    flush();

    assertTrue(creditCardDialog.$.expired.hidden);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the input values are correctly passed to save-credit-card.
    // guid is undefined when saving a new card.
    assertEquals(saveEvent.detail.guid, undefined);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
  });

  test('update local card value', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: true});
    const creditCard = createCreditCardEntry();
    creditCard.name = 'Wrong name';
    creditCard.nickname = 'Shopping Card';
    const now = new Date();
    // Set the expiration year to next year to avoid expired card.
    creditCard.expirationYear = now.getFullYear() + 1;
    creditCard.cardNumber = '4444333322221111';
    const creditCardDialog = createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // For editing local card, verify displaying with existing value.
    assertEquals(creditCardDialog.$$('#nameInput').value, 'Wrong name');
    assertEquals(creditCardDialog.$$('#nicknameInput').value, 'Shopping Card');
    assertEquals(creditCardDialog.$$('#numberInput').value, '4444333322221111');

    assertTrue(creditCardDialog.$.expired.hidden);
    assertFalse(creditCardDialog.$.saveButton.disabled);

    // Update cardholder name, card number and nickname, and trigger the
    // on-input handler.
    creditCardDialog.$$('#nameInput').value = 'Jane Doe';
    creditCardDialog.$$('#numberInput').value = '4111111111111111';
    typeInNickname(creditCardDialog.$$('#nicknameInput'), 'Grocery Card');
    flush();

    const savedPromise = eventToPromise('save-credit-card', creditCardDialog);
    creditCardDialog.$.saveButton.click();
    const saveEvent = await savedPromise;

    // Verify the updated values are correctly passed to save-credit-card.
    assertEquals(saveEvent.detail.guid, creditCard.guid);
    assertEquals(saveEvent.detail.name, 'Jane Doe');
    assertEquals(saveEvent.detail.cardNumber, '4111111111111111');
    assertEquals(saveEvent.detail.nickname, 'Grocery Card');
  });

  test('show error message when input nickname is invalid', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: true});
    const creditCardDialog = createAddCreditCardDialog();

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');

    // User clicks on nickname input.
    const nicknameInput = creditCardDialog.$$('#nicknameInput');
    assertTrue(!!nicknameInput);
    nicknameInput.focus();

    const validInputs = [
      '', ' ', '~@#$%^&**(){}|<>', 'Grocery Card', 'Two percent Cashback',
      /* UTF-16 hex encoded credit card emoji */ 'Chase Freedom \uD83D\uDCB3'
    ];
    for (const nickname of validInputs) {
      typeInNickname(nicknameInput, nickname);
      assertFalse(nicknameInput.invalid);
      // Error message is hidden for valid nickname input.
      assertEquals(
          'hidden', getComputedStyle(nicknameInput.$.error).visibility);
    }

    // Verify invalid nickname inputs.
    const invalidInputs = [
      '12345', '2abc', 'abc3', 'abc4de', 'a 1 b',
      /* UTF-16 hex encoded digt 7 emoji */ 'Digit emoji: \u0037\uFE0F\u20E3'
    ];
    for (const nickname of invalidInputs) {
      typeInNickname(nicknameInput, nickname);
      assertTrue(nicknameInput.invalid);
      assertNotEquals('', nicknameInput.errorMessage);
      // Error message is shown for invalid nickname input.
      assertEquals(
          'visible', getComputedStyle(nicknameInput.$.error).visibility);
    }
    // The error message is still shown even when user does not focus on the
    // nickname field.
    nicknameInput.blur();
    assertTrue(nicknameInput.invalid);
    assertEquals('visible', getComputedStyle(nicknameInput.$.error).visibility);
  });

  test('disable save button when input nickname is invalid', async function() {
    loadTimeData.overrideValues({nicknameManagementEnabled: true});
    const creditCard = createCreditCardEntry();
    creditCard.name = 'Wrong name';
    const now = new Date();
    // Set the expiration year to next year to avoid expired card.
    creditCard.expirationYear = now.getFullYear() + 1;
    creditCard.cardNumber = '4444333322221111';
    // Edit dialog for an existing card with no nickname.
    const creditCardDialog = createEditCreditCardDialog([creditCard]);

    // Wait for the dialog to open.
    await whenAttributeIs(creditCardDialog.$$('#dialog'), 'open', '');
    // Save button is enabled for existing card with no nickname.
    assertFalse(creditCardDialog.$.saveButton.disabled);
    const nicknameInput = creditCardDialog.$$('#nicknameInput');

    typeInNickname(nicknameInput, 'invalid: 123');
    // Save button is disabled since the nickname is invalid.
    assertTrue(creditCardDialog.$.saveButton.disabled);

    typeInNickname(nicknameInput, 'valid nickname');
    // Save button is back to enabled since user updates with a valid nickname.
    assertFalse(creditCardDialog.$.saveButton.disabled);
  });
});
