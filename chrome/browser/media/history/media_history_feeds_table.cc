// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feeds_table.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "chrome/browser/media/feeds/media_feeds_utils.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "services/media_session/public/cpp/media_image.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace media_history {

namespace {

// The maximum number of logos to allow.
const int kMaxLogoCount = 5;

}  // namespace

const char MediaHistoryFeedsTable::kTableName[] = "mediaFeed";

const char MediaHistoryFeedsTable::kFeedReadResultHistogramName[] =
    "Media.Feeds.Feed.ReadResult";

MediaHistoryFeedsTable::MediaHistoryFeedsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedsTable::~MediaHistoryFeedsTable() = default;

sql::InitStatus MediaHistoryFeedsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         "origin_id INTEGER NOT NULL UNIQUE,"
                         "url TEXT NOT NULL, "
                         "last_discovery_time_s INTEGER, "
                         "last_fetch_time_s INTEGER, "
                         "user_status INTEGER DEFAULT 0, "
                         "last_fetch_result INTEGER DEFAULT 0, "
                         "fetch_failed_count INTEGER, "
                         "last_fetch_time_not_cache_hit_s INTEGER, "
                         "last_fetch_item_count INTEGER, "
                         "last_fetch_safe_item_count INTEGER, "
                         "last_fetch_play_next_count INTEGER, "
                         "last_fetch_content_types INTEGER, "
                         "logo BLOB, "
                         "display_name TEXT, "
                         "last_display_time_s INTEGER, "
                         "CONSTRAINT fk_origin "
                         "FOREIGN KEY (origin_id) "
                         "REFERENCES origin(id) "
                         "ON DELETE CASCADE"
                         ")",
                         kTableName)
          .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS mediaFeed_origin_id_index ON "
            "%s (origin_id)",
            kTableName)
            .c_str());
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history feeds table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedsTable::DiscoverFeed(const GURL& url) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  const auto origin =
      MediaHistoryOriginTable::GetOriginForStorage(url::Origin::Create(url));
  const auto now = base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  base::Optional<GURL> feed_url;
  base::Optional<int64_t> feed_id;

  {
    // Check if we already have a feed for the current origin;
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT id, url FROM mediaFeed WHERE origin_id = (SELECT id FROM "
        "origin WHERE origin = ?)"));
    statement.BindString(0, origin);

    while (statement.Step()) {
      DCHECK(!feed_id);
      DCHECK(!feed_url);

      feed_id = statement.ColumnInt64(0);
      feed_url = GURL(statement.ColumnString(1));
    }
  }

  if (!feed_url || url != feed_url) {
    // If the feed does not exist or exists and has a different URL then we
    // should replace the feed.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO mediaFeed "
        "(origin_id, url, last_discovery_time_s) VALUES "
        "((SELECT id FROM origin WHERE origin = ?), ?, ?)"));
    statement.BindString(0, origin);
    statement.BindString(1, url.spec());
    statement.BindInt64(2, now);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  } else {
    // If the feed already exists in the database with the same URL we should
    // just update the last discovery time so we don't delete the old entry.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_discovery_time_s = ? WHERE id = ?"));
    statement.BindInt64(0, now);
    statement.BindInt64(1, *feed_id);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  }
}

