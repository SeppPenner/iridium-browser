// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_database.h"

#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Define an ordering based on ascending timestamps to allow sorting of
// AccessRecords for easier testing, as no ordering is guaranteed by the
// database.
bool TimestampOrdering(const AccessContextAuditDatabase::AccessRecord& a,
                       const AccessContextAuditDatabase::AccessRecord& b) {
  return a.last_access_time < b.last_access_time;
}

void ExpectAccessRecordsEqual(
    const AccessContextAuditDatabase::AccessRecord& a,
    const AccessContextAuditDatabase::AccessRecord& b) {
  EXPECT_EQ(a.top_frame_origin, b.top_frame_origin);
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.last_access_time, b.last_access_time);

  if (a.type == AccessContextAuditDatabase::StorageAPIType::kCookie) {
    EXPECT_EQ(a.name, b.name);
    EXPECT_EQ(a.domain, b.domain);
    EXPECT_EQ(a.path, b.path);
  } else {
    EXPECT_EQ(a.origin, b.origin);
  }
}

void ValidateDatabaseRecords(
    AccessContextAuditDatabase* database,
    const std::vector<AccessContextAuditDatabase::AccessRecord>&
        expected_records) {
  auto stored_records = database->GetAllRecords();
  std::sort(stored_records.begin(), stored_records.end(), TimestampOrdering);

  EXPECT_EQ(stored_records.size(), expected_records.size());
  for (size_t i = 0;
       i < std::min(stored_records.size(), expected_records.size()); i++) {
    ExpectAccessRecordsEqual(stored_records[i], expected_records[i]);
  }
}

}  // namespace

class AccessContextAuditDatabaseTest : public testing::Test {
 public:
  AccessContextAuditDatabaseTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    database_.reset();
    database_ =
        std::make_unique<AccessContextAuditDatabase>(temp_directory_.GetPath());
    EXPECT_TRUE(database_->Init());
  }

  void CloseDatabase() { database_.reset(); }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("AccessContextAudit"));
  }

  AccessContextAuditDatabase* database() { return database_.get(); }

  std::vector<AccessContextAuditDatabase::AccessRecord> GetTestRecords() {
    return {
        AccessContextAuditDatabase::AccessRecord(
            GURL("https://test.com"),
            AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
            GURL("https://test.com"),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromHours(1))),
        AccessContextAuditDatabase::AccessRecord(
            GURL("https://test2.com:8000"),
            AccessContextAuditDatabase::StorageAPIType::kLocalStorage,
            GURL("https://test.com"),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromHours(2))),
        AccessContextAuditDatabase::AccessRecord(
            GURL("https://test2.com"), "cookie1", "test.com", "/",
            base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromHours(3))),
        AccessContextAuditDatabase::AccessRecord(
            GURL("https://test2.com"), "cookie2", "test.com", "/",
            base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromHours(4))),
    };
  }

 private:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<AccessContextAuditDatabase> database_;
};

TEST_F(AccessContextAuditDatabaseTest, DatabaseInitialization) {
  // Check that tables are created and at least have the appropriate number of
  // columns.
  OpenDatabase();
  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  // [cookies] and [storageapi].
  EXPECT_EQ(2u, sql::test::CountSQLTables(&raw_db));

  // [top_frame_origin, name, domain, path, access_utc]
  EXPECT_EQ(5u, sql::test::CountTableColumns(&raw_db, "cookies"));

  // [top_frame_origin, type, origin, access_utc]
  EXPECT_EQ(4u, sql::test::CountTableColumns(&raw_db, "originStorageAPIs"));
}

TEST_F(AccessContextAuditDatabaseTest, DataPersisted) {
  // Check that data is retrievable both before and after a database reopening.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);

  CloseDatabase();
  OpenDatabase();

  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();
}

TEST_F(AccessContextAuditDatabaseTest, RecoveredOnOpen) {
  // Check that a database recovery is performed when opening a corrupted file.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // Data should be recovered.
  ValidateDatabaseRecords(database(), test_records);

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

TEST_F(AccessContextAuditDatabaseTest, RemoveRecord) {
  // Check that entries are removed from the database such that they are both
  // not returned by GetAllRecords and are removed from the database file.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);
  while (test_records.size() > 0) {
    database()->RemoveRecord(test_records[0]);
    test_records.erase(test_records.begin());
    ValidateDatabaseRecords(database(), test_records);
  }
  CloseDatabase();

  // Verify that everything is deleted.
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(0u, cookie_rows);
  EXPECT_EQ(0u, storage_api_rows);
}

TEST_F(AccessContextAuditDatabaseTest, RepeatedAccesses) {
  // Check that additional access records, only differing by timestamp to
  // previous entries, update those entries rather than creating new ones.
  auto test_records = GetTestRecords();
  OpenDatabase();
  database()->AddRecords(test_records);

  for (auto& record : test_records) {
    record.last_access_time += base::TimeDelta::FromHours(1);
  }

  database()->AddRecords(test_records);
  ValidateDatabaseRecords(database(), test_records);
  CloseDatabase();

  // Verify that extra entries are not present in the database.
  size_t num_test_cookie_entries = std::count_if(
      test_records.begin(), test_records.end(),
      [](const AccessContextAuditDatabase::AccessRecord& record) {
        return record.type ==
               AccessContextAuditDatabase::StorageAPIType::kCookie;
      });
  size_t num_test_storage_entries =
      test_records.size() - num_test_cookie_entries;

  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));

  size_t cookie_rows;
  size_t storage_api_rows;
  sql::test::CountTableRows(&raw_db, "cookies", &cookie_rows);
  sql::test::CountTableRows(&raw_db, "originStorageAPIs", &storage_api_rows);

  EXPECT_EQ(num_test_cookie_entries, cookie_rows);
  EXPECT_EQ(num_test_storage_entries, storage_api_rows);
}
