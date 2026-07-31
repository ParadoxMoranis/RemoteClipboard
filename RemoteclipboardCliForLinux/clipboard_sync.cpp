#include "../cli_common/config.h"
#include "../client_common/protocol.h"
#include "../server_common/filetransfer.h"
#include "../server_common/parseutils.h"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
constexpr auto kClipboardPollInterval = std::chrono::milliseconds(500);
constexpr auto kHeartbeatInterval = std::chrono::seconds(10);
constexpr auto kHeartbeatTimeout = std::chrono::seconds(30);

std::atomic<bool> g_running = true;

void signalHandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        g_running = false;
    }
}

std::string trim(const std::string& value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string runCommand(const std::string& command)
{
    std::array<char, 4096> buffer {};
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

std::vector<std::string> splitLines(const std::string& content)
{
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::string percentDecode(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = value.substr(index + 1, 2);
            decoded.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
            index += 2;
        } else if (value[index] == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(value[index]);
        }
    }

    return decoded;
}

std::string localPathFromUri(const std::string& uri)
{
    constexpr auto prefix = "file://";
    if (uri.rfind(prefix, 0) != 0) {
        return {};
    }

    std::string path = uri.substr(std::strlen(prefix));
    if (path.rfind("localhost/", 0) == 0) {
        path = path.substr(std::strlen("localhost"));
    }
    return percentDecode(path);
}

bool writeClipboardText(const std::string& content)
{
    FILE* pipe = popen("wl-copy", "w");
    if (pipe == nullptr) {
        return false;
    }
    const bool ok = fwrite(content.data(), 1, content.size(), pipe) == content.size();
    pclose(pipe);
    return ok;
}

struct ClipboardOutgoingMessage {
    json message;
    bool chunked = false;
    std::vector<std::filesystem::path> paths;
};

struct ClipboardState {
    std::string signature;
};

void printHelp()
{
    std::cout
        << "clipboard_sync\n"
        << "Usage: clipboard_sync [options]\n\n"
        << "Options:\n"
        << "  --host <host>             Server address (default: 127.0.0.1)\n"
        << "  --port <port>             Server port (default: 8080)\n"
        << "  --username <name>         Authentication username (default: admin)\n"
        << "  --password-file <path>    Authentication secret file\n"
        << "  --receive-dir <path>      Directory for received files\n"
        << "  --tls                     Enable TLS\n"
        << "  --tls-ca <path>           Optional CA certificate for TLS\n"
        << "  --development             Explicitly allow a plaintext development profile\n"
        << "  --insecure-tls            Ignore certificate errors; requires --development\n"
        << "  -h, --help                Show this help message\n";
}

CliAppConfig parseArgs(int argc, char* argv[])
{
    CliAppConfig config;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            std::exit(0);
        }
        if (arg == "--host" && index + 1 < argc) {
            config.host = argv[++index];
            continue;
        }
        if (arg == "--port" && index + 1 < argc) {
            config.port = parsePortArg(argv[++index], "--port");
            continue;
        }
        if (arg == "--username" && index + 1 < argc) {
            config.username = argv[++index];
            continue;
        }
        if (arg == "--password" && index + 1 < argc) {
            config.password = argv[++index];
            config.passwordFromCommandLine = true;
            continue;
        }
        if (arg == "--password-file" && index + 1 < argc) {
            config.passwordSecretFile = argv[++index];
            continue;
        }
        if (arg == "--receive-dir" && index + 1 < argc) {
            config.receiveDir = argv[++index];
            continue;
        }
        if (arg == "--tls") {
            config.tlsEnabled = true;
            continue;
        }
        if (arg == "--tls-ca" && index + 1 < argc) {
            config.tlsCaFile = argv[++index];
            continue;
        }
        if (arg == "--development") {
            config.developmentMode = true;
            config.tlsEnabled = false;
            continue;
        }
        if (arg == "--insecure-tls") {
            config.allowInsecureTls = true;
            continue;
        }
        throw std::invalid_argument("Unknown or incomplete option: " + arg);
    }

    return config;
}

