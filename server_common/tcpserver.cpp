#include "tcpserver.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t kReadChunkSize = 32 * 1024;
constexpr auto kCleanupInterval = std::chrono::minutes(15);

std::string currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#ifdef _WIN32
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return stream.str();
}

std::filesystem::path makeFilePathUnique(const std::filesystem::path& requestedPath)
{
    if (!std::filesystem::exists(requestedPath)) {
        return requestedPath;
    }

    const auto stem = requestedPath.stem().string();
    const auto extension = requestedPath.extension().string();

    for (int index = 1; index < 10000; ++index) {
        const auto candidate = requestedPath.parent_path() /
            (stem + "-" + std::to_string(index) + extension);
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return requestedPath.parent_path() /
        (stem + "-" + currentTimestamp() + extension);
}
}

TcpServer::TcpServer(std::size_t maxMessageBytes)
    : maxMessageBytes_(maxMessageBytes)
    , lastCleanupAt_(std::chrono::steady_clock::now())
{
#ifdef _WIN32
    WSADATA wsaData {};
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
    retentionDays_ = std::max(0, retentionDays);
}

void TcpServer::configureTls(bool enabled, const std::string& certificateFile, const std::string& privateKeyFile)
{
    tlsEnabled_ = enabled;
    certificateFile_ = certificateFile;
    privateKeyFile_ = privateKeyFile;
}

bool TcpServer::startServer(uint16_t port)
{
    if (running_) {
        return true;
    }

    if (!ensureStorageDir()) {
        return false;
    }

    if (tlsEnabled_) {
        initializeTls();
    }

    serverSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ == INVALID_SOCKET_VALUE) {
        std::cerr << "Failed to create server socket" << std::endl;
        return false;
    }

    if (!setSocketReuseAddress(serverSocket_)) {
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    if (!setSocketNonBlocking(serverSocket_)) {
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(serverSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Bind failed: " << std::strerror(errno) << std::endl;
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    if (::listen(serverSocket_, SOMAXCONN) < 0) {
        std::cerr << "Listen failed: " << std::strerror(errno) << std::endl;
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    running_ = true;
    cleanupExpiredFiles();
    std::cout << "Server listening on port " << port
              << (tlsEnabled_ ? " with TLS enabled" : " without TLS")
              << std::endl;
    return true;
}

void TcpServer::stopServer()
{
    if (!running_) {
        return;
    }

    running_ = false;

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [socket, client] : clients_) {
            if (client && client->ssl != nullptr) {
                SSL_shutdown(client->ssl);
                SSL_free(client->ssl);
                client->ssl = nullptr;
            }
            closeSocket(socket);
        }
        clients_.clear();
    }

    if (serverSocket_ != INVALID_SOCKET_VALUE) {
        closeSocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET_VALUE;
    }
}

void TcpServer::processEvents()
{
    if (!running_) {
        return;
    }

    acceptNewConnections();

    const auto now = std::chrono::steady_clock::now();
    if (now - lastCleanupAt_ >= kCleanupInterval) {
        cleanupExpiredFiles();
        lastCleanupAt_ = now;
    }
}

void TcpServer::initializeTls()
{
    if (sslContext_ != nullptr) {
        return;
    }

    if (certificateFile_.empty() || privateKeyFile_.empty()) {
        throw std::runtime_error("TLS enabled but certificate or key path is missing");
    }

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    sslContext_ = SSL_CTX_new(TLS_server_method());
    if (sslContext_ == nullptr) {
        throw std::runtime_error("Failed to create TLS context");
    }

    if (SSL_CTX_use_certificate_file(sslContext_, certificateFile_.c_str(), SSL_FILETYPE_PEM) != 1) {
        throw std::runtime_error("Failed to load TLS certificate");
    }

    if (SSL_CTX_use_PrivateKey_file(sslContext_, privateKeyFile_.c_str(), SSL_FILETYPE_PEM) != 1) {
        throw std::runtime_error("Failed to load TLS private key");
    }

    if (SSL_CTX_check_private_key(sslContext_) != 1) {
        throw std::runtime_error("TLS private key does not match certificate");
    }
}

void TcpServer::cleanupTls()
{
    if (sslContext_ != nullptr) {
        SSL_CTX_free(sslContext_);
        sslContext_ = nullptr;
    }
}

bool TcpServer::setSocketReuseAddress(SocketType socket) const
{
    int enabled = 1;
#ifdef _WIN32
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == 0;
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
    if (flags == -1) {
        return false;
    }
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool TcpServer::setSocketBlocking(SocketType socket) const
{
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(socket, F_SETFL, flags & ~O_NONBLOCK) == 0;
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

void TcpServer::acceptNewConnections()
{
    while (running_) {
        sockaddr_in clientAddress {};
        socklen_t clientAddressSize = sizeof(clientAddress);
        SocketType clientSocket = ::accept(serverSocket_,
            reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);

        if (clientSocket == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            const auto error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return;
            }
#endif
            std::cerr << "Accept failed" << std::endl;
            return;
        }

        setSocketBlocking(clientSocket);

        auto client = std::make_shared<ClientConnection>();
        client->socket = clientSocket;

        if (tlsEnabled_) {
            client->ssl = SSL_new(sslContext_);
            if (client->ssl == nullptr) {
                std::cerr << "Failed to allocate TLS session for client" << std::endl;
                closeSocket(clientSocket);
                continue;
            }

            SSL_set_fd(client->ssl, static_cast<int>(clientSocket));
            if (SSL_accept(client->ssl) != 1) {
                std::cerr << "TLS handshake failed for new client" << std::endl;
                SSL_free(client->ssl);
                client->ssl = nullptr;
                closeSocket(clientSocket);
                continue;
            }
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_[clientSocket] = client;
        }

        char clientIp[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp)) != nullptr) {
            std::cout << "Accepted connection from " << clientIp
                      << ":" << ntohs(clientAddress.sin_port)
                      << std::endl;
        }

        std::thread(&TcpServer::clientLoop, this, client).detach();
    }
}

