// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <set>
#include <string>
#include <vector>

#include "url/gurl.h"

namespace enterprise_connectors {

// Enums representing each connector to be used as arguments so the appropriate
// policies/settings can be read.
enum class AnalysisConnector {
  FILE_DOWNLOADED,
  FILE_ATTACHED,
  BULK_DATA_ENTRY,
};

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  NO_BLOCK = 0,
  BLOCK = 1,
};

// Structs representing settings to be used for an analysis or a report. These
// settings should only be kept and considered valid for the specific
// analysis/report they were obtained for.
struct AnalysisSettings {
  AnalysisSettings();
  AnalysisSettings(AnalysisSettings&&);
  AnalysisSettings& operator=(AnalysisSettings&&);
  ~AnalysisSettings();

  GURL analysis_url;
  std::set<std::string> tags;
  BlockUntilVerdict block_until_verdict;
  bool block_password_protected_files;
  bool block_large_files;
  bool block_unsupported_file_types;
};

struct ReportingSettings {
  ReportingSettings();
  ReportingSettings(ReportingSettings&&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  std::vector<GURL> reporting_urls;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