std::optional<ClipboardOutgoingMessage> pollClipboard(ClipboardState& state)
{
    const std::string mimeTypes = runCommand("wl-paste --list-types 2>/dev/null");
    if (mimeTypes.find("text/uri-list") != std::string::npos) {
        const std::string uriPayload = runCommand("wl-paste --type text/uri-list --no-newline 2>/dev/null");
        const auto uriLines = splitLines(uriPayload);
        std::vector<std::filesystem::path> paths;
        std::size_t totalBytes = 0;
        json files = json::array();

        for (const auto& uri : uriLines) {
            const std::string filePath = localPathFromUri(uri);
            if (filePath.empty() || !std::filesystem::exists(filePath)) {
                continue;
            }

            const auto path = std::filesystem::path(filePath);
            const std::size_t fileSize = static_cast<std::size_t>(std::filesystem::file_size(path));
            totalBytes += fileSize;
            paths.push_back(path);

            if (totalBytes <= filetransfer::kChunkTransferThresholdBytes) {
                std::ifstream stream(path, std::ios::binary);
                if (!stream) {
                    continue;
                }
                std::vector<unsigned char> data(
                    (std::istreambuf_iterator<char>(stream)),
                    std::istreambuf_iterator<char>());

                json fileObject;
                fileObject["name"] = path.filename().string();
                fileObject["size"] = data.size();
                fileObject["sha256"] = filetransfer::sha256Hex(data);
                fileObject["data"] = filetransfer::encodeBase64(data);
                files.push_back(fileObject);
            }
        }

        if (!paths.empty()) {
            std::ostringstream signature;
            signature << "files:";
            for (const auto& path : paths) {
                signature << path.string() << "|";
            }

            if (signature.str() != state.signature) {
                state.signature = signature.str();
                if (totalBytes <= filetransfer::kChunkTransferThresholdBytes) {
                    return ClipboardOutgoingMessage{
                        json{{"type", "file_bundle"}, {"files", files}},
                        false,
                        paths
                    };
                }
                return ClipboardOutgoingMessage{json{}, true, paths};
            }
            return std::nullopt;
        }
    }

    const std::string text = runCommand("wl-paste 2>/dev/null");
    const std::string signature = "text:" + text;
    if (signature != state.signature) {
        state.signature = signature;
        return ClipboardOutgoingMessage{
            json{{"type", "clipboard_text"}, {"content", text}},
            false,
            {}
        };
    }

    return std::nullopt;
}

struct IncomingTransfer {
    std::string transferId;
    std::string sender;
    std::string fileName;
    std::string sha256;
    std::filesystem::path targetPath;
    std::ofstream output;
    std::size_t expectedSize = 0;
    std::size_t receivedSize = 0;
};

class ClientConnection {
public:
    explicit ClientConnection(CliAppConfig config)
        : config_(std::move(config))
    {
    }

    ~ClientConnection()
    {
        close();
    }

