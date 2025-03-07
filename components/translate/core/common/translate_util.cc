// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

namespace {

// Parameter for TranslateSubFrames feature to determine whether language
// detection should include the sub frames (or just the main frame).
const char kDetectLanguageInSubFrames[] = "detect_language_in_sub_frames";

}  // namespace

const char kSecurityOrigin[] = "trk:220:https://translate.googleapis.com/";

const base::Feature kTranslateSubFrames{"TranslateSubFrames",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

GURL GetTranslateSecurityOrigin() {
  std::string security_origin(kSecurityOrigin);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

bool IsSubFrameTranslationEnabled() {
  return base::FeatureList::IsEnabled(kTranslateSubFrames);
}

bool IsSubFrameLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(kTranslateSubFrames) &&
         base::GetFieldTrialParamByFeatureAsBool(
             kTranslateSubFrames, kDetectLanguageInSubFrames, true);
}

}  // namespace translate
