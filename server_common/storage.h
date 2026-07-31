#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class StorageStatus
{
    Success,
    RetryableFailure,
    PermanentFailure,
};

struct StorageResult
{
    StorageStatus status = StorageStatus::Success;
    std::string message;

    explicit operator bool() const { return status == StorageStatus::Success; }
};

struct StoredEvent
{
    std::string eventId;
    std::string workspaceId;
    std::string deviceId;
    std::string sender;
    std::string type;
    std::string contentSha256;
    std::uint64_t sequence = 0;
    std::size_t payloadBytes = 0;
    std::int64_t createdAtUnixMs = 0;
};

class IStorage
{
  public:
    virtual ~IStorage() = default;
    virtual StorageResult initialize() = 0;
    virtual StorageResult recordEvent(const StoredEvent& event) = 0;
};
