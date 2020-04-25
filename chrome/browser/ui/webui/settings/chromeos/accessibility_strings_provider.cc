// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_strings_provider.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetA11ySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "A11y" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "A11y switch access" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessKeyboardConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "A11y switch access keyboard" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yLabelsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "A11y labels" search concepts.
  });
  return *tags;
}

bool AreExperimentalA11yLabelsAllowed() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityLabels);
}

bool IsSwitchAccessAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccess);
}

bool IsSwitchAccessTextAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

}  // namespace

AccessibilityStringsProvider::AccessibilityStringsProvider(
    Profile* profile,
    Delegate* per_page_delegate,
    PrefService* pref_service)
    : OsSettingsPerPageStringsProvider(profile, per_page_delegate),
      pref_service_(pref_service) {
  delegate()->AddSearchTags(GetA11ySearchConcepts());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(&AccessibilityStringsProvider::UpdateSearchTags,
                          base::Unretained(this)));
  UpdateSearchTags();
}

AccessibilityStringsProvider::~AccessibilityStringsProvider() = default;

void AccessibilityStringsProvider::AddUiStrings(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
      {"settingsSliderRoleDescription",
       IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
      {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
      {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
      {"largeMouseCursorSizeDefaultLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
      {"largeMouseCursorSizeLargeLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
      {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
      {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
      {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
      {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
      {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
      {"screenMagnifierZoomLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_LABEL},
      {"dockedMagnifierLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_LABEL},
      {"dockedMagnifierZoomLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_ZOOM_LABEL},
      {"screenMagnifierZoom2x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_2_X},
      {"screenMagnifierZoom4x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_4_X},
      {"screenMagnifierZoom6x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_6_X},
      {"screenMagnifierZoom8x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_8_X},
      {"screenMagnifierZoom10x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_10_X},
      {"screenMagnifierZoom12x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_12_X},
      {"screenMagnifierZoom14x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_14_X},
      {"screenMagnifierZoom16x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_16_X},
      {"screenMagnifierZoom18x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_18_X},
      {"screenMagnifierZoom20x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_20_X},
      {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
      {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
      {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
      {"delayBeforeClickExtremelyShort",
       IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
      {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
      {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
      {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
      {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
      {"autoclickRevertToLeftClick",
       IDS_SETTINGS_AUTOCLICK_REVERT_TO_LEFT_CLICK},
      {"autoclickStabilizeCursorPosition",
       IDS_SETTINGS_AUTOCLICK_STABILIZE_CURSOR_POSITION},
      {"autoclickMovementThresholdLabel",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LABEL},
      {"autoclickMovementThresholdExtraSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_SMALL},
      {"autoclickMovementThresholdSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_SMALL},
      {"autoclickMovementThresholdDefault",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_DEFAULT},
      {"autoclickMovementThresholdLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LARGE},
      {"autoclickMovementThresholdExtraLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_LARGE},
      {"dictationDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_DESCRIPTION},
      {"dictationLabel", IDS_SETTINGS_ACCESSIBILITY_DICTATION_LABEL},
      {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
      {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
      {"startupSoundLabel", IDS_SETTINGS_STARTUP_SOUND_LABEL},
      {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
      {"caretHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
      {"cursorHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
      {"focusHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
      {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
      {"selectToSpeakDisabledDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DISABLED_DESCRIPTION},
      {"selectToSpeakDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
      {"selectToSpeakDescriptionWithoutKeyboard",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION_WITHOUT_KEYBOARD},
      {"selectToSpeakOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
      {"switchAccessLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
      {"switchAccessOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
      {"manageSwitchAccessSettings",
       IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS},
      {"switchAssignmentHeading", IDS_SETTINGS_SWITCH_ASSIGNMENT_HEADING},
      {"switchAssignOptionNone", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_NONE},
      {"switchAssignOptionSpace", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_SPACE},
      {"switchAssignOptionEnter", IDS_SETTINGS_SWITCH_ASSIGN_OPTION_ENTER},
      {"assignSelectSwitchLabel", IDS_SETTINGS_ASSIGN_SELECT_SWITCH_LABEL},
      {"assignNextSwitchLabel", IDS_SETTINGS_ASSIGN_NEXT_SWITCH_LABEL},
      {"assignPreviousSwitchLabel", IDS_SETTINGS_ASSIGN_PREVIOUS_SWITCH_LABEL},
      {"switchAccessAutoScanHeading",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_HEADING},
      {"switchAccessAutoScanLabel", IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_LABEL},
      {"switchAccessAutoScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_SPEED_LABEL},
      {"switchAccessAutoScanKeyboardSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD_SPEED_LABEL},
      {"durationInSeconds", IDS_SETTINGS_DURATION_IN_SECONDS},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"textToSpeechHeading",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
      {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
      {"displaySettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
      {"displaySettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
      {"appearanceSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
      {"appearanceSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
      {"keyboardAndTextInputHeading",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_HEADING},
      {"keyboardSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
      {"keyboardSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
      {"mouseAndTouchpadHeading",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
      {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
      {"mouseSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
      {"audioAndCaptionsHeading",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_HEADING},
      {"additionalFeaturesTitle",
       IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
      {"manageTtsSettings", IDS_SETTINGS_MANAGE_TTS_SETTINGS},
      {"ttsSettingsLinkDescription", IDS_SETTINGS_TTS_LINK_DESCRIPTION},
      {"textToSpeechVoices", IDS_SETTINGS_TEXT_TO_SPEECH_VOICES},
      {"textToSpeechNoVoicesMessage",
       IDS_SETTINGS_TEXT_TO_SPEECH_NO_VOICES_MESSAGE},
      {"textToSpeechMoreLanguages", IDS_SETTINGS_TEXT_TO_SPEECH_MORE_LANGUAGES},
      {"textToSpeechProperties", IDS_SETTINGS_TEXT_TO_SPEECH_PROPERTIES},
      {"textToSpeechRate", IDS_SETTINGS_TEXT_TO_SPEECH_RATE},
      {"textToSpeechRateMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MINIMUM_LABEL},
      {"textToSpeechRateMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MAXIMUM_LABEL},
      {"textToSpeechPitch", IDS_SETTINGS_TEXT_TO_SPEECH_PITCH},
      {"textToSpeechPitchMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MINIMUM_LABEL},
      {"textToSpeechPitchMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MAXIMUM_LABEL},
      {"textToSpeechVolume", IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME},
      {"textToSpeechVolumeMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MINIMUM_LABEL},
      {"textToSpeechVolumeMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MAXIMUM_LABEL},
      {"percentage", IDS_SETTINGS_PERCENTAGE},
      {"defaultPercentage", IDS_SETTINGS_DEFAULT_PERCENTAGE},
      {"textToSpeechPreviewHeading",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_HEADING},
      {"textToSpeechPreviewInputLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT_LABEL},
      {"textToSpeechPreviewInput", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT},
      {"textToSpeechPreviewVoice", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_VOICE},
      {"textToSpeechPreviewPlay", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_PLAY},
      {"textToSpeechEngines", IDS_SETTINGS_TEXT_TO_SPEECH_ENGINES},
      {"tabletModeShelfNavigationButtonsSettingLabel",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_LABEL},
      {"tabletModeShelfNavigationButtonsSettingDescription",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_DESCRIPTION},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean("showExperimentalAccessibilitySwitchAccess",
                          IsSwitchAccessAllowed());
  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccessImprovedTextInput",
      IsSwitchAccessTextAllowed());

  html_source->AddBoolean("showExperimentalA11yLabels",
                          AreExperimentalA11yLabelsAllowed());

  html_source->AddBoolean(
      "showTabletModeShelfNavigationButtonsSettings",
      ash::features::IsHideShelfControlsInTabletModeEnabled());

  html_source->AddString("tabletModeShelfNavigationButtonsLearnMoreUrl",
                         chrome::kTabletModeGesturesLearnMoreURL);

  html_source->AddBoolean("enableLiveCaption",
                          base::FeatureList::IsEnabled(media::kLiveCaption));

  ::settings::AddCaptionSubpageStrings(html_source);
}

void AccessibilityStringsProvider::UpdateSearchTags() {
  if (accessibility_state_utils::IsScreenReaderEnabled() &&
      AreExperimentalA11yLabelsAllowed()) {
    delegate()->AddSearchTags(GetA11yLabelsSearchConcepts());
  } else {
    delegate()->RemoveSearchTags(GetA11yLabelsSearchConcepts());
  }

  if (IsSwitchAccessAllowed())
    delegate()->AddSearchTags(GetA11ySwitchAccessConcepts());
  else
    delegate()->RemoveSearchTags(GetA11ySwitchAccessConcepts());

  if (IsSwitchAccessTextAllowed() &&
      pref_service_->GetBoolean(
          ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled)) {
    delegate()->AddSearchTags(GetA11ySwitchAccessKeyboardConcepts());
  } else {
    delegate()->RemoveSearchTags(GetA11ySwitchAccessKeyboardConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
