// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace upboarding {
namespace test {

namespace {

const char kTimeStr[] = "03/18/20 01:00:00 AM";

void SerializeEntry(const QueryTileEntry* entry, std::stringstream& out) {
  if (!entry)
    return;
  out << "entry id: " << entry->id << " query text: " << entry->query_text
      << "  display text: " << entry->display_text
      << "  accessibility_text: " << entry->accessibility_text << " \n";

  for (const auto& image : entry->image_metadatas)
    out << "image id: " << image.id
        << " image url: " << image.url.possibly_invalid_spec() << " \n";
}

}  // namespace

std::string DebugString(const QueryTileEntry* root) {
  if (!root)
    return std::string();
  std::stringstream out;
  out << "Entries detail: \n";
  std::map<std::string, std::vector<std::string>> cache;
  std::deque<const QueryTileEntry*> queue;
  queue.emplace_back(root);
  while (!queue.empty()) {
    size_t size = queue.size();
    for (size_t i = 0; i < size; i++) {
      auto* parent = queue.front();
      SerializeEntry(parent, out);
      queue.pop_front();
      for (size_t j = 0; j < parent->sub_tiles.size(); j++) {
        cache[parent->id].emplace_back(parent->sub_tiles[j]->id);
        queue.emplace_back(parent->sub_tiles[j].get());
      }
    }
  }
  out << "Tree table: \n";
  for (auto& pair : cache) {
    std::string line;
    line += pair.first + " : [";
    std::sort(pair.second.begin(), pair.second.end());
    for (const auto& child : pair.second)
      line += " " + child;
    line += " ]\n";
    out << line;
  }
  return out.str();
}

std::string DebugString(const TileGroup* group) {
  if (!group)
    return std::string();
  std::stringstream out;
  out << "Group detail: \n";
  out << "id: " << group->id << " locale: " << group->locale
      << " last_updated_ts: " << group->last_updated_ts << " \n";
  for (const auto& tile : group->tiles) {
    out << DebugString(tile.get());
  }
  return out.str();
}

void ResetTestEntry(QueryTileEntry* entry) {
  entry->id = "guid-1-1";
  entry->query_text = "test query str";
  entry->display_text = "test display text";
  entry->accessibility_text = "read this test display text";
  entry->image_metadatas.clear();
  entry->image_metadatas.emplace_back("image-test-id-1",
                                      GURL("http://www.example.com"));
  entry->image_metadatas.emplace_back("image-test-id-2",
                                      GURL("http://www.fakeurl.com"));

  auto entry1 = std::make_unique<QueryTileEntry>();
  entry1->id = "guid-2-1";
  auto entry2 = std::make_unique<QueryTileEntry>();
  entry2->id = "guid-2-2";
  auto entry3 = std::make_unique<QueryTileEntry>();
  entry3->id = "guid-3-1";
  entry1->sub_tiles.emplace_back(std::move(entry3));
  entry->sub_tiles.clear();
  entry->sub_tiles.emplace_back(std::move(entry1));
  entry->sub_tiles.emplace_back(std::move(entry2));
}

void ResetTestGroup(TileGroup* group) {
  group->id = "group_guid";
  group->locale = "en-US";
  DCHECK(base::Time::FromString(kTimeStr, &group->last_updated_ts));
  group->tiles.clear();
  auto test_entry_1 = std::make_unique<QueryTileEntry>();
  ResetTestEntry(test_entry_1.get());
  auto test_entry_2 = std::make_unique<QueryTileEntry>();
  test_entry_2->id = "guid-1-2";
  auto test_entry_3 = std::make_unique<QueryTileEntry>();
  test_entry_3->id = "guid-1-3";
  auto test_entry_4 = std::make_unique<QueryTileEntry>();
  test_entry_4->id = "guid-1-4";
  test_entry_3->sub_tiles.emplace_back(std::move(test_entry_4));
  group->tiles.emplace_back(std::move(test_entry_1));
  group->tiles.emplace_back(std::move(test_entry_2));
  group->tiles.emplace_back(std::move(test_entry_3));
}

bool AreTileGroupsIdentical(const TileGroup& lhs, const TileGroup& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.tiles) {
    auto* target = it.get();
    auto found =
        std::find_if(rhs.tiles.begin(), rhs.tiles.end(),
                     [&target](const std::unique_ptr<QueryTileEntry>& entry) {
                       return entry->id == target->id;
                     });
    if (found == rhs.tiles.end() || *target != *found->get())
      return false;
  }

  return true;
}

bool AreTilesIdentical(const QueryTileEntry& lhs, const QueryTileEntry& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.image_metadatas) {
    auto found =
        std::find_if(rhs.image_metadatas.begin(), rhs.image_metadatas.end(),
                     [it](const ImageMetadata& image) { return image == it; });
    if (found == rhs.image_metadatas.end())
      return false;
  }

  for (const auto& it : lhs.sub_tiles) {
    auto* target = it.get();
    auto found =
        std::find_if(rhs.sub_tiles.begin(), rhs.sub_tiles.end(),
                     [&target](const std::unique_ptr<QueryTileEntry>& entry) {
                       return entry->id == target->id;
                     });
    if (found == rhs.sub_tiles.end() ||
        !AreTilesIdentical(*target, *found->get()))
      return false;
  }
  return true;
}

bool AreTilesIdentical(std::vector<QueryTileEntry*> lhs,
                       std::vector<QueryTileEntry*> rhs) {
  if (lhs.size() != rhs.size())
    return false;

  auto entry_comparator = [](QueryTileEntry* a, QueryTileEntry* b) {
    return a->id < b->id;
  };

  std::sort(lhs.begin(), lhs.end(), entry_comparator);
  std::sort(rhs.begin(), rhs.end(), entry_comparator);

  for (size_t i = 0; i < lhs.size(); i++) {
    if (*lhs[i] != *rhs[i])
      return false;
  }

  return true;
}

}  // namespace test

}  // namespace upboarding
