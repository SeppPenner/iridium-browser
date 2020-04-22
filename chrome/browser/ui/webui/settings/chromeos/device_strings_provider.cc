// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_strings_provider.h"

#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/events/devices/device_data_manager.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetDeviceSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Device" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTouchpadSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Touchpad" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetMouseSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Mouse" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetStylusSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Stylus" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayArrangementSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display arrangement" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayMirrorSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display mirror" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayUnifiedDesktopSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display with Unified Desktop" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayMultipleSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display multiple" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayExternalSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display external" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayOrientationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display orientation" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayAmbientSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display ambient" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayTouchCalibrationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display touch calibration" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayNightLightOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Display Night Light on" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetExternalStorageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "External storage" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithBatterySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Power with battery" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithLaptopLidSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Power with laptop lid" search concepts.
  });
  return *tags;
}

bool IsUnifiedDesktopAvailable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableUnifiedDesktop);
}

bool DoesDeviceSupportAmbientColor() {
  return ash::features::IsAllowAmbientEQEnabled();
}

bool IsTouchCalibrationAvailable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableTouchCalibrationSetting) &&
         display::HasExternalTouchscreenDevice();
}

void AddDeviceKeyboardStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyCommand", IDS_SETTINGS_KEYBOARD_KEY_COMMAND},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyBackspace", IDS_SETTINGS_KEYBOARD_KEY_BACKSPACE},
      {"keyboardKeyAssistant", IDS_SETTINGS_KEYBOARD_KEY_ASSISTANT},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardKeyExternalCommand",
       IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_COMMAND},
      {"keyboardKeyExternalMeta", IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_META},
      {"keyboardKeyMeta", IDS_SETTINGS_KEYBOARD_KEY_META},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardEnableAutoRepeat", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_ENABLE},
      {"keyRepeatDelay", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY},
      {"keyRepeatDelayLong", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_LONG},
      {"keyRepeatDelayShort", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_SHORT},
      {"keyRepeatRate", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE},
      {"keyRepeatRateSlow", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE_SLOW},
      {"keyRepeatRateFast", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_FAST},
      {"showKeyboardShortcutViewer",
       IDS_SETTINGS_KEYBOARD_SHOW_SHORTCUT_VIEWER},
      {"keyboardShowLanguageAndInput",
       IDS_SETTINGS_KEYBOARD_SHOW_LANGUAGE_AND_INPUT},
  };
  AddLocalizedStringsBulk(html_source, keyboard_strings);

  html_source->AddLocalizedString("keyboardKeySearch",
                                  ui::DeviceUsesKeyboardLayout2()
                                      ? IDS_SETTINGS_KEYBOARD_KEY_LAUNCHER
                                      : IDS_SETTINGS_KEYBOARD_KEY_SEARCH);
  html_source->AddLocalizedString(
      "keyboardSendFunctionKeysDescription",
      ui::DeviceUsesKeyboardLayout2()
          ? IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_LAYOUT2_DESCRIPTION
          : IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION);
}

void AddDeviceStylusStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStylusStrings[] = {
      {"stylusTitle", IDS_SETTINGS_STYLUS_TITLE},
      {"stylusEnableStylusTools", IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS},
      {"stylusAutoOpenStylusTools", IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS},
      {"stylusFindMoreAppsPrimary", IDS_SETTINGS_STYLUS_FIND_MORE_APPS_PRIMARY},
      {"stylusFindMoreAppsSecondary",
       IDS_SETTINGS_STYLUS_FIND_MORE_APPS_SECONDARY},
      {"stylusNoteTakingApp", IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LABEL},
      {"stylusNoteTakingAppEnabledOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LOCK_SCREEN_CHECKBOX},
      {"stylusNoteTakingAppKeepsLastNoteOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_KEEP_LATEST_NOTE},
      {"stylusNoteTakingAppLockScreenSettingsHeader",
       IDS_SETTINGS_STYLUS_LOCK_SCREEN_NOTES_TITLE},
      {"stylusNoteTakingAppNoneAvailable",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_NONE_AVAILABLE},
      {"stylusNoteTakingAppWaitingForAndroid",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_WAITING_FOR_ANDROID}};
  AddLocalizedStringsBulk(html_source, kStylusStrings);
}

void AddDeviceDisplayStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDisplayStrings[] = {
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangementText", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TEXT},
      {"displayArrangementTitle", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TITLE},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMirrorDisplayName", IDS_SETTINGS_DISPLAY_MIRROR_DISPLAY_NAME},
      {"displayAmbientColorTitle", IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_TITLE},
      {"displayAmbientColorSubtitle",
       IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_SUBTITLE},
      {"displayNightLightLabel", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_LABEL},
      {"displayNightLightOnAtSunset",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_ON_AT_SUNSET},
      {"displayNightLightOffAtSunrise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_OFF_AT_SUNRISE},
      {"displayNightLightScheduleCustom",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_CUSTOM},
      {"displayNightLightScheduleLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_LABEL},
      {"displayNightLightScheduleNever",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_NEVER},
      {"displayNightLightScheduleSunsetToSunRise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_SUNSET_TO_SUNRISE},
      {"displayNightLightStartTime",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_START_TIME},
      {"displayNightLightStopTime", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_STOP_TIME},
      {"displayNightLightText", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEXT},
      {"displayNightLightTemperatureLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMPERATURE_LABEL},
      {"displayNightLightTempSliderMaxLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MAX_LABEL},
      {"displayNightLightTempSliderMinLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MIN_LABEL},
      {"displayUnifiedDesktop", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP},
      {"displayUnifiedDesktopOn", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_ON},
      {"displayUnifiedDesktopOff", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_OFF},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayResolutionSublabel", IDS_SETTINGS_DISPLAY_RESOLUTION_SUBLABEL},
      {"displayResolutionMenuItem", IDS_SETTINGS_DISPLAY_RESOLUTION_MENU_ITEM},
      {"displayResolutionInterlacedMenuItem",
       IDS_SETTINGS_DISPLAY_RESOLUTION_INTERLACED_MENU_ITEM},
      {"displayZoomTitle", IDS_SETTINGS_DISPLAY_ZOOM_TITLE},
      {"displayZoomSublabel", IDS_SETTINGS_DISPLAY_ZOOM_SUBLABEL},
      {"displayZoomValue", IDS_SETTINGS_DISPLAY_ZOOM_VALUE},
      {"displayZoomLogicalResolutionText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_TEXT},
      {"displayZoomNativeLogicalResolutionNativeText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_NATIVE_TEXT},
      {"displayZoomLogicalResolutionDefaultText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_DEFAULT_TEXT},
      {"displaySizeSliderMinLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MINIMUM},
      {"displaySizeSliderMaxLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MAXIMUM},
      {"displayScreenTitle", IDS_SETTINGS_DISPLAY_SCREEN},
      {"displayScreenExtended", IDS_SETTINGS_DISPLAY_SCREEN_EXTENDED},
      {"displayScreenPrimary", IDS_SETTINGS_DISPLAY_SCREEN_PRIMARY},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
      {"displayOrientationAutoRotate",
       IDS_SETTINGS_DISPLAY_ORIENTATION_AUTO_ROTATE},
      {"displayOverscanPageText", IDS_SETTINGS_DISPLAY_OVERSCAN_TEXT},
      {"displayOverscanPageTitle", IDS_SETTINGS_DISPLAY_OVERSCAN_TITLE},
      {"displayOverscanSubtitle", IDS_SETTINGS_DISPLAY_OVERSCAN_SUBTITLE},
      {"displayOverscanInstructions",
       IDS_SETTINGS_DISPLAY_OVERSCAN_INSTRUCTIONS},
      {"displayOverscanResize", IDS_SETTINGS_DISPLAY_OVERSCAN_RESIZE},
      {"displayOverscanPosition", IDS_SETTINGS_DISPLAY_OVERSCAN_POSITION},
      {"displayOverscanReset", IDS_SETTINGS_DISPLAY_OVERSCAN_RESET},
      {"displayTouchCalibrationTitle",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TITLE},
      {"displayTouchCalibrationText",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TEXT}};
  AddLocalizedStringsBulk(html_source, kDisplayStrings);

  html_source->AddBoolean("unifiedDesktopAvailable",
                          IsUnifiedDesktopAvailable());

  html_source->AddBoolean("listAllDisplayModes",
                          display::features::IsListAllDisplayModesEnabled());

  html_source->AddBoolean("deviceSupportsAmbientColor",
                          DoesDeviceSupportAmbientColor());

  html_source->AddBoolean("enableTouchCalibrationSetting",
                          IsTouchCalibrationAvailable());

  html_source->AddBoolean(
      "allowDisplayIdentificationApi",
      base::FeatureList::IsEnabled(ash::features::kDisplayIdentification));

  html_source->AddString("invalidDisplayId",
                         base::NumberToString(display::kInvalidDisplayId));
}

void AddDeviceStorageStrings(content::WebUIDataSource* html_source,
                             bool is_external_storage_page_available) {
  static constexpr webui::LocalizedString kStorageStrings[] = {
      {"storageTitle", IDS_SETTINGS_STORAGE_TITLE},
      {"storageItemInUse", IDS_SETTINGS_STORAGE_ITEM_IN_USE},
      {"storageItemAvailable", IDS_SETTINGS_STORAGE_ITEM_AVAILABLE},
      {"storageItemSystem", IDS_SETTINGS_STORAGE_ITEM_SYSTEM},
      {"storageItemMyFiles", IDS_SETTINGS_STORAGE_ITEM_MY_FILES},
      {"storageItemBrowsingData", IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA},
      {"storageItemApps", IDS_SETTINGS_STORAGE_ITEM_APPS},
      {"storageItemCrostini", IDS_SETTINGS_STORAGE_ITEM_CROSTINI},
      {"storageItemOtherUsers", IDS_SETTINGS_STORAGE_ITEM_OTHER_USERS},
      {"storageSizeComputing", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"storageSizeUnknown", IDS_SETTINGS_STORAGE_SIZE_UNKNOWN},
      {"storageSpaceLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE},
      {"storageSpaceLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1},
      {"storageSpaceLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2},
      {"storageSpaceCriticallyLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE},
      {"storageSpaceCriticallyLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1},
      {"storageSpaceCriticallyLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2},
      {"storageExternal", IDS_SETTINGS_STORAGE_EXTERNAL},
      {"storageExternalStorageEmptyListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_EMPTY_LIST_HEADER},
      {"storageExternalStorageListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_LIST_HEADER},
      {"storageOverviewAriaLabel", IDS_SETTINGS_STORAGE_OVERVIEW_ARIA_LABEL}};
  AddLocalizedStringsBulk(html_source, kStorageStrings);

  html_source->AddBoolean("androidEnabled", is_external_storage_page_available);

  html_source->AddString(
      "storageAndroidAppsExternalDrivesNote",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_STORAGE_ANDROID_APPS_ACCESS_EXTERNAL_DRIVES_NOTE,
          base::ASCIIToUTF16(chrome::kArcExternalStorageLearnMoreURL)));
}

void AddDevicePowerStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPowerStrings[] = {
      {"powerTitle", IDS_SETTINGS_POWER_TITLE},
      {"powerSourceLabel", IDS_SETTINGS_POWER_SOURCE_LABEL},
      {"powerSourceBattery", IDS_SETTINGS_POWER_SOURCE_BATTERY},
      {"powerSourceAcAdapter", IDS_SETTINGS_POWER_SOURCE_AC_ADAPTER},
      {"powerSourceLowPowerCharger",
       IDS_SETTINGS_POWER_SOURCE_LOW_POWER_CHARGER},
      {"calculatingPower", IDS_SETTINGS_POWER_SOURCE_CALCULATING},
      {"powerIdleLabel", IDS_SETTINGS_POWER_IDLE_LABEL},
      {"powerIdleWhileChargingLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_LABEL},
      {"powerIdleWhileChargingAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_ARIA_LABEL},
      {"powerIdleWhileOnBatteryLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_LABEL},
      {"powerIdleWhileOnBatteryAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_ARIA_LABEL},
      {"powerIdleDisplayOffSleep", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF_SLEEP},
      {"powerIdleDisplayOff", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF},
      {"powerIdleDisplayOn", IDS_SETTINGS_POWER_IDLE_DISPLAY_ON},
      {"powerIdleOther", IDS_SETTINGS_POWER_IDLE_OTHER},
      {"powerLidSleepLabel", IDS_SETTINGS_POWER_LID_CLOSED_SLEEP_LABEL},
      {"powerLidSignOutLabel", IDS_SETTINGS_POWER_LID_CLOSED_SIGN_OUT_LABEL},
      {"powerLidShutDownLabel", IDS_SETTINGS_POWER_LID_CLOSED_SHUT_DOWN_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kPowerStrings);
}

}  // namespace

DeviceStringsProvider::DeviceStringsProvider(Profile* profile,
                                             Delegate* per_page_delegate)
    : OsSettingsPerPageStringsProvider(profile, per_page_delegate) {
  delegate()->AddSearchTags(GetDeviceSearchConcepts());

  if (features::ShouldShowExternalStorageSettings(profile))
    delegate()->AddSearchTags(GetExternalStorageSearchConcepts());

  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  if (power_manager_client) {
    power_manager_client->AddObserver(this);

    const base::Optional<power_manager::PowerSupplyProperties>& last_status =
        power_manager_client->GetLastStatus();
    if (last_status)
      PowerChanged(*last_status);

    // Determine whether to show laptop lid power settings.
    power_manager_client->GetSwitchStates(
        base::BindOnce(&DeviceStringsProvider::OnGotSwitchStates,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Keyboard/mouse search tags are added/removed dynamically.
  pointer_device_observer_.Init();
  pointer_device_observer_.AddObserver(this);
  pointer_device_observer_.CheckDevices();

  // Stylus search tags are added/removed dynamically.
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  UpdateStylusSearchTags();

  // Display search tags are added/removed dynamically.
  ash::BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
  mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver> observer;
  cros_display_config_observer_receiver_.Bind(
      observer.InitWithNewEndpointAndPassReceiver());
  cros_display_config_->AddObserver(std::move(observer));
  OnDisplayConfigChanged();

  // Night Light settings are added/removed dynamically.
  ash::NightLightController* night_light_controller =
      ash::NightLightController::GetInstance();
  if (night_light_controller) {
    ash::NightLightController::GetInstance()->AddObserver(this);
    OnNightLightEnabledChanged(
        ash::NightLightController::GetInstance()->GetEnabled());
  }
}

DeviceStringsProvider::~DeviceStringsProvider() {
  pointer_device_observer_.RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  if (power_manager_client)
    power_manager_client->RemoveObserver(this);

  ash::NightLightController* night_light_controller =
      ash::NightLightController::GetInstance();
  if (night_light_controller)
    night_light_controller->RemoveObserver(this);
}

void DeviceStringsProvider::AddUiStrings(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDeviceStrings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"scrollLabel", IDS_SETTINGS_SCROLL_LABEL},
      {"touchPadScrollLabel", IDS_OS_SETTINGS_TOUCHPAD_REVERSE_SCROLL_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kDeviceStrings);

  AddDevicePointersStrings(html_source);
  AddDeviceKeyboardStrings(html_source);
  AddDeviceStylusStrings(html_source);
  AddDeviceDisplayStrings(html_source);
  AddDeviceStorageStrings(
      html_source, features::ShouldShowExternalStorageSettings(profile()));
  AddDevicePowerStrings(html_source);
}

void DeviceStringsProvider::TouchpadExists(bool exists) {
  if (exists)
    delegate()->AddSearchTags(GetTouchpadSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetTouchpadSearchConcepts());
}

void DeviceStringsProvider::MouseExists(bool exists) {
  if (exists)
    delegate()->AddSearchTags(GetMouseSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetMouseSearchConcepts());
}

void DeviceStringsProvider::OnDeviceListsComplete() {
  UpdateStylusSearchTags();
}

void DeviceStringsProvider::OnNightLightEnabledChanged(bool enabled) {
  OnDisplayConfigChanged();
}

void DeviceStringsProvider::OnDisplayConfigChanged() {
  cros_display_config_->GetDisplayUnitInfoList(
      /*single_unified=*/true,
      base::BindOnce(&DeviceStringsProvider::OnGetDisplayUnitInfoList,
                     base::Unretained(this)));
}

void DeviceStringsProvider::PowerChanged(
    const power_manager::PowerSupplyProperties& properties) {
  if (properties.battery_state() !=
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    delegate()->AddSearchTags(GetPowerWithBatterySearchConcepts());
  }
}

void DeviceStringsProvider::OnGetDisplayUnitInfoList(
    std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list) {
  cros_display_config_->GetDisplayLayoutInfo(base::BindOnce(
      &DeviceStringsProvider::OnGetDisplayLayoutInfo, base::Unretained(this),
      std::move(display_unit_info_list)));
}

void DeviceStringsProvider::OnGetDisplayLayoutInfo(
    std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list,
    ash::mojom::DisplayLayoutInfoPtr display_layout_info) {
  bool has_multiple_displays = display_unit_info_list.size() > 1u;

  // Mirroring mode is active if there's at least one display and if there's a
  // mirror source ID.
  bool is_mirrored = !display_unit_info_list.empty() &&
                     display_layout_info->mirror_source_id.has_value();

  bool has_internal_display = false;
  bool has_external_display = false;
  bool unified_desktop_mode = false;
  for (const auto& display_unit_info : display_unit_info_list) {
    has_internal_display |= display_unit_info->is_internal;
    has_external_display |= !display_unit_info->is_internal;

    unified_desktop_mode |= display_unit_info->is_primary &&
                            display_layout_info->layout_mode ==
                                ash::mojom::DisplayLayoutMode::kUnified;
  }

  // Arrangement UI.
  if (has_multiple_displays || is_mirrored)
    delegate()->AddSearchTags(GetDisplayArrangementSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayArrangementSearchConcepts());

  // Mirror toggle.
  if (is_mirrored || (!unified_desktop_mode && has_multiple_displays))
    delegate()->AddSearchTags(GetDisplayMirrorSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayMirrorSearchConcepts());

  // Unified Desktop toggle.
  if (unified_desktop_mode ||
      (IsUnifiedDesktopAvailable() && has_multiple_displays && !is_mirrored)) {
    delegate()->AddSearchTags(GetDisplayUnifiedDesktopSearchConcepts());
  } else {
    delegate()->RemoveSearchTags(GetDisplayUnifiedDesktopSearchConcepts());
  }

  // Multiple displays UI.
  if (has_multiple_displays)
    delegate()->AddSearchTags(GetDisplayMultipleSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayMultipleSearchConcepts());

  // External display settings.
  if (has_external_display)
    delegate()->AddSearchTags(GetDisplayExternalSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayExternalSearchConcepts());

  // Orientation settings.
  if (!unified_desktop_mode)
    delegate()->AddSearchTags(GetDisplayOrientationSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayOrientationSearchConcepts());

  // Ambient color settings.
  if (DoesDeviceSupportAmbientColor() && has_internal_display)
    delegate()->AddSearchTags(GetDisplayAmbientSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayAmbientSearchConcepts());

  // Touch calibration settings.
  if (IsTouchCalibrationAvailable())
    delegate()->AddSearchTags(GetDisplayTouchCalibrationSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayTouchCalibrationSearchConcepts());

  // Night Light on settings.
  if (ash::NightLightController::GetInstance()->GetEnabled())
    delegate()->AddSearchTags(GetDisplayNightLightOnSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetDisplayNightLightOnSearchConcepts());
}

void DeviceStringsProvider::OnGotSwitchStates(
    base::Optional<PowerManagerClient::SwitchStates> result) {
  if (result && result->lid_state != PowerManagerClient::LidState::NOT_PRESENT)
    delegate()->AddSearchTags(GetPowerWithLaptopLidSearchConcepts());
}

void DeviceStringsProvider::UpdateStylusSearchTags() {
  // If not yet complete, wait for OnDeviceListsComplete() callback.
  if (!ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete())
    return;

  // TODO(https://crbug.com/1071905): Only show stylus settings if a stylus has
  // been set up. HasStylusInput() will return true for any stylus-compatible
  // device, even if it doesn't have a stylus.
  if (ash::stylus_utils::HasStylusInput())
    delegate()->AddSearchTags(GetStylusSearchConcepts());
  else
    delegate()->RemoveSearchTags(GetStylusSearchConcepts());
}

void DeviceStringsProvider::AddDevicePointersStrings(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPointersStrings[] = {
      {"mouseTitle", IDS_SETTINGS_MOUSE_TITLE},
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"mouseAndTouchpadTitle", IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
      {"touchpadSpeed", IDS_SETTINGS_TOUCHPAD_SPEED_LABEL},
      {"pointerSlow", IDS_SETTINGS_POINTER_SPEED_SLOW_LABEL},
      {"pointerFast", IDS_SETTINGS_POINTER_SPEED_FAST_LABEL},
      {"mouseScrollSpeed", IDS_SETTINGS_MOUSE_SCROLL_SPEED_LABEL},
      {"mouseSpeed", IDS_SETTINGS_MOUSE_SPEED_LABEL},
      {"mouseSwapButtons", IDS_SETTINGS_MOUSE_SWAP_BUTTONS_LABEL},
      {"mouseReverseScroll", IDS_SETTINGS_MOUSE_REVERSE_SCROLL_LABEL},
      {"mouseAccelerationLabel", IDS_SETTINGS_MOUSE_ACCELERATION_LABEL},
      {"mouseScrollAccelerationLabel",
       IDS_SETTINGS_MOUSE_SCROLL_ACCELERATION_LABEL},
      {"touchpadAccelerationLabel", IDS_SETTINGS_TOUCHPAD_ACCELERATION_LABEL},
      {"touchpadScrollAccelerationLabel",
       IDS_SETTINGS_TOUCHPAD_SCROLL_ACCELERATION_LABEL},
      {"touchpadScrollSpeed", IDS_SETTINGS_TOUCHPAD_SCROLL_SPEED_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kPointersStrings);

  html_source->AddString("naturalScrollLearnMoreLink",
                         GetHelpUrlWithBoard(chrome::kNaturalScrollHelpURL));

  html_source->AddBoolean(
      "allowDisableMouseAcceleration",
      base::FeatureList::IsEnabled(::features::kAllowDisableMouseAcceleration));
  html_source->AddBoolean(
      "allowScrollSettings",
      base::FeatureList::IsEnabled(::chromeos::features::kAllowScrollSettings));
}

}  // namespace settings
}  // namespace chromeos
