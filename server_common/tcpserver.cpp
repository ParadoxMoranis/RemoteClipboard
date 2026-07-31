#include "tcpserver.h"

#include "../client_common/protocol.h"
#include "sqlitestorage.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <openssl/err.h>
#include <openssl/ssl.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
using namespace std::chrono_literals;
using remoteclipboard::v1::type::kAuth;
using remoteclipboard::v1::type::kClipboardText;
using remoteclipboard::v1::type::kFileBundle;
using remoteclipboard::v1::type::kPing;
using remoteclipboard::v1::type::kTransferChunk;
using remoteclipboard::v1::type::kTransferComplete;
using remoteclipboard::v1::type::kTransferStart;

constexpr std::size_t kReadChunkSize = 32 * 1024;
constexpr std::size_t kMaxConnections = 1024;
constexpr std::size_t kMaxConcurrentHandshakes = 32;
constexpr std::size_t kMaxTransfersPerSession = 8;
constexpr std::size_t kMaxUnauthenticatedFrameBytes = 16 * 1024;
constexpr std::size_t kMaxAuthenticationFailuresPerMinute = 5;
constexpr std::size_t kMaxAuthenticationRateEntries = 4096;
constexpr std::size_t kWorkspaceUploadBytesPerMinute = 1024ull * 1024ull * 1024ull;
constexpr auto kSocketTimeout = 10s;
constexpr auto kCleanupInterval = 15min;
constexpr auto kTransferLifetime = 5min;

std::string makeLogString(const char* level, const char* event, const std::string& detail)
{
    nlohmann::json entry = {{"level", level}, {"event", event}, {"detail", detail}};
    return entry.dump();
}

std::string socketErrorDetail(const std::string& prefix)
{
#ifdef _WIN32
    return prefix + ": Winsock error " + std::to_string(WSAGetLastError());
#else
    return prefix + ": " + std::strerror(errno);
#endif
}

std::int64_t unixMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string hashString(const std::string& value)
{
    return filetransfer::sha256Hex(std::vector<unsigned char>(value.begin(), value.end()));
}

bool isUnsignedInteger(const nlohmann::json& value) { return value.is_number_unsigned(); }

} // namespace

TcpServer::ClientConnection::ClientConnection(std::uint64_t newSessionId, SocketType newSocket,
                                              std::string newPeerAddress, std::size_t maxFrameBytes)
    : sessionId(newSessionId), socket(newSocket), peerAddress(std::move(newPeerAddress)),
      parser(maxFrameBytes)
{}

TcpServer::TcpServer(std::size_t maxMessageBytes)
    : maxMessageBytes_(maxMessageBytes),
      maxSendQueueBytes_(std::min<std::size_t>(maxMessageBytes, 16 * 1024 * 1024)),
      router_(std::make_shared<MemoryRouter>()), lastCleanupAt_(std::chrono::steady_clock::now())
{
    if (maxMessageBytes_ == 0) {
        throw std::invalid_argument("Server message limit must be positive");
    }
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif
}

TcpServer::~TcpServer()
{
    stopServer();
    cleanupTls();
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpServer::setCredentials(const std::string& username, const std::string& password)
{
    username_ = username;
    password_ = password;
}

void TcpServer::configureStorage(const std::string& storageDir, int retentionDays)
{
    if (!storageDir.empty()) {
        storageDir_ = storageDir;
    }
    retentionDays_ = retentionDays;
}

void TcpServer::configureDatabase(const std::string& sqlitePath)
{
    sqlitePath_ = sqlitePath;
    storage_.reset();
}

void TcpServer::setStorage(std::shared_ptr<IStorage> storage)
{
    storage_ = std::move(storage);
    sqlitePath_.clear();
}

void TcpServer::configureTls(bool enabled, const std::string& certificateFile,
                             const std::string& privateKeyFile)
{
    tlsEnabled_ = enabled;
    certificateFile_ = certificateFile;
    privateKeyFile_ = privateKeyFile;
}

bool TcpServer::startServer(std::uint16_t port)
{
    if (running_) {
        return true;
    }
    if (username_.empty() || password_.empty()) {
        std::cerr << makeLogString("error", "configuration",
                                   "Authentication credentials are required")
                  << '\n';
        return false;
    }
    if (!ensureStorageDir() || !initializeStorage()) {
        return false;
    }

    try {
        if (tlsEnabled_) {
            initializeTls();
        }
    } catch (const std::exception& error) {
        std::cerr << makeLogString("error", "tls_initialization", error.what()) << '\n';
        return false;
    }

    serverSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET_VALUE) {
        std::cerr << makeLogString("error", "socket_create",
                                   socketErrorDetail("Failed to create server socket"))
                  << '\n';
        return false;
    }
    if (!setSocketReuseAddress(serverSocket_) || !setSocketNonBlocking(serverSocket_)) {
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(serverSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        ::listen(serverSocket_, SOMAXCONN) < 0) {
        std::cerr << makeLogString("error", "socket_listen", "Unable to bind or listen") << '\n';
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    running_ = true;
    cleanupExpiredFiles();
    acceptorThread_ = std::thread(&TcpServer::acceptLoop, this);
    std::cout << makeLogString(
                     "info", "server_started",
                     "Listening on port " + std::to_string(boundPort()) +
                         (tlsEnabled_ ? " with TLS 1.3" : " in development plaintext mode"))
              << '\n';
    return true;
}

void TcpServer::stopServer()
{
    const bool wasRunning = running_.exchange(false);
    if (!wasRunning && serverSocket_ == INVALID_SOCKET_VALUE && !acceptorThread_.joinable()) {
        return;
    }

    if (serverSocket_ != INVALID_SOCKET_VALUE) {
        shutdownSocket(serverSocket_);
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
    }
    if (acceptorThread_.joinable()) {
        acceptorThread_.join();
    }

    std::vector<std::shared_ptr<ClientConnection>> clients;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [socket, client] : clients_) {
            clients.push_back(client);
        }
        clients_.clear();
    }
    for (const auto& client : clients) {
        requestClientClose(client);
    }
    for (const auto& client : clients) {
        finalizeClient(client);
    }
    router_->clear();
}

