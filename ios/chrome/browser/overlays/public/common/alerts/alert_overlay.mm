// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/alerts/alert_overlay.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace alert_overlays {

#pragma mark - ButtonConfig

ButtonConfig::ButtonConfig(NSString* title, UIAlertActionStyle style)
    : title(title), style(style) {
  DCHECK_GT(title.length, 0U);
}

#pragma mark - AlertRequest

OVERLAY_USER_DATA_SETUP_IMPL(AlertRequest);

AlertRequest::AlertRequest(NSString* title,
                           NSString* message,
                           NSString* accessibility_identifier,
                           NSArray<TextFieldConfiguration*>* text_field_configs,
                           const std::vector<ButtonConfig>& button_configs,
                           ResponseConverter response_converter)
    : title_(title),
      message_(message),
      accessibility_identifier_(accessibility_identifier),
      text_field_configs_(text_field_configs),
      button_configs_(button_configs),
      response_converter_(response_converter) {
  // All alerts need a title, a message, or both.
  DCHECK(title_.length || message_.length);
  // All alerts must have at least one button.
  DCHECK_GT(button_configs_.size(), 0U);
  // All alerts must have a response converter to create feature-specific
  // responses from alert UI interaction information.
  DCHECK(!response_converter.is_null());
}

AlertRequest::~AlertRequest() = default;

#pragma mark - AlertResponse

OVERLAY_USER_DATA_SETUP_IMPL(AlertResponse);

AlertResponse::AlertResponse(size_t tapped_button_index,
                             NSArray<NSString*>* text_field_values)
    : tapped_button_index_(tapped_button_index),
      text_field_values_(text_field_values) {}

AlertResponse::~AlertResponse() = default;

}  // alert_overlays