void TcpServer::clientLoop(std::shared_ptr<ClientConnection> client)
{
    std::vector<char> buffer(kReadChunkSize);

    while (running_) {
        const int bytesRead = readFromClient(client, buffer.data(), buffer.size());
        if (bytesRead <= 0) {
            break;
        }

        client->buffer.append(buffer.data(), static_cast<std::size_t>(bytesRead));
        if (client->buffer.size() > maxMessageBytes_) {
            std::cerr << "Client message buffer exceeded limit, disconnecting socket "
                      << client->socket << std::endl;
            break;
        }

        processBufferedMessages(client);
    }

    removeClient(client->socket);
}

int TcpServer::readFromClient(const std::shared_ptr<ClientConnection>& client, char* buffer, std::size_t bufferSize)
{
    if (client->ssl != nullptr) {
        const int bytesRead = SSL_read(client->ssl, buffer, static_cast<int>(bufferSize));
        if (bytesRead <= 0) {
            const int error = SSL_get_error(client->ssl, bytesRead);
            if (error == SSL_ERROR_ZERO_RETURN) {
                return 0;
            }
            return -1;
        }
        return bytesRead;
    }

    return static_cast<int>(::recv(client->socket, buffer, bufferSize, 0));
}

bool TcpServer::writeAll(const std::shared_ptr<ClientConnection>& client, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(client->writeMutex);

    std::size_t offset = 0;
    while (offset < payload.size()) {
        int sent = 0;
        if (client->ssl != nullptr) {
            sent = SSL_write(client->ssl, payload.data() + offset,
                static_cast<int>(payload.size() - offset));
            if (sent <= 0) {
                return false;
            }
        } else {
            sent = static_cast<int>(::send(client->socket,
                payload.data() + offset,
                payload.size() - offset,
                0));
            if (sent <= 0) {
                return false;
            }
        }

        offset += static_cast<std::size_t>(sent);
    }

    return true;
}

