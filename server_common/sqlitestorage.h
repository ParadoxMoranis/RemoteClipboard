#pragma once

#include "storage.h"

#include <mutex>
#include <string>

struct sqlite3;

class SqliteStorage final : public IEventStorage
{
  public:
    explicit SqliteStorage(std::string databasePath);
    ~SqliteStorage() override;

    StorageResult initialize() override;
    StorageResult recordEvent(const StoredEvent& event) override;

    const std::string& databasePath() const;

  private:
    StorageResult executeSchema(const char* sql);
    StorageResult classifyError(int code, const std::string& operation) const;

    std::string databasePath_;
    sqlite3* database_ = nullptr;
    mutable std::mutex mutex_;
};