void TcpServer::processEvents()
{
    reapClosedClients();
    if (!running_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastCleanupAt_ >= kCleanupInterval) {
        cleanupExpiredFiles();
        lastCleanupAt_ = now;
    }
}

std::uint16_t TcpServer::boundPort() const
{
    if (serverSocket_ == INVALID_SOCKET_VALUE) {
        return 0;
    }
    sockaddr_in address{};
#ifdef _WIN32
    int addressSize = sizeof(address);
#else
    socklen_t addressSize = sizeof(address);
#endif
    if (getsockname(serverSocket_, reinterpret_cast<sockaddr*>(&address), &addressSize) != 0) {
        return 0;
    }
    return ntohs(address.sin_port);
}

std::size_t TcpServer::activeSessionCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
}

void TcpServer::initializeTls()
{
    if (sslContext_ != nullptr) {
        return;
    }
    if (certificateFile_.empty() || privateKeyFile_.empty()) {
        throw std::runtime_error("TLS certificate and private key are required");
    }

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    sslContext_ = SSL_CTX_new(TLS_server_method());
    if (sslContext_ == nullptr) {
        throw std::runtime_error("Failed to create TLS context");
    }
    if (SSL_CTX_set_min_proto_version(sslContext_, TLS1_3_VERSION) != 1 ||
        SSL_CTX_use_certificate_file(sslContext_, certificateFile_.c_str(), SSL_FILETYPE_PEM) !=
            1 ||
        SSL_CTX_use_PrivateKey_file(sslContext_, privateKeyFile_.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(sslContext_) != 1) {
        cleanupTls();
        throw std::runtime_error("Failed to configure TLS 1.3 certificate or key");
    }
    SSL_CTX_set_mode(sslContext_, SSL_MODE_AUTO_RETRY);
}

void TcpServer::cleanupTls()
{
    if (sslContext_ != nullptr) {
        SSL_CTX_free(sslContext_);
        sslContext_ = nullptr;
    }
}

bool TcpServer::initializeStorage()
{
    if (!storage_ && !sqlitePath_.empty()) {
        storage_ = std::make_shared<SqliteStorage>(sqlitePath_);
    }
    if (!storage_) {
        return true;
    }
    const auto result = storage_->initialize();
    if (!result) {
        std::cerr << makeLogString("error", "storage_initialization", result.message) << '\n';
        return false;
    }
    return true;
}

bool TcpServer::setSocketReuseAddress(SocketType socket) const
{
    int enabled = 1;
#ifdef _WIN32
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enabled),
                      sizeof(enabled)) == 0;
#else
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == 0;
#endif
}

bool TcpServer::setSocketNonBlocking(SocketType socket) const
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags != -1 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool TcpServer::setSocketBlocking(SocketType socket) const
{
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags != -1 && fcntl(socket, F_SETFL, flags & ~O_NONBLOCK) == 0;
#endif
}

bool TcpServer::setSocketTimeouts(SocketType socket, std::chrono::seconds timeout) const
{
#ifdef _WIN32
    const DWORD milliseconds =
        static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&milliseconds),
                      sizeof(milliseconds)) == 0 &&
           setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&milliseconds),
                      sizeof(milliseconds)) == 0;
#else
    timeval value{};
    value.tv_sec = timeout.count();
    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) == 0 &&
           setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value)) == 0;
#endif
}

void TcpServer::shutdownSocket(SocketType socket) const
{
    if (socket == INVALID_SOCKET_VALUE) {
        return;
    }
#ifdef _WIN32
    ::shutdown(socket, SD_BOTH);
#else
    ::shutdown(socket, SHUT_RDWR);
#endif
}

