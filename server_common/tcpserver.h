#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
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

using json = nlohmann::json;

class TcpServer {
public:
    explicit TcpServer(std::size_t maxMessageBytes);
    ~TcpServer();

    void setCredentials(const std::string& username, const std::string& password);
    void configureStorage(const std::string& storageDir, int retentionDays);
    void configureTls(bool enabled, const std::string& certificateFile, const std::string& privateKeyFile);

    bool startServer(uint16_t port);
    void stopServer();
    void processEvents();

private:
    struct ClientConnection {
        SocketType socket = INVALID_SOCKET_VALUE;
        SSL* ssl = nullptr;
        bool authenticated = false;
        std::string username;
        std::string buffer;
        std::mutex writeMutex;
    };

    void initializeTls();
    void cleanupTls();
    bool setSocketReuseAddress(SocketType socket) const;
    bool setSocketNonBlocking(SocketType socket) const;
    bool setSocketBlocking(SocketType socket) const;
    void closeSocket(SocketType socket) const;

    void acceptNewConnections();
    void clientLoop(std::shared_ptr<ClientConnection> client);
    int readFromClient(const std::shared_ptr<ClientConnection>& client, char* buffer, std::size_t bufferSize);
    bool writeAll(const std::shared_ptr<ClientConnection>& client, const std::string& payload);
    bool sendJson(const std::shared_ptr<ClientConnection>& client, const json& message);
    void removeClient(SocketType socket);

    void processBufferedMessages(const std::shared_ptr<ClientConnection>& client);
    void processMessage(const std::shared_ptr<ClientConnection>& client, const json& message);
    bool authenticateClient(const std::shared_ptr<ClientConnection>& client, const json& message);
    void broadcastMessage(const json& message, SocketType excludeSocket);

    bool ensureStorageDir();
    void cleanupExpiredFiles();
    std::string sanitizeFileName(const std::string& fileName) const;
    bool decodeBase64(const std::string& input, std::vector<unsigned char>& output) const;
    std::string sha256Hex(const std::vector<unsigned char>& data) const;
    void persistFiles(const std::shared_ptr<ClientConnection>& client, const json& message);

    const std::size_t maxMessageBytes_;
    SocketType serverSocket_ = INVALID_SOCKET_VALUE;
    std::atomic<bool> running_ = false;
    std::map<SocketType, std::shared_ptr<ClientConnection>> clients_;
    std::mutex clientsMutex_;

    std::string username_ = "admin";
    std::string password_ = "admin";
    std::string storageDir_ = "received_files";
    int retentionDays_ = 7;

    bool tlsEnabled_ = false;
    std::string certificateFile_;
    std::string privateKeyFile_;
    SSL_CTX* sslContext_ = nullptr;

    std::chrono::steady_clock::time_point lastCleanupAt_;
};
