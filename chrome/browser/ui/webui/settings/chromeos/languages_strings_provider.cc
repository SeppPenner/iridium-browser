// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/languages_strings_provider.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetLanguagesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Languages" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetLanguagesAssistivePersonalInfoSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Languages assistive personal info" search
      // concepts.
  });
  return *tags;
}

bool IsAssistivePersonalInfoAllowed() {
  return !features::IsGuestModeActive() &&
         base::FeatureList::IsEnabled(
             ::chromeos::features::kAssistPersonalInfo);
}

void AddSmartInputsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"smartInputsTitle", IDS_SETTINGS_SMART_INPUTS_TITLE},
      {"personalInfoSuggestionTitle",
       IDS_SETTINGS_SMART_INPUTS_PERSONAL_INFO_TITLE},
      {"personalInfoSuggestionDescription",
       IDS_SETTINGS_SMART_INPUTS_PERSONAL_INFO_DESCRIPTION},
      {"showPersonalInfoSuggestion",
       IDS_SETTINGS_SMART_INPUTS_SHOW_PERSONAL_INFO},
      {"managePersonalInfo", IDS_SETTINGS_SMART_INPUTS_MANAGE_PERSONAL_INFO},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("allowAssistivePersonalInfo",
                          IsAssistivePersonalInfoAllowed());
}

}  // namespace

LanguagesStringsProvider::LanguagesStringsProvider(Profile* profile,
                                                   Delegate* per_page_delegate)
    : OsSettingsPerPageStringsProvider(profile, per_page_delegate) {
  delegate()->AddSearchTags(GetLanguagesSearchConcepts());

  if (IsAssistivePersonalInfoAllowed()) {
    delegate()->AddSearchTags(
        GetLanguagesAssistivePersonalInfoSearchConcepts());
  }
}

LanguagesStringsProvider::~LanguagesStringsProvider() = default;

void LanguagesStringsProvider::AddUiStrings(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"orderLanguagesInstructions",
       IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"osLanguagesListTitle", IDS_OS_SETTINGS_LANGUAGES_LIST_TITLE},
      {"inputMethodsListTitle",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
      {"inputMethodEnabled", IDS_SETTINGS_LANGUAGES_INPUT_METHOD_ENABLED},
      {"inputMethodsExpandA11yLabel",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_EXPAND_ACCESSIBILITY_LABEL},
      {"inputMethodsManagedbyPolicy",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGED_BY_POLICY},
      {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
      {"manageInputMethodsPageTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE},
      {"showImeMenu", IDS_SETTINGS_LANGUAGES_SHOW_IME_MENU},
      {"displayLanguageRestart",
       IDS_SETTINGS_LANGUAGES_RESTART_TO_DISPLAY_LANGUAGE},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
      {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  AddSmartInputsStrings(html_source);

  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
}

}  // namespace settings
}  // namespace chromeos