void TcpServer::closeSocket(SocketType socket) const
{
    if (socket == INVALID_SOCKET_VALUE) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void TcpServer::acceptLoop()
{
    while (running_) {
        acceptNewConnections();
        std::this_thread::sleep_for(10ms);
    }
}

void TcpServer::acceptNewConnections()
{
    while (running_) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientAddressSize = sizeof(clientAddress);
#else
        socklen_t clientAddressSize = sizeof(clientAddress);
#endif
        const SocketType clientSocket = ::accept(
            serverSocket_, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
        if (clientSocket == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                return;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
                return;
            }
#endif
            return;
        }

        bool atConnectionLimit = false;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            atConnectionLimit = clients_.size() >= kMaxConnections;
        }
        const bool atHandshakeLimit =
            tlsEnabled_ && activeHandshakes_.load() >= kMaxConcurrentHandshakes;
        if (atConnectionLimit || atHandshakeLimit || !setSocketBlocking(clientSocket) ||
            !setSocketTimeouts(clientSocket, kSocketTimeout)) {
            closeSocket(clientSocket);
            continue;
        }

        char peerAddress[INET_ADDRSTRLEN] = {0};
        const char* convertedAddress =
            inet_ntop(AF_INET, &clientAddress.sin_addr, peerAddress, sizeof(peerAddress));
        auto client = std::make_shared<ClientConnection>(
            nextSessionId_.fetch_add(1), clientSocket,
            convertedAddress != nullptr ? peerAddress : "unknown", maxMessageBytes_);
        if (tlsEnabled_) {
            activeHandshakes_.fetch_add(1);
        }
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.emplace(clientSocket, client);
        }
        client->readerThread = std::thread(&TcpServer::clientLoop, this, client);
    }
}

void TcpServer::clientLoop(const std::shared_ptr<ClientConnection>& client)
{
    bool handshakeSlot = tlsEnabled_;
    const auto releaseHandshakeSlot = [&]() {
        if (handshakeSlot) {
            activeHandshakes_.fetch_sub(1);
            handshakeSlot = false;
        }
    };

    if (tlsEnabled_) {
        client->ssl = SSL_new(sslContext_);
        if (client->ssl == nullptr) {
            releaseHandshakeSlot();
            requestClientClose(client);
            client->finished = true;
            return;
        }
        SSL_set_fd(client->ssl, static_cast<int>(client->socket));
        if (SSL_accept(client->ssl) != 1) {
            releaseHandshakeSlot();
            requestClientClose(client);
            client->finished = true;
            return;
        }
    }
    releaseHandshakeSlot();

    client->writerThread = std::thread(&TcpServer::writerLoop, this, client);
    std::array<char, kReadChunkSize> buffer{};
    while (running_ && !client->closing) {
        const int bytesRead = readFromClient(client, buffer.data(), buffer.size());
        if (bytesRead <= 0) {
            break;
        }
        if (!client->parser.append(buffer.data(), static_cast<std::size_t>(bytesRead))) {
            sendProtocolError(client, "FRAME_TOO_LARGE",
                              "JSON Lines frame exceeds the configured limit");
            std::this_thread::sleep_for(50ms);
            break;
        }
        if (!client->authenticated &&
            client->parser.pendingBytes() > kMaxUnauthenticatedFrameBytes) {
            sendProtocolError(client, "QUOTA_EXCEEDED", "Unauthenticated frame limit exceeded");
            std::this_thread::sleep_for(50ms);
            break;
        }

        std::string frame;
        while (client->parser.popFrame(frame)) {
            if (!client->authenticated && frame.size() > kMaxUnauthenticatedFrameBytes) {
                sendProtocolError(client, "QUOTA_EXCEEDED", "Unauthenticated frame limit exceeded");
                std::this_thread::sleep_for(50ms);
                requestClientClose(client);
                break;
            }
            if (!frame.empty()) {
                processFrame(client, frame);
            }
            if (client->closing) {
                break;
            }
        }
    }

    router_->unregisterSession(client->sessionId);
    cleanupClientTransfers(client);
    requestClientClose(client);
    client->finished = true;
}

void TcpServer::writerLoop(const std::shared_ptr<ClientConnection>& client)
{
    while (true) {
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(client->queueMutex);
            client->queueCondition.wait(
                lock, [&]() { return client->closing || !client->sendQueue.empty(); });
            if (client->sendQueue.empty()) {
                return;
            }
            payload = std::move(client->sendQueue.front());
            client->sendQueue.pop_front();
            client->queuedBytes -= payload.size();
        }
        if (!writeRaw(client, payload)) {
            requestClientClose(client);
            return;
        }
    }
}