bool TcpServer::sendJson(const std::shared_ptr<ClientConnection>& client, const json& message)
{
    return writeAll(client, message.dump() + "\n");
}

void TcpServer::removeClient(SocketType socket)
{
    std::shared_ptr<ClientConnection> client;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        const auto it = clients_.find(socket);
        if (it == clients_.end()) {
            return;
        }
        client = it->second;
        clients_.erase(it);
    }

    if (client && client->ssl != nullptr) {
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl = nullptr;
    }

    closeSocket(socket);
    std::cout << "Client disconnected: socket " << socket << std::endl;
}

void TcpServer::processBufferedMessages(const std::shared_ptr<ClientConnection>& client)
{
    std::size_t newlinePosition = std::string::npos;
    while ((newlinePosition = client->buffer.find('\n')) != std::string::npos) {
        std::string line = client->buffer.substr(0, newlinePosition);
        client->buffer.erase(0, newlinePosition + 1);

        if (line.empty()) {
            continue;
        }

        try {
            processMessage(client, json::parse(line));
        } catch (const std::exception& error) {
            json response = {
                {"type", "error"},
                {"message", std::string("Invalid JSON message: ") + error.what()}
            };
            sendJson(client, response);
        }
    }
}

void TcpServer::processMessage(const std::shared_ptr<ClientConnection>& client, const json& message)
{
    const std::string messageType = message.value("type", "");
    if (messageType.empty()) {
        sendJson(client, json{{"type", "error"}, {"message", "Missing message type"}});
        return;
    }

    if (messageType == "auth") {
        authenticateClient(client, message);
        return;
    }

    if (!client->authenticated) {
        sendJson(client, json{{"type", "error"}, {"message", "Authentication required"}});
        return;
    }

    if (messageType == "ping") {
        sendJson(client, json{{"type", "pong"}});
        return;
    }

    if (messageType == "clipboard_text") {
        json forwarded = message;
        forwarded["sender"] = client->username;
        broadcastMessage(forwarded, client->socket);
        return;
    }

    if (messageType == "file_bundle") {
        persistFiles(client, message);
        json forwarded = message;
        forwarded["sender"] = client->username;
        broadcastMessage(forwarded, client->socket);
        return;
    }

    sendJson(client, json{{"type", "error"}, {"message", "Unsupported message type"}});
}

bool TcpServer::authenticateClient(const std::shared_ptr<ClientConnection>& client, const json& message)
{
    const std::string receivedUsername = message.value("username", "");
    const std::string receivedPassword = message.value("password", "");

    const bool authenticated = receivedUsername == username_ && receivedPassword == password_;
    client->authenticated = authenticated;
    client->username = receivedUsername;

    json response = {
        {"type", "auth_response"},
        {"status", authenticated ? "ok" : "failed"},
        {"message", authenticated ? "Authenticated" : "Invalid username or password"},
        {"tls", tlsEnabled_}
    };
    sendJson(client, response);

    if (!authenticated) {
        std::cerr << "Authentication failed for socket " << client->socket << std::endl;
    }

    return authenticated;
}

void TcpServer::broadcastMessage(const json& message, SocketType excludeSocket)
{
    std::vector<std::shared_ptr<ClientConnection>> recipients;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [socket, client] : clients_) {
            if (socket != excludeSocket && client->authenticated) {
                recipients.push_back(client);
            }
        }
    }

    for (const auto& client : recipients) {
        if (!sendJson(client, message)) {
            std::cerr << "Broadcast failed for socket " << client->socket << std::endl;
        }
    }
}

bool TcpServer::ensureStorageDir()
{
    try {
        if (storageDir_.empty()) {
            storageDir_ = "received_files";
        }
        std::filesystem::create_directories(storageDir_);
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Failed to prepare storage directory: " << error.what() << std::endl;
        return false;
    }
}

