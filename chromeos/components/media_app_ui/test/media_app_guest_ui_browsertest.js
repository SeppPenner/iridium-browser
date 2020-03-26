// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://media-app-guest.
 */

GEN('#include "chromeos/constants/chromeos_features.h"');

// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var MediaAppGuestUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://media-app-guest/app.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kMediaApp']};
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  preLoad() {
    document.addEventListener('DOMContentLoaded', () => {
      const mojoBindingsLite = document.createElement('script');
      mojoBindingsLite.src =
          'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
      document.head.appendChild(mojoBindingsLite);
    });
    // The guest will try and create a message pipe to its parent, since there
    // is no containing frame here, window.parent === parent. This line mocks
    // a wrapping iframe so the message pipe can still instantiate correctly.
    window.parent = document.createElement('iframe');
  }
};

// Test web workers can be spawned from chrome://media-app-guest. Errors
// will be logged in console from web_ui_browser_test.cc.
TEST_F('MediaAppGuestUIBrowserTest', 'GuestCanSpawnWorkers', () => {
  let error = null;

  try {
    const worker = new Worker('js/app_drop_target_module.js');
  } catch (e) {
    error = e;
  }

  assertEquals(error, null);
});