int TcpServer::readFromClient(const std::shared_ptr<ClientConnection>& client, char* buffer,
                              std::size_t bufferSize)
{
    if (client->ssl != nullptr) {
        const int bytesRead = SSL_read(client->ssl, buffer, static_cast<int>(bufferSize));
        if (bytesRead > 0) {
            return bytesRead;
        }
        const int error = SSL_get_error(client->ssl, bytesRead);
        return error == SSL_ERROR_WANT_READ ? 0 : -1;
    }
    return static_cast<int>(::recv(client->socket, buffer, static_cast<int>(bufferSize), 0));
}

bool TcpServer::writeRaw(const std::shared_ptr<ClientConnection>& client,
                         const std::string& payload)
{
    std::size_t offset = 0;
    while (offset < payload.size() && !client->closing) {
        int sent = 0;
        if (client->ssl != nullptr) {
            sent = SSL_write(client->ssl, payload.data() + offset,
                             static_cast<int>(payload.size() - offset));
        } else {
            sent = static_cast<int>(::send(client->socket, payload.data() + offset,
                                           static_cast<int>(payload.size() - offset), 0));
        }
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return offset == payload.size();
}

bool TcpServer::enqueue(const std::shared_ptr<ClientConnection>& client, std::string payload)
{
    if (!client || client->closing) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(client->queueMutex);
        if (client->closing || payload.size() > maxSendQueueBytes_ ||
            client->queuedBytes > maxSendQueueBytes_ - payload.size()) {
            requestClientClose(client);
            return false;
        }
        client->queuedBytes += payload.size();
        client->sendQueue.push_back(std::move(payload));
    }
    client->queueCondition.notify_one();
    return true;
}

bool TcpServer::sendJson(const std::shared_ptr<ClientConnection>& client, const Json& message)
{
    return enqueue(client, message.dump() + "\n");
}

void TcpServer::requestClientClose(const std::shared_ptr<ClientConnection>& client)
{
    if (!client) {
        return;
    }
    if (!client->closing.exchange(true)) {
        shutdownSocket(client->socket);
    }
    client->queueCondition.notify_all();
}

void TcpServer::finalizeClient(const std::shared_ptr<ClientConnection>& client)
{
    if (!client) {
        return;
    }
    requestClientClose(client);
    if (client->readerThread.joinable() &&
        client->readerThread.get_id() != std::this_thread::get_id()) {
        client->readerThread.join();
    }
    if (client->writerThread.joinable() &&
        client->writerThread.get_id() != std::this_thread::get_id()) {
        client->writerThread.join();
    }
    cleanupClientTransfers(client);
    if (client->ssl != nullptr) {
        SSL_free(client->ssl);
        client->ssl = nullptr;
    }
    closeSocket(client->socket);
    client->socket = INVALID_SOCKET_VALUE;
}

