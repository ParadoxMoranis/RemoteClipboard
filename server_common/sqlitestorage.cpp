#include "sqlitestorage.h"

#include <filesystem>
#include <utility>

#include <sqlite3.h>

namespace {
constexpr int kSchemaVersion = 1;

class Statement final
{
  public:
    explicit Statement(sqlite3_stmt* statement) : statement_(statement) {}

    ~Statement()
    {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    sqlite3_stmt* get() const { return statement_; }

  private:
    sqlite3_stmt* statement_;
};

bool bindText(sqlite3_stmt* statement, int index, const std::string& value)
{
    return sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
}
} // namespace

SqliteStorage::SqliteStorage(std::string databasePath) : databasePath_(std::move(databasePath)) {}

SqliteStorage::~SqliteStorage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

StorageResult SqliteStorage::initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (database_ != nullptr) {
        return {};
    }

    const auto parent = std::filesystem::path(databasePath_).parent_path();
    std::error_code error;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return {StorageStatus::PermanentFailure,
                    "Unable to create SQLite directory: " + error.message()};
        }
    }

    const int openResult = sqlite3_open_v2(
        databasePath_.c_str(), &database_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (openResult != SQLITE_OK) {
        const auto result = classifyError(openResult, "open database");
        if (database_ != nullptr) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
        return result;
    }

    sqlite3_busy_timeout(database_, 5000);
    const char* schema = R"sql(
PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS clipboard_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    workspace_id TEXT NOT NULL,
    event_id TEXT NOT NULL,
    device_id TEXT NOT NULL,
    sender TEXT NOT NULL,
    type TEXT NOT NULL,
    sequence INTEGER NOT NULL DEFAULT 0,
    content_sha256 TEXT NOT NULL,
    payload_bytes INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    UNIQUE(workspace_id, event_id)
);
CREATE INDEX IF NOT EXISTS idx_clipboard_events_workspace_created
    ON clipboard_events(workspace_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_clipboard_events_device_sequence
    ON clipboard_events(device_id, sequence);
INSERT OR IGNORE INTO schema_migrations(version, applied_at)
    VALUES(1, CAST(strftime('%s','now') AS INTEGER) * 1000);
)sql";
    return executeSchema(schema);
}

StorageResult SqliteStorage::recordEvent(const StoredEvent& event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (database_ == nullptr) {
        return {StorageStatus::PermanentFailure, "SQLite storage is not initialized"};
    }

    static constexpr const char* kInsertSql = R"sql(
INSERT OR IGNORE INTO clipboard_events(
    workspace_id, event_id, device_id, sender, type, sequence,
    content_sha256, payload_bytes, created_at
) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt* rawStatement = nullptr;
    int result = sqlite3_prepare_v2(database_, kInsertSql, -1, &rawStatement, nullptr);
    if (result != SQLITE_OK) {
        return classifyError(result, "prepare event insert");
    }
    Statement statement(rawStatement);

    const bool bound =
        bindText(statement.get(), 1, event.workspaceId) &&
        bindText(statement.get(), 2, event.eventId) &&
        bindText(statement.get(), 3, event.deviceId) &&
        bindText(statement.get(), 4, event.sender) && bindText(statement.get(), 5, event.type) &&
        sqlite3_bind_int64(statement.get(), 6, static_cast<sqlite3_int64>(event.sequence)) ==
            SQLITE_OK &&
        bindText(statement.get(), 7, event.contentSha256) &&
        sqlite3_bind_int64(statement.get(), 8, static_cast<sqlite3_int64>(event.payloadBytes)) ==
            SQLITE_OK &&
        sqlite3_bind_int64(statement.get(), 9, static_cast<sqlite3_int64>(event.createdAtUnixMs)) ==
            SQLITE_OK;
    if (!bound) {
        return classifyError(sqlite3_errcode(database_), "bind event fields");
    }

    result = sqlite3_step(statement.get());
    if (result != SQLITE_DONE) {
        return classifyError(result, "insert event");
    }
    return {};
}

const std::string& SqliteStorage::databasePath() const { return databasePath_; }

StorageResult SqliteStorage::executeSchema(const char* sql)
{
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(database_, sql, nullptr, nullptr, &errorMessage);
    std::string detail;
    if (errorMessage != nullptr) {
        detail = errorMessage;
        sqlite3_free(errorMessage);
    }
    if (result != SQLITE_OK) {
        auto classified = classifyError(result, "apply schema");
        if (!detail.empty()) {
            classified.message += ": " + detail;
        }
        return classified;
    }
    return {};
}

StorageResult SqliteStorage::classifyError(int code, const std::string& operation) const
{
    const bool retryable = code == SQLITE_BUSY || code == SQLITE_LOCKED || code == SQLITE_IOERR;
    std::string message = "SQLite failed to " + operation;
    if (database_ != nullptr) {
        message += ": ";
        message += sqlite3_errmsg(database_);
    }
    return {retryable ? StorageStatus::RetryableFailure : StorageStatus::PermanentFailure,
            std::move(message)};
}
