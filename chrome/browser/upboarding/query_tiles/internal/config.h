// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CONFIG_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "url/gurl.h"

namespace upboarding {

struct QueryTilesConfig {
  // Creates a default QueryTilesConfig.
  static std::unique_ptr<QueryTilesConfig> Create();

  // Creates a QueryTilesConfig that reads parameters from Finch.
  static std::unique_ptr<QueryTilesConfig> CreateFromFinch();

  QueryTilesConfig();
  ~QueryTilesConfig();
  QueryTilesConfig(const QueryTilesConfig& other) = delete;
  QueryTilesConfig& operator=(const QueryTilesConfig& other) = delete;

  // Flag to tell whether query tiles is enabled or not.
  bool is_enabled;

  // The base URL for the Query Tiles server.
  GURL base_url;

  // The URL for GetQueryTiles RPC.
  GURL get_query_tile_url;

  // The maximum duration for holding current group's info and images.
  base::TimeDelta expire_duration;

  // Locale setting from operating system.
  std::string locale;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CONFIG_H_