void TcpServer::reapClosedClients()
{
    std::vector<std::shared_ptr<ClientConnection>> finished;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto iterator = clients_.begin(); iterator != clients_.end();) {
            if (iterator->second->finished) {
                finished.push_back(iterator->second);
                iterator = clients_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
    for (const auto& client : finished) {
        finalizeClient(client);
    }
}

void TcpServer::processFrame(const std::shared_ptr<ClientConnection>& client,
                             const std::string& frame)
{
    try {
        const Json message = Json::parse(frame);
        const std::size_t routablePayloadLimit =
            maxSendQueueBytes_ > 4096 ? maxSendQueueBytes_ - 4096 : maxSendQueueBytes_;
        const auto validation =
            remoteclipboard::v1::validateClientMessage(message, routablePayloadLimit);
        if (!validation.valid) {
            sendProtocolError(client, "INVALID_MESSAGE", validation.error, &message);
            return;
        }
        processMessage(client, message);
    } catch (const std::exception&) {
        sendProtocolError(client, "INVALID_JSON", "Invalid JSON message");
    }
}

void TcpServer::processMessage(const std::shared_ptr<ClientConnection>& client, const Json& message)
{
    cleanupExpiredTransfers(client);
    const std::string messageType = message["type"].get<std::string>();
    if (messageType == kAuth) {
        authenticateClient(client, message);
        return;
    }
    if (!client->authenticated) {
        sendProtocolError(client, "UNAUTHENTICATED", "Authentication required", &message);
        return;
    }
    if (messageType == kPing) {
        sendJson(client, Json{{"type", remoteclipboard::v1::type::kPong}});
        return;
    }

    if (messageType == kClipboardText) {
        Json forwarded = message;
        forwarded["sender"] = client->username;
        recordAcceptedEvent(forwarded, client->username);
        broadcastMessage(forwarded, client->sessionId);
        return;
    }
    if (messageType == kFileBundle) {
        if (persistFiles(client, message)) {
            Json forwarded = message;
            forwarded["sender"] = client->username;
            recordAcceptedEvent(forwarded, client->username);
            broadcastMessage(forwarded, client->sessionId);
        }
        return;
    }
    if (messageType == kTransferStart || messageType == kTransferChunk ||
        messageType == kTransferComplete) {
        if (handleChunkTransfer(client, message)) {
            Json forwarded = message;
            forwarded["sender"] = client->username;
            recordAcceptedEvent(forwarded, client->username);
            broadcastMessage(forwarded, client->sessionId);
        }
    }
}

bool TcpServer::authenticateClient(const std::shared_ptr<ClientConnection>& client,
                                   const Json& message)
{
    if (client->authenticated) {
        sendProtocolError(client, "ALREADY_AUTHENTICATED", "Session is already authenticated",
                          &message);
        return true;
    }
    const std::string receivedUsername = message["username"].get<std::string>();
    const std::string receivedPassword = message["password"].get<std::string>();
    const bool rateAllowed = authenticationAllowed(client->peerAddress, receivedUsername);
    const bool authenticated =
        rateAllowed && receivedUsername == username_ && receivedPassword == password_;
    sendJson(client,
             Json{{"type", remoteclipboard::v1::type::kAuthResponse},
                  {"status", authenticated ? "ok" : "failed"},
                  {"message", authenticated ? "Authenticated" : "Invalid username or password"},
                  {"tls", tlsEnabled_}});

    if (!authenticated) {
        recordAuthenticationFailure(client->peerAddress, receivedUsername);
        std::cerr << makeLogString("warning", "authentication_failed",
                                   "session=" + std::to_string(client->sessionId))
                  << '\n';
        std::this_thread::sleep_for(100ms);
        requestClientClose(client);
        return false;
    }

    client->authenticated = true;
    client->username = receivedUsername;
    registerAuthenticatedClient(client);
    return true;
}

bool TcpServer::authenticationAllowed(const std::string& peerAddress, const std::string& username)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(authenticationRateMutex_);
    if (authenticationRates_.size() >= kMaxAuthenticationRateEntries) {
        for (auto iterator = authenticationRates_.begin();
             iterator != authenticationRates_.end();) {
            if (now - iterator->second.windowStartedAt >= 1min) {
                iterator = authenticationRates_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
    const auto belowLimit = [&](const std::string& key) {
        const auto iterator = authenticationRates_.find(key);
        return iterator == authenticationRates_.end() ||
               now - iterator->second.windowStartedAt >= 1min ||
               iterator->second.failures < kMaxAuthenticationFailuresPerMinute;
    };
    return authenticationRates_.size() < kMaxAuthenticationRateEntries &&
           belowLimit("ip:" + peerAddress) && belowLimit("account:" + username);
}

void TcpServer::recordAuthenticationFailure(const std::string& peerAddress,
                                            const std::string& username)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(authenticationRateMutex_);
    const auto increment = [&](const std::string& key) {
        if (authenticationRates_.size() >= kMaxAuthenticationRateEntries &&
            authenticationRates_.count(key) == 0) {
            return;
        }
        auto& state = authenticationRates_[key];
        if (state.windowStartedAt.time_since_epoch().count() == 0 ||
            now - state.windowStartedAt >= 1min) {
            state.failures = 0;
            state.windowStartedAt = now;
        }
        ++state.failures;
    };
    increment("ip:" + peerAddress);
    increment("account:" + username);
}

bool TcpServer::consumeUploadBudget(std::size_t bytes)
{
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(uploadRateMutex_);
    if (now - uploadWindowStartedAt_ >= 1min) {
        uploadWindowStartedAt_ = now;
        uploadedBytesInWindow_ = 0;
    }
    if (bytes > kWorkspaceUploadBytesPerMinute - uploadedBytesInWindow_) {
        return false;
    }
    uploadedBytesInWindow_ += bytes;
    return true;
}

void TcpServer::registerAuthenticatedClient(const std::shared_ptr<ClientConnection>& client)
{
    std::weak_ptr<ClientConnection> weakClient = client;
    router_->registerSession(client->sessionId, [this, weakClient](const std::string& payload) {
        const auto target = weakClient.lock();
        return target && enqueue(target, payload);
    });
}

void TcpServer::broadcastMessage(const Json& message, std::uint64_t excludedSession)
{
    router_->broadcast(message.dump() + "\n", excludedSession);
}

void TcpServer::sendProtocolError(const std::shared_ptr<ClientConnection>& client,
                                  const std::string& code, const std::string& message,
                                  const Json* request)
{
    Json response = {
        {"type", remoteclipboard::v1::type::kError}, {"code", code}, {"message", message}};
    if (request != nullptr && request->contains("request_id") &&
        (*request)["request_id"].is_string()) {
        response["request_id"] = (*request)["request_id"];
    }
    sendJson(client, response);
}

void TcpServer::recordAcceptedEvent(const Json& message, const std::string& sender)
{
    if (!storage_) {
        return;
    }
    const std::string payload = message.dump();
    StoredEvent event;
    event.eventId = message.contains("event_id") && message["event_id"].is_string()
                        ? message["event_id"].get<std::string>()
                        : "legacy-" + std::to_string(unixMilliseconds()) + "-" +
                              std::to_string(nextEventId_.fetch_add(1));
    event.workspaceId = message.value("workspace_id", "legacy");
    event.deviceId = message.value("device_id", sender);
    event.sender = sender;
    event.type = message.value("type", "unknown");
    event.sequence = message.contains("sequence") && message["sequence"].is_number_unsigned()
                         ? message["sequence"].get<std::uint64_t>()
                         : 0;
    event.contentSha256 = hashString(payload);
    event.payloadBytes = payload.size();
    event.createdAtUnixMs = unixMilliseconds();
    const auto result = storage_->recordEvent(event);
    if (!result) {
        std::cerr << makeLogString("error", "storage_record_failed", result.message) << '\n';
    }
}

bool TcpServer::ensureStorageDir()
{
    if (storageDir_.empty()) {
        return false;
    }
    if (!filetransfer::ensureDirectory(storageDir_) ||
        !filetransfer::ensureDirectory(std::filesystem::path(storageDir_) / ".transfers")) {
        std::cerr << makeLogString("error", "storage_directory", "Unable to prepare file storage")
                  << '\n';
        return false;
    }
    return true;
}

void TcpServer::cleanupExpiredFiles()
{
    if (retentionDays_ < 0) {
        return;
    }
    try {
        const auto expiration =
            std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * retentionDays_);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(storageDir_)) {
            const auto relativePath = entry.path().lexically_relative(storageDir_);
            if (!relativePath.empty() && *relativePath.begin() == ".transfers") {
                continue;
            }
            if (entry.is_regular_file() && entry.last_write_time() < expiration) {
                std::filesystem::remove(entry.path());
            }
        }
    } catch (const std::exception& error) {
        std::cerr << makeLogString("error", "retention_cleanup", error.what()) << '\n';
    }
}