std::vector<media_feeds::mojom::MediaFeedPtr> MediaHistoryFeedsTable::GetRows(
    const MediaHistoryKeyedService::GetMediaFeedsRequest& request) {
  std::vector<media_feeds::mojom::MediaFeedPtr> feeds;
  if (!CanAccessDatabase())
    return feeds;

  base::Optional<double> origin_count;
  double rank = 0;

  std::vector<std::string> sql;
  sql.push_back(
      "SELECT "
      "mediaFeed.id, "
      "mediaFeed.url, "
      "mediaFeed.last_discovery_time_s, "
      "mediaFeed.last_fetch_time_s, "
      "mediaFeed.user_status, "
      "mediaFeed.last_fetch_result, "
      "mediaFeed.fetch_failed_count, "
      "mediaFeed.last_fetch_time_not_cache_hit_s, "
      "mediaFeed.last_fetch_item_count, "
      "mediaFeed.last_fetch_safe_item_count, "
      "mediaFeed.last_fetch_play_next_count, "
      "mediaFeed.last_fetch_content_types, "
      "mediaFeed.logo, "
      "mediaFeed.display_name, "
      "mediaFeed.last_display_time_s");

  if (request.include_origin_watchtime_percentile_data) {
    // If we need the percentile data we should select rows from the origin
    // table and LEFT JOIN mediaFeed. This means there should be a row for each
    // origin and if there is a media feed that will be included.
    sql.push_back(
        ","
        "origin.aggregate_watchtime_audio_video_s "
        "FROM origin "
        "LEFT JOIN mediaFeed "
        "ON origin.id = mediaFeed.origin_id");
  } else if (request.audio_video_watchtime_min) {
    // If we need to filter by |audio_video_watchtime_min| then we should join
    // the origin table to get the watchtime data.
    sql.push_back(
        ","
        "origin.aggregate_watchtime_audio_video_s "
        "FROM mediaFeed "
        "LEFT JOIN origin "
        "ON origin.id = mediaFeed.origin_id");
  } else {
    sql.push_back("FROM mediaFeed");
  }

  // Sorts the feeds in descending watchtime order. We also need the total count
  // of the origins so we can calculate a percentile.
  if (request.include_origin_watchtime_percentile_data) {
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE, "SELECT COUNT(id) FROM origin"));

    while (statement.Step()) {
      origin_count = statement.ColumnDouble(0);
      rank = *origin_count;
    }

    DCHECK(origin_count.has_value());

    sql.push_back("ORDER BY origin.aggregate_watchtime_audio_video_s DESC");
  }

  // Apply any SQL filters and limits. If we have the watchtime data then these
  // are implemented in C++ below since we need all the rows to calculate the
  // rank.
  if (!request.include_origin_watchtime_percentile_data) {
    if (request.audio_video_watchtime_min.has_value()) {
      sql.push_back(base::StringPrintf(
          "WHERE origin.aggregate_watchtime_audio_video_s >= %f",
          request.audio_video_watchtime_min->InSecondsF()));
    }

    if (request.fetched_items_min.has_value()) {
      if (request.fetched_items_min_should_be_safe) {
        sql.push_back(
            base::StringPrintf("WHERE last_fetch_safe_item_count >= %i",
                               *request.fetched_items_min));
      } else {
        sql.push_back(base::StringPrintf("WHERE last_fetch_item_count >= %i",
                                         *request.fetched_items_min));
      }
    }

    if (request.limit.has_value())
      sql.push_back(base::StringPrintf("LIMIT %i", *request.limit));
  }

  sql::Statement statement(
      DB()->GetUniqueStatement(base::JoinString(sql, " ").c_str()));

  while (statement.Step()) {
    rank--;

    // If there is no mediaFeed data then skip this.
    if (statement.GetColumnType(0) == sql::ColumnType::kNull)
      continue;

    auto feed = media_feeds::mojom::MediaFeed::New();
    feed->last_fetch_item_count = statement.ColumnInt64(8);
    feed->last_fetch_safe_item_count = statement.ColumnInt64(9);

    // Apply any C++ filters.
    if (request.include_origin_watchtime_percentile_data) {
      if (request.audio_video_watchtime_min.has_value()) {
        auto agg_watchtime =
            base::TimeDelta::FromSeconds(statement.ColumnInt64(15));

        if (agg_watchtime < *request.audio_video_watchtime_min)
          continue;
      }

      if (request.fetched_items_min.has_value()) {
        if (request.fetched_items_min_should_be_safe) {
          if (feed->last_fetch_safe_item_count < *request.fetched_items_min)
            continue;
        } else {
          if (feed->last_fetch_item_count < *request.fetched_items_min)
            continue;
        }
      }
    }

    feed->user_status = static_cast<media_feeds::mojom::FeedUserStatus>(
        statement.ColumnInt64(4));
    feed->last_fetch_result =
        static_cast<media_feeds::mojom::FetchResult>(statement.ColumnInt64(5));

    if (!IsKnownEnumValue(feed->user_status)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadUserStatus);
      continue;
    }

    if (!IsKnownEnumValue(feed->last_fetch_result)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadFetchResult);
      continue;
    }

    if (statement.GetColumnType(12) == sql::ColumnType::kBlob) {
      media_feeds::ImageSet image_set;
      if (!GetProto(statement, 12, image_set)) {
        base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                      FeedReadResult::kBadLogo);

        continue;
      }

      feed->logos = media_feeds::ProtoToMediaImages(image_set, kMaxLogoCount);
    }

    base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                  FeedReadResult::kSuccess);

    feed->id = statement.ColumnInt64(0);
    feed->url = GURL(statement.ColumnString(1));
    feed->last_discovery_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2)));

    if (statement.GetColumnType(3) == sql::ColumnType::kInteger) {
      feed->last_fetch_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSeconds(statement.ColumnInt64(3)));
    }

    feed->fetch_failed_count = statement.ColumnInt64(6);

    if (statement.GetColumnType(7) == sql::ColumnType::kInteger) {
      feed->last_fetch_time_not_cache_hit =
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromSeconds(statement.ColumnInt64(7)));
    }

    feed->last_fetch_play_next_count = statement.ColumnInt64(10);
    feed->last_fetch_content_types = statement.ColumnInt64(11);
    feed->display_name = statement.ColumnString(13);

    if (statement.GetColumnType(14) == sql::ColumnType::kInteger) {
      feed->last_display_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSeconds(statement.ColumnInt64(14)));
    }

    if (request.include_origin_watchtime_percentile_data && origin_count > 1) {
      feed->origin_audio_video_watchtime_percentile =
          (rank / (*origin_count - 1)) * 100;
    }

    feeds.push_back(std::move(feed));

    if (request.include_origin_watchtime_percentile_data &&
        request.limit.has_value() && feeds.size() >= *request.limit) {
      break;
    }
  }

  DCHECK(statement.Succeeded());
  return feeds;
}

