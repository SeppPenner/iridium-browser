// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/files_strings_provider.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetFilesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Files" search concepts.
  });
  return *tags;
}

}  // namespace

FilesStringsProvider::FilesStringsProvider(Profile* profile,
                                           Delegate* per_page_delegate)
    : OsSettingsPerPageStringsProvider(profile, per_page_delegate) {
  delegate()->AddSearchTags(GetFilesSearchConcepts());
}

FilesStringsProvider::~FilesStringsProvider() = default;

void FilesStringsProvider::AddUiStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
      {"filesPageTitle", IDS_OS_SETTINGS_FILES},
      {"smbSharesTitle", IDS_SETTINGS_DOWNLOADS_SMB_SHARES},
      {"smbSharesLearnMoreLabel",
       IDS_SETTINGS_DOWNLOADS_SMB_SHARES_LEARN_MORE_LABEL},
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"smbShareAddedSuccessfulMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_SUCCESS_MESSAGE},
      {"smbShareAddedErrorMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_ERROR_MESSAGE},
      {"smbShareAddedAuthFailedMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_AUTH_FAILED_MESSAGE},
      {"smbShareAddedNotFoundMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_NOT_FOUND_MESSAGE},
      {"smbShareAddedUnsupportedDeviceMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_UNSUPPORTED_DEVICE_MESSAGE},
      {"smbShareAddedMountExistsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_EXISTS_MESSAGE},
      {"smbShareAddedTooManyMountsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_TOO_MANY_MOUNTS_MESSAGE},
      {"smbShareAddedInvalidURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_URL_MESSAGE},
      {"smbShareAddedInvalidSSOURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_SSO_URL_MESSAGE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  chromeos::smb_dialog::AddLocalizedStrings(html_source);

  html_source->AddString("smbSharesLearnMoreURL",
                         GetHelpUrlWithBoard(chrome::kSmbSharesLearnMoreURL));

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile());
  html_source->AddBoolean("isActiveDirectoryUser",
                          user && user->IsActiveDirectoryUser());
}

}  // namespace settings
}  // namespace chromeos
