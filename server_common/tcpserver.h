#pragma once

#include "filetransfer.h"
#include "frameparser.h"
#include "router.h"
#include "storage.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <sys/socket.h>
using SocketType = int;
constexpr SocketType INVALID_SOCKET_VALUE = -1;
#endif

struct ssl_ctx_st;
struct ssl_st;
using SSL_CTX = ssl_ctx_st;
using SSL = ssl_st;

class TcpServer
{
  public:
    explicit TcpServer(std::size_t maxMessageBytes);
    ~TcpServer();

    void setCredentials(const std::string& username, const std::string& password);
    void configureStorage(const std::string& storageDir, int retentionDays);
    void configureDatabase(const std::string& sqlitePath);
    void setStorage(std::shared_ptr<IEventStorage> storage);
    void configureTls(bool enabled, const std::string& certificateFile,
                      const std::string& privateKeyFile);

    bool startServer(std::uint16_t port);
    void stopServer();
    void processEvents();

    std::uint16_t boundPort() const;
    std::size_t activeSessionCount() const;

  private:
    using Json = nlohmann::json;

    struct ClientConnection
    {
        ClientConnection(std::uint64_t newSessionId, SocketType newSocket,
                         std::string newPeerAddress, std::size_t maxFrameBytes);

        std::uint64_t sessionId;
        SocketType socket;
        std::string peerAddress;
        SSL* ssl = nullptr;
        std::atomic<bool> closing = false;
        std::atomic<bool> finished = false;
        bool authenticated = false;
        std::string username;
        FrameParser parser;
        std::map<std::string, filetransfer::IncomingTransferState> incomingTransfers;
        std::thread readerThread;
        std::thread writerThread;
        std::mutex queueMutex;
        std::condition_variable queueCondition;
        std::deque<std::string> sendQueue;
        std::size_t queuedBytes = 0;
    };

    void initializeTls();
    void cleanupTls();
    bool initializeStorage();
    bool setSocketReuseAddress(SocketType socket) const;
    bool setSocketNonBlocking(SocketType socket) const;
    bool setSocketBlocking(SocketType socket) const;
    bool setSocketTimeouts(SocketType socket, std::chrono::seconds timeout) const;
    void shutdownSocket(SocketType socket) const;
    void closeSocket(SocketType socket) const;

    void acceptLoop();
    void acceptNewConnections();
    void clientLoop(const std::shared_ptr<ClientConnection>& client);
    void writerLoop(const std::shared_ptr<ClientConnection>& client);
    int readFromClient(const std::shared_ptr<ClientConnection>& client, char* buffer,
                       std::size_t bufferSize);
    bool writeRaw(const std::shared_ptr<ClientConnection>& client, const std::string& payload);
    bool enqueue(const std::shared_ptr<ClientConnection>& client, std::string payload);
    bool sendJson(const std::shared_ptr<ClientConnection>& client, const Json& message);
    void requestClientClose(const std::shared_ptr<ClientConnection>& client);
    void finalizeClient(const std::shared_ptr<ClientConnection>& client);
    void reapClosedClients();

    void processFrame(const std::shared_ptr<ClientConnection>& client, const std::string& frame);
    void processMessage(const std::shared_ptr<ClientConnection>& client, const Json& message);
    bool authenticateClient(const std::shared_ptr<ClientConnection>& client, const Json& message);
    void registerAuthenticatedClient(const std::shared_ptr<ClientConnection>& client);
    void broadcastMessage(const Json& message, std::uint64_t excludedSession);
    void sendProtocolError(const std::shared_ptr<ClientConnection>& client, const std::string& code,
                           const std::string& message, const Json* request = nullptr);
    void recordAcceptedEvent(const Json& message, const std::string& sender);
    bool authenticationAllowed(const std::string& peerAddress, const std::string& username);
    void recordAuthenticationFailure(const std::string& peerAddress, const std::string& username);
    bool consumeUploadBudget(std::size_t bytes);

    bool ensureStorageDir();
    void cleanupExpiredFiles();
    bool persistFiles(const std::shared_ptr<ClientConnection>& client, const Json& message);
    bool handleChunkTransfer(const std::shared_ptr<ClientConnection>& client, const Json& message);
    bool handleChunkTransferStart(const std::shared_ptr<ClientConnection>& client,
                                  const Json& message);
    bool handleChunkTransferChunk(const std::shared_ptr<ClientConnection>& client,
                                  const Json& message);
    bool handleChunkTransferComplete(const std::shared_ptr<ClientConnection>& client,
                                     const Json& message);
    void cleanupExpiredTransfers(const std::shared_ptr<ClientConnection>& client);
    void discardTransfer(filetransfer::IncomingTransferState& transfer);
    void cleanupClientTransfers(const std::shared_ptr<ClientConnection>& client);

    const std::size_t maxMessageBytes_;
    const std::size_t maxSendQueueBytes_;
    SocketType serverSocket_ = INVALID_SOCKET_VALUE;
    std::atomic<bool> running_ = false;
    std::thread acceptorThread_;
    std::map<SocketType, std::shared_ptr<ClientConnection>> clients_;
    mutable std::mutex clientsMutex_;
    std::atomic<std::uint64_t> nextSessionId_ = 1;
    std::atomic<std::uint64_t> nextEventId_ = 1;
    std::atomic<std::size_t> activeHandshakes_ = 0;

    struct AuthenticationRateState
    {
        std::size_t failures = 0;
        std::chrono::steady_clock::time_point windowStartedAt;
    };
    std::mutex authenticationRateMutex_;
    std::unordered_map<std::string, AuthenticationRateState> authenticationRates_;
    std::mutex uploadRateMutex_;
    std::size_t uploadedBytesInWindow_ = 0;
    std::chrono::steady_clock::time_point uploadWindowStartedAt_ = std::chrono::steady_clock::now();

    std::string username_;
    std::string password_;
    std::string storageDir_ = "received_files";
    std::string sqlitePath_;
    int retentionDays_ = 7;

    bool tlsEnabled_ = false;
    std::string certificateFile_;
    std::string privateKeyFile_;
    SSL_CTX* sslContext_ = nullptr;

    std::shared_ptr<IEventStorage> storage_;
    std::shared_ptr<MemoryRouter> router_;
    std::chrono::steady_clock::time_point lastCleanupAt_;
};
