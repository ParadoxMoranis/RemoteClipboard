#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

class IRouter
{
  public:
    using SessionId = std::uint64_t;
    using DeliveryCallback = std::function<bool(const std::string&)>;

    virtual ~IRouter() = default;
    virtual void registerSession(SessionId sessionId, DeliveryCallback callback) = 0;
    virtual void unregisterSession(SessionId sessionId) = 0;
    virtual void broadcast(const std::string& payload, SessionId excludedSession) = 0;
    virtual void clear() = 0;
};

class MemoryRouter final : public IRouter
{
  public:
    void registerSession(SessionId sessionId, DeliveryCallback callback) override;
    void unregisterSession(SessionId sessionId) override;
    void broadcast(const std::string& payload, SessionId excludedSession) override;
    void clear() override;

    std::size_t sessionCount() const;

  private:
    mutable std::mutex mutex_;
    std::unordered_map<SessionId, DeliveryCallback> sessions_;
};
