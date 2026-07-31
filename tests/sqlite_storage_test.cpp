#include "test_support.h"

#include "server_common/sqlitestorage.h"

#include <chrono>
#include <filesystem>
#include <string>

#include <sqlite3.h>

namespace {
std::filesystem::path uniqueTemporaryDirectory()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("remoteclipboard-sqlite-test-" + std::to_string(suffix));
}

std::string queryText(sqlite3* database, const char* sql)
{
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return {};
    }
    std::string value;
    if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_text(statement, 0) != nullptr) {
        value = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    }
    sqlite3_finalize(statement);
    return value;
}
} // namespace

int main()
{
    TestContext test;
    const auto directory = uniqueTemporaryDirectory();
    const auto databasePath = directory / "events.sqlite3";

    {
        SqliteStorage storage(databasePath.string());
        RC_EXPECT(test, storage.initialize());

        StoredEvent event;
        event.eventId = "event'; DROP TABLE clipboard_events; --";
        event.workspaceId = "workspace'; DELETE FROM schema_migrations; --";
        event.deviceId = "device'); DROP TABLE schema_migrations; --";
        event.sender = "sender'; SELECT randomblob(1000000); --";
        event.type = "clipboard_text'; --";
        event.sequence = 42;
        event.contentSha256 = "abc'; UPDATE clipboard_events SET sender='bad";
        event.payloadBytes = 17;
        event.createdAtUnixMs = 123456789;
        RC_EXPECT(test, storage.recordEvent(event));
        RC_EXPECT(test, storage.recordEvent(event));
    }

    sqlite3* database = nullptr;
    RC_EXPECT_EQ(test, sqlite3_open(databasePath.string().c_str(), &database), SQLITE_OK);
    if (database != nullptr) {
        RC_EXPECT_EQ(
            test,
            queryText(
                database,
                "SELECT name FROM sqlite_master WHERE type='table' AND name='clipboard_events';"),
            "clipboard_events");
        RC_EXPECT_EQ(
            test,
            queryText(
                database,
                "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_migrations';"),
            "schema_migrations");
        RC_EXPECT_EQ(test, queryText(database, "SELECT COUNT(*) FROM clipboard_events;"), "1");
        RC_EXPECT_EQ(test, queryText(database, "SELECT device_id FROM clipboard_events LIMIT 1;"),
                     "device'); DROP TABLE schema_migrations; --");
        RC_EXPECT_EQ(test, queryText(database, "SELECT sender FROM clipboard_events LIMIT 1;"),
                     "sender'; SELECT randomblob(1000000); --");
        sqlite3_close(database);
    }

    std::filesystem::remove_all(directory);
    return test.result();
}