bool TcpServer::persistFiles(const std::shared_ptr<ClientConnection>& client, const Json& message)
{
    struct PendingFile
    {
        std::string name;
        std::vector<unsigned char> data;
    };
    std::vector<PendingFile> pendingFiles;
    std::size_t totalBytes = 0;
    if (message["files"].size() > 64) {
        sendProtocolError(client, "QUOTA_EXCEEDED", "A bundle may contain at most 64 files",
                          &message);
        return false;
    }
    const std::size_t routablePayloadLimit =
        maxSendQueueBytes_ > 4096 ? maxSendQueueBytes_ - 4096 : maxSendQueueBytes_;
    for (const auto& item : message["files"]) {
        if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
            !item.contains("data") || !item["data"].is_string()) {
            sendProtocolError(client, "INVALID_FILE", "Each bundled file requires name and data",
                              &message);
            return false;
        }
        const auto& encoded = item["data"].get_ref<const std::string&>();
        if (totalBytes > routablePayloadLimit ||
            encoded.size() > (routablePayloadLimit - totalBytes) / 3 * 4 + 8) {
            sendProtocolError(client, "QUOTA_EXCEEDED", "File bundle exceeds the server limit",
                              &message);
            return false;
        }
        std::vector<unsigned char> decoded;
        if (!filetransfer::decodeBase64(encoded, decoded) ||
            decoded.size() > routablePayloadLimit - totalBytes) {
            sendProtocolError(client, "INVALID_FILE", "Invalid Base64 file data", &message);
            return false;
        }
        if (item.contains("size") && (!isUnsignedInteger(item["size"]) ||
                                      item["size"].get<std::uint64_t>() != decoded.size())) {
            sendProtocolError(client, "CHECKSUM_MISMATCH", "Bundled file size mismatch", &message);
            return false;
        }
        if (item.contains("sha256")) {
            if (!item["sha256"].is_string() ||
                !filetransfer::isValidSha256(item["sha256"].get<std::string>()) ||
                filetransfer::sha256Hex(decoded) != item["sha256"].get<std::string>()) {
                sendProtocolError(client, "CHECKSUM_MISMATCH", "Bundled file SHA-256 mismatch",
                                  &message);
                return false;
            }
        }
        totalBytes += decoded.size();
        pendingFiles.push_back(
            {filetransfer::sanitizeFileName(item["name"].get<std::string>()), std::move(decoded)});
    }
    if (!consumeUploadBudget(totalBytes)) {
        sendProtocolError(client, "RATE_LIMITED", "Workspace upload rate exceeded", &message);
        return false;
    }

    const auto batchDir =
        std::filesystem::path(storageDir_) /
        (filetransfer::currentTimestamp() + "-" + std::to_string(nextEventId_.fetch_add(1)) + "-" +
         filetransfer::sanitizeFileName(client->username));
    if (!filetransfer::ensureDirectory(batchDir)) {
        sendProtocolError(client, "INTERNAL", "Unable to create bundle directory", &message);
        return false;
    }

    std::vector<std::filesystem::path> temporaryPaths;
    std::vector<std::filesystem::path> finalPaths;
    for (std::size_t index = 0; index < pendingFiles.size(); ++index) {
        const auto temporaryPath = batchDir / ("." + std::to_string(index) + ".part");
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(pendingFiles[index].data.data()),
                     static_cast<std::streamsize>(pendingFiles[index].data.size()));
        output.close();
        if (!output) {
            std::filesystem::remove_all(batchDir);
            sendProtocolError(client, "INTERNAL", "Unable to persist bundled file", &message);
            return false;
        }
        temporaryPaths.push_back(temporaryPath);
        finalPaths.push_back(batchDir / pendingFiles[index].name);
    }

    std::vector<std::filesystem::path> committed;
    for (std::size_t index = 0; index < temporaryPaths.size(); ++index) {
        const auto finalPath = filetransfer::makeFilePathUnique(finalPaths[index]);
        std::error_code error;
        std::filesystem::rename(temporaryPaths[index], finalPath, error);
        if (error) {
            for (const auto& path : committed) {
                std::filesystem::remove(path);
            }
            std::filesystem::remove_all(batchDir);
            sendProtocolError(client, "INTERNAL", "Unable to commit bundled files", &message);
            return false;
        }
        committed.push_back(finalPath);
    }
    return true;
}

