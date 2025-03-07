// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

namespace metrics {

const char kNewMetricsServerUrl[] =
    "trk:265:https://clientservices.googleapis.com/uma/v2";

const char kNewMetricsServerUrlInsecure[] =
    "http://clientservices.googleapis.com/uma/v2";

const char kOldMetricsServerUrl[] = "https://clients4.google.com/uma/v2";

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

} // namespace metrics