bool MediaHistoryFeedsTable::UpdateFeedFromFetch(
    const int64_t feed_id,
    const media_feeds::mojom::FetchResult result,
    const bool was_fetched_from_cache,
    const int item_count,
    const int item_play_next_count,
    const int item_content_types,
    const std::vector<media_session::MediaImage>& logos,
    const std::string& display_name,
    const int item_safe_count) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  int fetch_failed_count = 0;

  {
    if (result != media_feeds::mojom::FetchResult::kSuccess) {
      // See how many times we have failed to fetch the feed.
      sql::Statement statement(DB()->GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT fetch_failed_count FROM mediaFeed WHERE id = ?"));
      statement.BindInt64(0, feed_id);

      while (statement.Step()) {
        DCHECK(!fetch_failed_count);
        fetch_failed_count = statement.ColumnInt64(0) + 1;
      }
    }
  }

  sql::Statement statement;
  if (was_fetched_from_cache) {
    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_fetch_time_s = ?, last_fetch_result = ?, "
        "fetch_failed_count = ?, last_fetch_item_count = ?, "
        "last_fetch_play_next_count = ?, last_fetch_content_types = ?, "
        "logo = ?, display_name = ?, last_fetch_safe_item_count = ? WHERE id = "
        "?"));
  } else {
    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_fetch_time_s = ?, last_fetch_result = ?, "
        "fetch_failed_count = ?, last_fetch_item_count = ?, "
        "last_fetch_play_next_count = ?, last_fetch_content_types = ?, "
        "logo = ?, display_name = ?, last_fetch_safe_item_count = ?, "
        "last_fetch_time_not_cache_hit_s = ? WHERE "
        "id = ?"));
  }

  statement.BindInt64(0,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(1, static_cast<int>(result));
  statement.BindInt64(2, fetch_failed_count);
  statement.BindInt64(3, item_count);
  statement.BindInt64(4, item_play_next_count);
  statement.BindInt64(5, item_content_types);

  if (!logos.empty()) {
    BindProto(statement, 6,
              media_feeds::MediaImagesToProto(logos, kMaxLogoCount));
  } else {
    statement.BindNull(6);
  }

  statement.BindString(7, display_name);
  statement.BindInt64(8, item_safe_count);

  if (was_fetched_from_cache) {
    statement.BindInt64(9, feed_id);
  } else {
    statement.BindInt64(
        9, base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
    statement.BindInt64(10, feed_id);
  }

  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedsTable::UpdateDisplayTime(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET last_display_time_s = ? WHERE id = ?"));

  statement.BindInt64(0,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(1, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedsTable::RecalculateSafeSearchItemCount(
    const int64_t feed_id) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET last_fetch_safe_item_count = (SELECT COUNT(id) "
      "FROM mediaFeedItem WHERE safe_search_result = ? AND feed_id = ?) WHERE "
      "id = ?"));
  statement.BindInt64(
      0, static_cast<int>(media_feeds::mojom::SafeSearchResult::kSafe));
  statement.BindInt64(1, feed_id);
  statement.BindInt64(2, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

}  // namespace media_history
