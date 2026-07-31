#include "router.h"

#include <utility>
#include <vector>

void MemoryRouter::registerSession(SessionId sessionId, DeliveryCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[sessionId] = std::move(callback);
}

void MemoryRouter::unregisterSession(SessionId sessionId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
}

void MemoryRouter::broadcast(const std::string& payload, SessionId excludedSession)
{
    std::vector<DeliveryCallback> recipients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recipients.reserve(sessions_.size());
        for (const auto& [sessionId, callback] : sessions_) {
            if (sessionId != excludedSession) {
                recipients.push_back(callback);
            }
        }
    }

    for (const auto& recipient : recipients) {
        recipient(payload);
    }
}

void MemoryRouter::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}

std::size_t MemoryRouter::sessionCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}