bool TcpServer::handleChunkTransfer(const std::shared_ptr<ClientConnection>& client,
                                    const Json& message)
{
    const std::string messageType = message["type"].get<std::string>();
    if (messageType == kTransferStart) {
        return handleChunkTransferStart(client, message);
    }
    if (messageType == kTransferChunk) {
        return handleChunkTransferChunk(client, message);
    }
    return handleChunkTransferComplete(client, message);
}

bool TcpServer::handleChunkTransferStart(const std::shared_ptr<ClientConnection>& client,
                                         const Json& message)
{
    const std::string transferId = message["transfer_id"].get<std::string>();
    const std::uint64_t declaredSize = message["size"].get<std::uint64_t>();
    if (client->incomingTransfers.count(transferId) != 0) {
        sendProtocolError(client, "INVALID_SEQUENCE", "Duplicate transfer_id", &message);
        return false;
    }
    if (client->incomingTransfers.size() >= kMaxTransfersPerSession ||
        declaredSize > maxMessageBytes_) {
        sendProtocolError(client, "QUOTA_EXCEEDED", "Transfer quota exceeded", &message);
        return false;
    }
    std::uint64_t declaredTotal = declaredSize;
    for (const auto& [id, transfer] : client->incomingTransfers) {
        if (transfer.expectedSize > maxMessageBytes_ - declaredTotal) {
            sendProtocolError(client, "QUOTA_EXCEEDED", "Session upload quota exceeded", &message);
            return false;
        }
        declaredTotal += transfer.expectedSize;
    }
    const std::string sha256 = message.value("sha256", "");
    if (!sha256.empty() && !filetransfer::isValidSha256(sha256)) {
        sendProtocolError(client, "INVALID_MESSAGE", "Invalid SHA-256 metadata", &message);
        return false;
    }

    filetransfer::IncomingTransferState transfer;
    transfer.transferId = transferId;
    transfer.sender = client->username;
    transfer.fileName = filetransfer::sanitizeFileName(message["name"].get<std::string>());
    transfer.mimeType = message.value("mime", "");
    transfer.sha256 = sha256;
    transfer.expectedSize = static_cast<std::size_t>(declaredSize);
    transfer.directory =
        std::filesystem::path(storageDir_) /
        (filetransfer::currentTimestamp() + "-" + std::to_string(nextEventId_.fetch_add(1)) + "-" +
         filetransfer::sanitizeFileName(client->username));
    if (!filetransfer::ensureDirectory(transfer.directory)) {
        sendProtocolError(client, "INTERNAL", "Unable to create transfer directory", &message);
        return false;
    }
    transfer.targetPath = transfer.directory / transfer.fileName;
    transfer.temporaryPath = std::filesystem::path(storageDir_) / ".transfers" /
                             (std::to_string(client->sessionId) + "-" +
                              std::to_string(nextEventId_.fetch_add(1)) + ".part");
    transfer.output =
        std::make_unique<std::ofstream>(transfer.temporaryPath, std::ios::binary | std::ios::trunc);
    if (!transfer.output || !transfer.output->is_open()) {
        std::filesystem::remove_all(transfer.directory);
        sendProtocolError(client, "INTERNAL", "Unable to open transfer work file", &message);
        return false;
    }
    transfer.expiresAt = std::chrono::steady_clock::now() + kTransferLifetime;
    client->incomingTransfers.emplace(transferId, std::move(transfer));
    return true;
}