    bool connectToServer()
    {
        close();

        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddress {};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(config_.port);

        if (inet_pton(AF_INET, config_.host.c_str(), &serverAddress.sin_addr) <= 0) {
            hostent* host = gethostbyname(config_.host.c_str());
            if (host == nullptr) {
                std::cerr << "Failed to resolve host: " << config_.host << std::endl;
                close();
                return false;
            }
            std::memcpy(&serverAddress.sin_addr, host->h_addr, host->h_length);
        }

        if (::connect(socket_, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
            std::cerr << "Connect failed" << std::endl;
            close();
            return false;
        }

        if (config_.tlsEnabled) {
            SSL_load_error_strings();
            OpenSSL_add_ssl_algorithms();

            context_ = SSL_CTX_new(TLS_client_method());
            if (context_ == nullptr) {
                std::cerr << "Failed to create TLS context" << std::endl;
                close();
                return false;
            }
            if (SSL_CTX_set_min_proto_version(context_, TLS1_3_VERSION) != 1) {
                std::cerr << "Failed to require TLS 1.3" << std::endl;
                close();
                return false;
            }

            if (!config_.tlsCaFile.empty()) {
                if (SSL_CTX_load_verify_locations(context_, config_.tlsCaFile.c_str(), nullptr) != 1) {
                    std::cerr << "Failed to load CA certificate: " << config_.tlsCaFile << std::endl;
                    close();
                    return false;
                }
            } else if (!config_.allowInsecureTls && SSL_CTX_set_default_verify_paths(context_) != 1) {
                std::cerr << "Failed to load the system CA trust store" << std::endl;
                close();
                return false;
            }

            SSL_CTX_set_verify(context_,
                config_.allowInsecureTls ? SSL_VERIFY_NONE : SSL_VERIFY_PEER,
                nullptr);

            ssl_ = SSL_new(context_);
            if (ssl_ == nullptr ||
                SSL_set_tlsext_host_name(ssl_, config_.host.c_str()) != 1 ||
                (!config_.allowInsecureTls && SSL_set1_host(ssl_, config_.host.c_str()) != 1)) {
                std::cerr << "Failed to configure TLS peer verification" << std::endl;
                close();
                return false;
            }
            SSL_set_fd(ssl_, socket_);
            if (SSL_connect(ssl_) != 1) {
                std::cerr << "TLS handshake failed" << std::endl;
                close();
                return false;
            }
        }

        authenticated_ = false;
        readBuffer_.clear();
        awaitingPong_ = false;
        authenticationRejected_ = false;
        lastHeartbeatSentAt_ = std::chrono::steady_clock::time_point{};
        return sendJson(remoteclipboard::v1::makeAuth(config_.username, config_.password));
    }

    void close()
    {
        for (auto& [transferId, transfer] : incomingTransfers_) {
            transfer.output.close();
            if (!transfer.targetPath.empty()) {
                std::filesystem::remove(transfer.targetPath);
            }
        }
        incomingTransfers_.clear();

        if (ssl_ != nullptr) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (context_ != nullptr) {
            SSL_CTX_free(context_);
            context_ = nullptr;
        }
        if (socket_ >= 0) {
            ::close(socket_);
            socket_ = -1;
        }
        authenticated_ = false;
        awaitingPong_ = false;
        readBuffer_.clear();
    }

    bool isConnected() const
    {
        return socket_ >= 0;
    }

    bool isAuthenticated() const
    {
        return authenticated_;
    }

    bool authenticationRejected() const
    {
        return authenticationRejected_;
    }

    bool sendJson(const json& message)
    {
        return writeAll(message.dump() + "\n");
    }

    bool sendChunkedFiles(const std::vector<std::filesystem::path>& paths)
    {
        for (const auto& path : paths) {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                std::cerr << "Failed to read file for transfer: " << path << std::endl;
                continue;
            }

            const std::string transferId = std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count())
                + "-" + path.filename().string();

            json start = remoteclipboard::v1::makeTransferStart(transferId,
                path.filename().string(),
                static_cast<std::uint64_t>(std::filesystem::file_size(path)),
                filetransfer::sha256HexForFile(path));
            if (!sendJson(start)) {
                return false;
            }

            std::vector<unsigned char> buffer(filetransfer::kChunkSizeBytes);
            int sequence = 0;
            while (input) {
                input.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
                const auto count = input.gcount();
                if (count <= 0) {
                    break;
                }
                std::vector<unsigned char> chunk(buffer.begin(), buffer.begin() + count);
                json chunkMessage = remoteclipboard::v1::makeTransferChunk(
                    transferId, sequence++, filetransfer::encodeBase64(chunk));
                if (!sendJson(chunkMessage)) {
                    return false;
                }
            }

            if (!sendJson(remoteclipboard::v1::makeTransferComplete(transferId))) {
                return false;
            }
        }

