// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Subpages
import './appearance_page/appearance_fonts_page.m.js';
import './autofill_page/password_check.m.js';
import './autofill_page/passwords_section.m.js';
import './autofill_page/payments_section.m.js';
import './clear_browsing_data_dialog/clear_browsing_data_dialog.m.js';
import './search_engines_page/search_engines_page.m.js';

// TODO(https://crbug.com/1026426): Uncomment these imports once the pages have
// been migrated to Polymer 3.
// import './privacy_page/cookies_page.m.js';
// import './privacy_page/security_keys_subpage.m.js';
// import './privacy_page/security_page.m.js';
// import './site_settings/all_sites.m.js';
import './site_settings/site_data_details_subpage.m.js';
// import 'site_settings_page/site_settings_page.m.js';
// import 'site_settings/category_default_setting.m.js';
// import 'site_settings/category_setting_exceptions.m.js';
// import 'site_settings/chooser_exception_list.m.js';
// import 'site_settings/media_picker.m.js';
// import 'site_settings/pdf_documents.m.js';
// import 'site_settings/protocol_handlers.m.js';
// import 'site_settings/site_data.m.js';
// import 'site_settings/site_details.m.js';
// import 'site_settings/zoom_levels.m.js';

// <if expr="not chromeos">
// import 'people_page/manage_profile.m.js';
// </if>
// import 'people_page/sync_account_control.m.js';

// <if expr="use_nss_certs">
import 'chrome://resources/cr_components/certificate_manager/certificate_manager.m.js';
// </if>

// Sections
import './a11y_page/a11y_page.m.js';
import './downloads_page/downloads_page.m.js';
import './languages_page/languages_page.m.js';
import './printing_page/printing_page.m.js';
import './reset_page/reset_page.m.js';

// <if expr="not chromeos">
import './system_page/system_page.m.js';
// </if>

// <if expr="not is_macosx">
import './languages_page/edit_dictionary_page.m.js';
// </if>

// Dependencies imported separately for tests
import './controls/controlled_button.m.js';
import './controls/settings_slider.m.js';
import './controls/settings_textarea.m.js';
// <if expr="_google_chrome and is_win">
import './incompatible_applications_page/incompatible_applications_page.m.js';
// </if>
import './search_engines_page/omnibox_extension_entry.m.js';
import './search_engines_page/search_engine_dialog.m.js';
import './search_engines_page/search_engine_entry.m.js';

// <if expr="_google_chrome and is_win">
export {CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW} from './chrome_cleanup_page/items_to_remove_list.m.js';
export {ChromeCleanupIdleReason} from './chrome_cleanup_page/chrome_cleanup_page.m.js';
export {ChromeCleanupProxyImpl} from './chrome_cleanup_page/chrome_cleanup_proxy.m.js';
export {IncompatibleApplication, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_page/incompatible_applications_browser_proxy.m.js';
// </if>

export {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';

export {FontsBrowserProxy, FontsBrowserProxyImpl} from './appearance_page/fonts_browser_proxy.m.js';
export {CountryDetailManagerImpl} from './autofill_page/address_edit_dialog.m.js';
export {AutofillManagerImpl} from './autofill_page/autofill_section.m.js';
export {PaymentsManagerImpl} from './autofill_page/payments_section.m.js';
export {ClearBrowsingDataBrowserProxyImpl} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.m.js';
export {DownloadsBrowserProxyImpl} from './downloads_page/downloads_browser_proxy.m.js';
export {kMenuCloseDelay} from './languages_page/languages_page.m.js';
export {LanguagesBrowserProxyImpl} from './languages_page/languages_browser_proxy.m.js';
export {cookieInfo} from './site_settings/cookie_info.m.js';
export {LocalDataBrowserProxyImpl} from './site_settings/local_data_browser_proxy.m.js';

// <if expr="not chromeos">
export {SystemPageBrowserProxyImpl} from './system_page/system_page_browser_proxy.m.js';
// </if>

// <if expr="chromeos">
export {BlockingRequestManager} from './autofill_page/blocking_request_manager.m.js';
// </if>
