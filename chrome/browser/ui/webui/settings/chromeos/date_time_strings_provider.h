// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_STRINGS_PROVIDER_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_per_page_strings_provider.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for Date and Time settings.
class DateTimeStringsProvider : public OsSettingsPerPageStringsProvider {
 public:
  DateTimeStringsProvider(Profile* profile, Delegate* per_page_delegate);
  ~DateTimeStringsProvider() override;

 private:
  // OsSettingsPerPageStringsProvider:
  void AddUiStrings(content::WebUIDataSource* html_source) override;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_STRINGS_PROVIDER_H_