void TcpServer::cleanupExpiredFiles()
{
    if (retentionDays_ <= 0) {
        return;
    }

    try {
        const auto expiration =
            std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * retentionDays_);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(storageDir_)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.last_write_time() < expiration) {
                std::filesystem::remove(entry.path());
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to cleanup expired files: " << error.what() << std::endl;
    }
}

std::string TcpServer::sanitizeFileName(const std::string& fileName) const
{
    std::string sanitized = fileName.empty() ? "clipboard-file" : fileName;
    std::replace_if(sanitized.begin(), sanitized.end(), [](char value) {
        return value == '/' || value == '\\' || value == ':' || value == '*' ||
            value == '?' || value == '"' || value == '<' || value == '>' || value == '|';
    }, '_');
    return sanitized;
}

bool TcpServer::decodeBase64(const std::string& input, std::vector<unsigned char>& output) const
{
    std::string compact;
    compact.reserve(input.size());
    for (const char character : input) {
        if (!std::isspace(static_cast<unsigned char>(character))) {
            compact.push_back(character);
        }
    }

    if (compact.empty()) {
        output.clear();
        return true;
    }

    output.assign((compact.size() * 3) / 4 + 4, 0);
    const int decodedLength = EVP_DecodeBlock(output.data(),
        reinterpret_cast<const unsigned char*>(compact.data()),
        static_cast<int>(compact.size()));
    if (decodedLength < 0) {
        output.clear();
        return false;
    }

    std::size_t actualLength = static_cast<std::size_t>(decodedLength);
    if (!compact.empty() && compact.back() == '=') {
        --actualLength;
    }
    if (compact.size() > 1 && compact[compact.size() - 2] == '=') {
        --actualLength;
    }
    output.resize(actualLength);
    return true;
}

std::string TcpServer::sha256Hex(const std::vector<unsigned char>& data) const
{
    unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
    SHA256(data.data(), data.size(), hash);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const unsigned char byte : hash) {
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

void TcpServer::persistFiles(const std::shared_ptr<ClientConnection>& client, const json& message)
{
    if (!message.contains("files") || !message["files"].is_array()) {
        sendJson(client, json{{"type", "error"}, {"message", "file_bundle message requires a files array"}});
        return;
    }

    const auto batchDir = std::filesystem::path(storageDir_) /
        (currentTimestamp() + "-" + sanitizeFileName(client->username.empty() ? "anonymous" : client->username));

    try {
        std::filesystem::create_directories(batchDir);
    } catch (const std::exception& error) {
        sendJson(client, json{{"type", "error"}, {"message", std::string("Failed to create storage directory: ") + error.what()}});
        return;
    }

    std::size_t totalBytes = 0;
    for (const auto& item : message["files"]) {
        if (!item.is_object()) {
            continue;
        }

        const std::string originalName = item.value("name", "clipboard-file");
        const std::string base64Data = item.value("data", "");
        std::vector<unsigned char> decoded;

        if (!decodeBase64(base64Data, decoded)) {
            sendJson(client, json{{"type", "error"}, {"message", "Failed to decode base64 file payload"}});
            continue;
        }

        totalBytes += decoded.size();
        if (totalBytes > maxMessageBytes_) {
            sendJson(client, json{{"type", "error"}, {"message", "Received file bundle exceeds server buffer limit"}});
            return;
        }

        const std::string expectedSha = item.value("sha256", "");
        if (!expectedSha.empty() && sha256Hex(decoded) != expectedSha) {
            sendJson(client, json{{"type", "error"}, {"message", "SHA256 mismatch while storing file"}});
            continue;
        }

        auto targetPath = makeFilePathUnique(batchDir / sanitizeFileName(originalName));
        std::ofstream stream(targetPath, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
        stream.close();
    }

    cleanupExpiredFiles();
}