        return true;
    }

    std::vector<json> pollIncomingMessages()
    {
        std::vector<json> messages;
        if (!isConnected()) {
            return messages;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        timeval timeout {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        const int ready = select(socket_ + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready <= 0 || !FD_ISSET(socket_, &readSet)) {
            return messages;
        }

        std::array<char, 8192> buffer {};
        const int bytesRead = ssl_ != nullptr
            ? SSL_read(ssl_, buffer.data(), static_cast<int>(buffer.size()))
            : recv(socket_, buffer.data(), buffer.size(), 0);

        if (bytesRead <= 0) {
            close();
            return messages;
        }

        readBuffer_.append(buffer.data(), static_cast<std::size_t>(bytesRead));
        std::size_t newlinePos = std::string::npos;
        while ((newlinePos = readBuffer_.find('\n')) != std::string::npos) {
            const std::string line = trim(readBuffer_.substr(0, newlinePos));
            readBuffer_.erase(0, newlinePos + 1);
            if (line.empty()) {
                continue;
            }
            try {
                messages.push_back(json::parse(line));
            } catch (const std::exception& error) {
                std::cerr << "Failed to parse server message: " << error.what() << std::endl;
            }
        }

        return messages;
    }

    void processProtocolMessage(const json& message)
    {
        const std::string type = message.value("type", "");
        if (type == "auth_response") {
            authenticated_ = message.value("status", "") == "ok";
            if (!authenticated_) {
                std::cerr << "Authentication failed: " << message.value("message", "unknown error") << std::endl;
                authenticationRejected_ = true;
                close();
            } else {
                std::cout << "Authenticated successfully" << std::endl;
                authenticationRejected_ = false;
            }
            return;
        }

        if (type == "pong") {
            awaitingPong_ = false;
            return;
        }
    }

    bool maybeSendHeartbeat()
    {
        if (!authenticated_) {
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (awaitingPong_ && now - lastHeartbeatSentAt_ > kHeartbeatTimeout) {
            std::cerr << "Heartbeat timeout, reconnecting..." << std::endl;
            close();
            return false;
        }

        if (!awaitingPong_ && now - lastHeartbeatSentAt_ >= kHeartbeatInterval) {
            if (!sendJson(json{{"type", "ping"}})) {
                close();
                return false;
            }
            awaitingPong_ = true;
            lastHeartbeatSentAt_ = now;
        }

        return true;
    }

    void saveLegacyFiles(const json& message)
    {
        if (!message.contains("files") || !message["files"].is_array()) {
            return;
        }

        std::filesystem::create_directories(config_.receiveDir);
        const auto batchDir = std::filesystem::path(config_.receiveDir) /
            (filetransfer::currentTimestamp() + "-"
                + filetransfer::sanitizeFileName(message.value("sender", "peer")));
        std::filesystem::create_directories(batchDir);

        std::size_t savedCount = 0;
        for (const auto& item : message["files"]) {
            std::vector<unsigned char> decoded;
            if (!filetransfer::decodeBase64(item.value("data", ""), decoded)) {
                continue;
            }

            auto targetPath = batchDir / filetransfer::sanitizeFileName(item.value("name", "clipboard-file"));
            if (std::filesystem::exists(targetPath)) {
                targetPath = batchDir / (targetPath.stem().string() + "-" + filetransfer::currentTimestamp() + targetPath.extension().string());
            }

            std::ofstream stream(targetPath, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
            ++savedCount;
        }

        std::cout << "Saved " << savedCount << " file(s) to " << batchDir << std::endl;
    }

    void handleChunkTransfer(const json& message)
    {
        const std::string type = message.value("type", "");
        const std::string transferId = message.value("transfer_id", "");
        if (transferId.empty()) {
            return;
        }

        if (type == "file_transfer_start") {
            std::filesystem::create_directories(config_.receiveDir);
            const auto batchDir = std::filesystem::path(config_.receiveDir) /
                (filetransfer::currentTimestamp() + "-"
                    + filetransfer::sanitizeFileName(message.value("sender", "peer")));
            std::filesystem::create_directories(batchDir);

            IncomingTransfer transfer;
            transfer.transferId = transferId;
            transfer.sender = message.value("sender", "peer");
            transfer.fileName = filetransfer::sanitizeFileName(message.value("name", "clipboard-file"));
            transfer.sha256 = message.value("sha256", "");
            transfer.expectedSize = static_cast<std::size_t>(message.value("size", 0));
            transfer.targetPath = filetransfer::makeFilePathUnique(batchDir / transfer.fileName);
            transfer.output.open(transfer.targetPath, std::ios::binary);
            if (!transfer.output) {
                std::cerr << "Failed to open incoming transfer target: " << transfer.targetPath << std::endl;
                return;
            }
            incomingTransfers_[transferId] = std::move(transfer);
            std::cout << "Receiving file: " << message.value("name", "clipboard-file") << std::endl;
            return;
        }

        auto it = incomingTransfers_.find(transferId);
        if (it == incomingTransfers_.end()) {
            return;
        }

        if (type == "file_transfer_chunk") {
            std::vector<unsigned char> decoded;
            if (!filetransfer::decodeBase64(message.value("data", ""), decoded)) {
                return;
            }
            it->second.output.write(reinterpret_cast<const char*>(decoded.data()),
                static_cast<std::streamsize>(decoded.size()));
            it->second.receivedSize += decoded.size();
            return;
        }

        if (type == "file_transfer_complete") {
            it->second.output.close();

            if (it->second.expectedSize > 0 && it->second.receivedSize != it->second.expectedSize) {
                std::filesystem::remove(it->second.targetPath);
                std::cerr << "Discarded received file because size mismatched: " << it->second.targetPath << std::endl;
                incomingTransfers_.erase(it);
                return;
            }

            if (!it->second.sha256.empty()) {
                const std::string actualHash = filetransfer::sha256HexForFile(it->second.targetPath);
                if (actualHash != it->second.sha256) {
                    std::filesystem::remove(it->second.targetPath);
                    std::cerr << "Discarded received file because hash mismatched: " << it->second.targetPath << std::endl;
                    incomingTransfers_.erase(it);
                    return;
                }
            }

            std::cout << "Saved chunked file to " << it->second.targetPath << std::endl;
            incomingTransfers_.erase(it);
        }
    }

private:
    bool writeAll(const std::string& payload)
    {
        std::size_t offset = 0;
        while (offset < payload.size()) {
            const int sent = ssl_ != nullptr
                ? SSL_write(ssl_, payload.data() + offset, static_cast<int>(payload.size() - offset))
                : send(socket_, payload.data() + offset, payload.size() - offset, 0);

            if (sent <= 0) {
                std::cerr << "Send failed" << std::endl;
                close();
                return false;
            }
            offset += static_cast<std::size_t>(sent);
        }
        return true;
    }

    CliAppConfig config_;
    int socket_ = -1;
    SSL_CTX* context_ = nullptr;
    SSL* ssl_ = nullptr;
    bool authenticated_ = false;
    bool awaitingPong_ = false;
    bool authenticationRejected_ = false;
    std::string readBuffer_;
    std::map<std::string, IncomingTransfer> incomingTransfers_;
    std::chrono::steady_clock::time_point lastHeartbeatSentAt_ {};
};

CliAppConfig prepareConfig(int argc, char* argv[])
{
    CliConfigStore store("linux-cli");
    CliAppConfig config = store.load();
    const CliAppConfig argsConfig = parseArgs(argc, argv);

    config.host = argsConfig.host != "127.0.0.1" ? argsConfig.host : config.host;
    config.port = argsConfig.port != 8080 ? argsConfig.port : config.port;
    if (!argsConfig.username.empty()) {
        config.username = argsConfig.username;
    }
    if (!argsConfig.password.empty()) {
        config.password = argsConfig.password;
        config.passwordFromCommandLine = argsConfig.passwordFromCommandLine;
    }
    if (!argsConfig.passwordSecretFile.empty()) {
        config.passwordSecretFile = argsConfig.passwordSecretFile;
    }
    if (!argsConfig.receiveDir.empty()) {
        config.receiveDir = argsConfig.receiveDir;
    }
    if (argsConfig.developmentMode) {
        config.developmentMode = true;
        config.tlsEnabled = false;
    } else {
        config.tlsEnabled = argsConfig.tlsEnabled || config.tlsEnabled;
    }
    if (!argsConfig.tlsCaFile.empty()) {
        config.tlsCaFile = argsConfig.tlsCaFile;
    }
    if (argsConfig.allowInsecureTls) {
        config.allowInsecureTls = true;
    }

    if (!config.passwordSecretFile.empty()) {
        std::ifstream secretInput(config.passwordSecretFile);
        if (!secretInput || !std::getline(secretInput, config.password)) {
            throw std::runtime_error("Unable to read password secret file");
        }
        config.password = trim(config.password);
    } else if (const char* secret = std::getenv("REMOTE_CLIPBOARD_PASSWORD")) {
        config.password = secret;
    }
    if (!config.developmentMode && !config.tlsEnabled) {
        throw std::runtime_error("Plaintext transport requires --development");
    }
    if (config.allowInsecureTls && !config.developmentMode) {
        throw std::runtime_error("Ignoring TLS certificate errors requires --development");
    }
    if (config.passwordFromCommandLine && !config.developmentMode) {
        throw std::runtime_error(
            "--password is only accepted with --development; use --password-file in production");
    }
    if (config.developmentMode) {
        if (config.username.empty()) {
            config.username = "admin";
        }
        if (config.password.empty()) {
            config.password = "admin";
        }
    }
    if (config.username.empty() || config.password.empty()) {
        throw std::runtime_error("Username and password secret are required");
    }

    while (config.receiveDir.empty()) {
        std::cout << "Enter the default receive directory for incoming files: ";
        std::getline(std::cin, config.receiveDir);
        config.receiveDir = trim(config.receiveDir);
    }

    std::string resolvedDir;
    while (!ensureDirectoryInteractive(config.receiveDir, true, resolvedDir)) {
        std::cout << "Please enter another directory path: ";
        std::getline(std::cin, config.receiveDir);
        config.receiveDir = trim(config.receiveDir);
    }
    config.receiveDir = resolvedDir;

    std::string errorMessage;
    if (!store.save(config, &errorMessage)) {
        std::cerr << "Failed to save CLI config: " << errorMessage << std::endl;
    } else {
        std::cout << "CLI config saved to " << store.configPath() << std::endl;
    }

    return config;
}
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    CliAppConfig config;
    try {
        config = prepareConfig(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Configuration error: " << error.what() << std::endl;
        return 1;
    }
    std::filesystem::create_directories(config.receiveDir);

    ClientConnection connection(config);
    ClipboardState clipboardState;

    int reconnectAttempt = 0;
    auto nextReconnectAt = std::chrono::steady_clock::now();
    auto nextClipboardPollAt = std::chrono::steady_clock::now();

    while (g_running) {
        const auto now = std::chrono::steady_clock::now();

        if (!connection.isConnected() && now >= nextReconnectAt) {
            std::cout << "Connecting to " << config.host << ":" << config.port
                      << (config.tlsEnabled ? " with TLS" : " without TLS") << std::endl;
            if (connection.connectToServer()) {
                reconnectAttempt = 0;
            } else {
                const int delay = std::min(30, 1 << std::min(reconnectAttempt, 5));
                ++reconnectAttempt;
                nextReconnectAt = now + std::chrono::seconds(delay);
            }
        }

        for (const auto& message : connection.pollIncomingMessages()) {
            connection.processProtocolMessage(message);

            const std::string type = message.value("type", "");
            if (type == "clipboard_text") {
                const std::string content = message.value("content", "");
                writeClipboardText(content);
                clipboardState.signature = "text:" + content;
                std::cout << "Updated local clipboard text" << std::endl;
            } else if (type == "file_bundle") {
                connection.saveLegacyFiles(message);
            } else if (type == "file_transfer_start" ||
                       type == "file_transfer_chunk" ||
                       type == "file_transfer_complete") {
                connection.handleChunkTransfer(message);
            }
        }

        if (connection.authenticationRejected()) {
            return 1;
        }

        if (connection.isConnected() && connection.isAuthenticated() && now >= nextClipboardPollAt) {
            nextClipboardPollAt = now + kClipboardPollInterval;
            const auto clipboardMessage = pollClipboard(clipboardState);
            if (clipboardMessage.has_value()) {
                if (clipboardMessage->chunked) {
                    connection.sendChunkedFiles(clipboardMessage->paths);
                } else {
                    connection.sendJson(clipboardMessage->message);
                }
            }
        }

        if (connection.isConnected() && !connection.maybeSendHeartbeat()) {
            const int delay = std::min(30, 1 << std::min(reconnectAttempt, 5));
            ++reconnectAttempt;
            nextReconnectAt = std::chrono::steady_clock::now() + std::chrono::seconds(delay);
        }

        if (connection.isConnected() && !connection.isAuthenticated()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return 0;
}
