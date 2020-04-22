// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/clock.h"
#include "chrome/browser/upboarding/query_tiles/internal/config.h"
#include "chrome/browser/upboarding/query_tiles/internal/store.h"
#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"
#include "chrome/browser/upboarding/query_tiles/internal/tile_types.h"
#include "chrome/browser/upboarding/query_tiles/tile.h"

namespace upboarding {

// Manages the in-memory tile group and coordinates with storage layer.
class TileManager {
 public:
  using TileStore = Store<TileGroup>;
  using TileGroupStatusCallback = base::OnceCallback<void(TileGroupStatus)>;

  // Creates the instance.
  static std::unique_ptr<TileManager> Create(
      std::unique_ptr<TileStore> tile_store,
      base::Clock* clock,
      TileConfig* config);

  // Initializes the query tile store, loading them into memory after
  // validating.
  virtual void Init(TileGroupStatusCallback callback) = 0;

  // Returns tiles to the caller.
  virtual void GetTiles(std::vector<Tile*>* tiles) = 0;

  // Save the query tiles into database.
  virtual void SaveTiles(std::vector<std::unique_ptr<Tile>> top_level_tiles,
                         TileGroupStatusCallback callback) = 0;

  TileManager();
  virtual ~TileManager() = default;

  TileManager(const TileManager& other) = delete;
  TileManager& operator=(const TileManager& other) = delete;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