bool TcpServer::handleChunkTransferChunk(const std::shared_ptr<ClientConnection>& client,
                                         const Json& message)
{
    const std::string transferId = message["transfer_id"].get<std::string>();
    auto iterator = client->incomingTransfers.find(transferId);
    if (iterator == client->incomingTransfers.end()) {
        sendProtocolError(client, "TRANSFER_EXPIRED", "Unknown or expired transfer_id", &message);
        return false;
    }
    auto& transfer = iterator->second;
    const std::uint64_t sequence = message["seq"].get<std::uint64_t>();
    if (sequence != transfer.nextSequence) {
        sendProtocolError(client, "INVALID_SEQUENCE", "Chunk sequence is not the expected value",
                          &message);
        return false;
    }
    const auto& encoded = message["data"].get_ref<const std::string&>();
    if (encoded.size() > ((filetransfer::kChunkSizeBytes + 2) / 3) * 4 + 4) {
        sendProtocolError(client, "FRAME_TOO_LARGE", "Transfer chunk exceeds the chunk limit",
                          &message);
        return false;
    }
    std::vector<unsigned char> decoded;
    if (!filetransfer::decodeBase64(encoded, decoded) ||
        decoded.size() > filetransfer::kChunkSizeBytes ||
        decoded.size() > transfer.expectedSize - transfer.receivedSize) {
        sendProtocolError(client, "INVALID_MESSAGE", "Invalid or oversized transfer chunk",
                          &message);
        return false;
    }
    if (!consumeUploadBudget(decoded.size())) {
        sendProtocolError(client, "RATE_LIMITED", "Workspace upload rate exceeded", &message);
        return false;
    }
    transfer.output->write(reinterpret_cast<const char*>(decoded.data()),
                           static_cast<std::streamsize>(decoded.size()));
    if (!*transfer.output) {
        discardTransfer(transfer);
        client->incomingTransfers.erase(iterator);
        sendProtocolError(client, "INTERNAL", "Unable to write transfer chunk", &message);
        return false;
    }
    transfer.receivedSize += decoded.size();
    ++transfer.nextSequence;
    transfer.expiresAt = std::chrono::steady_clock::now() + kTransferLifetime;
    return true;
}

bool TcpServer::handleChunkTransferComplete(const std::shared_ptr<ClientConnection>& client,
                                            const Json& message)
{
    const std::string transferId = message["transfer_id"].get<std::string>();
    auto iterator = client->incomingTransfers.find(transferId);
    if (iterator == client->incomingTransfers.end()) {
        sendProtocolError(client, "TRANSFER_EXPIRED", "Unknown or expired transfer_id", &message);
        return false;
    }
    auto transfer = std::move(iterator->second);
    client->incomingTransfers.erase(iterator);
    if (transfer.output && transfer.output->is_open()) {
        transfer.output->close();
    }
    if (transfer.receivedSize != transfer.expectedSize) {
        discardTransfer(transfer);
        sendProtocolError(client, "CHECKSUM_MISMATCH", "Transfer size does not match metadata",
                          &message);
        return false;
    }
    if (!transfer.sha256.empty() &&
        filetransfer::sha256HexForFile(transfer.temporaryPath) != transfer.sha256) {
        discardTransfer(transfer);
        sendProtocolError(client, "CHECKSUM_MISMATCH", "Transfer SHA-256 does not match metadata",
                          &message);
        return false;
    }
    transfer.targetPath = filetransfer::makeFilePathUnique(transfer.targetPath);
    std::error_code error;
    std::filesystem::rename(transfer.temporaryPath, transfer.targetPath, error);
    if (error) {
        discardTransfer(transfer);
        sendProtocolError(client, "INTERNAL", "Unable to commit transfer", &message);
        return false;
    }
    return true;
}

void TcpServer::cleanupExpiredTransfers(const std::shared_ptr<ClientConnection>& client)
{
    const auto now = std::chrono::steady_clock::now();
    for (auto iterator = client->incomingTransfers.begin();
         iterator != client->incomingTransfers.end();) {
        if (iterator->second.expiresAt <= now) {
            discardTransfer(iterator->second);
            iterator = client->incomingTransfers.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void TcpServer::discardTransfer(filetransfer::IncomingTransferState& transfer)
{
    if (transfer.output && transfer.output->is_open()) {
        transfer.output->close();
    }
    std::error_code error;
    if (!transfer.temporaryPath.empty()) {
        std::filesystem::remove(transfer.temporaryPath, error);
    }
    if (!transfer.directory.empty()) {
        std::filesystem::remove(transfer.directory, error);
    }
}

void TcpServer::cleanupClientTransfers(const std::shared_ptr<ClientConnection>& client)
{
    if (!client) {
        return;
    }
    for (auto& [transferId, transfer] : client->incomingTransfers) {
        discardTransfer(transfer);
    }
    client->incomingTransfers.clear();
}
