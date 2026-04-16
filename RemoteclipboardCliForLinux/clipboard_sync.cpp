#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
constexpr std::size_t kMaxFileBundleBytes = 32ull * 1024ull * 1024ull;
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

struct AppConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string username = "admin";
    std::string password = "admin";
    std::string receiveDir = "received_files";
    bool tlsEnabled = false;
    std::string tlsCaFile;
    bool allowInsecureTls = true;
};

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

std::string sanitizeFileName(const std::string& fileName)
{
    std::string sanitized = fileName.empty() ? "clipboard-file" : fileName;
    for (char& ch : sanitized) {
        if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' ||
            ch == '"' || ch == '<' || ch == '>' || ch == '|') {
            ch = '_';
        }
    }
    return sanitized;
}

std::string currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
    localtime_r(&raw, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return stream.str();
}

bool decodeBase64(const std::string& input, std::vector<unsigned char>& output)
{
    std::string compact;
    compact.reserve(input.size());
    for (const char ch : input) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            compact.push_back(ch);
        }
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

std::string encodeBase64(const std::vector<unsigned char>& input)
{
    if (input.empty()) {
        return {};
    }

    std::string output(((input.size() + 2) / 3) * 4, '\0');
    const int encodedLength = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(output.data()),
        input.data(),
        static_cast<int>(input.size()));
    output.resize(static_cast<std::size_t>(encodedLength));
    return output;
}

std::string sha256Hex(const std::vector<unsigned char>& data)
{
    unsigned char digest[SHA256_DIGEST_LENGTH] = {0};
    SHA256(data.data(), data.size(), digest);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const unsigned char byte : digest) {
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

void printHelp()
{
    std::cout
        << "clipboard_sync\n"
        << "Usage: clipboard_sync [options]\n\n"
        << "Options:\n"
        << "  --host <host>             Server address (default: 127.0.0.1)\n"
        << "  --port <port>             Server port (default: 8080)\n"
        << "  --username <name>         Authentication username (default: admin)\n"
        << "  --password <password>     Authentication password (default: admin)\n"
        << "  --receive-dir <path>      Directory for received files\n"
        << "  --tls                     Enable TLS\n"
        << "  --tls-ca <path>           Optional CA certificate for TLS\n"
        << "  --strict-tls              Verify the peer certificate instead of allowing self-signed certs\n"
        << "  -h, --help                Show this help message\n";
}

AppConfig parseArgs(int argc, char* argv[])
{
    AppConfig config;

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
            config.port = static_cast<uint16_t>(std::stoi(argv[++index]));
            continue;
        }
        if (arg == "--username" && index + 1 < argc) {
            config.username = argv[++index];
            continue;
        }
        if (arg == "--password" && index + 1 < argc) {
            config.password = argv[++index];
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
        if (arg == "--strict-tls") {
            config.allowInsecureTls = false;
            continue;
        }
    }

    return config;
}

struct ClipboardState {
    std::string signature;
};

std::optional<json> pollClipboard(ClipboardState& state)
{
    const std::string mimeTypes = runCommand("wl-paste --list-types 2>/dev/null");
    if (mimeTypes.find("text/uri-list") != std::string::npos) {
        const std::string uriPayload = runCommand("wl-paste --type text/uri-list --no-newline 2>/dev/null");
        const auto uriLines = splitLines(uriPayload);
        std::vector<std::string> paths;
        std::size_t totalBytes = 0;
        json files = json::array();

        for (const auto& uri : uriLines) {
            const std::string filePath = localPathFromUri(uri);
            if (filePath.empty() || !std::filesystem::exists(filePath)) {
                continue;
            }

            std::ifstream stream(filePath, std::ios::binary);
            if (!stream) {
                continue;
            }

            std::vector<unsigned char> data(
                (std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());

            totalBytes += data.size();
            if (totalBytes > kMaxFileBundleBytes) {
                std::cerr << "Clipboard file bundle exceeds 32MB limit, skipping transfer" << std::endl;
                return std::nullopt;
            }

            json fileObject;
            fileObject["name"] = std::filesystem::path(filePath).filename().string();
            fileObject["size"] = data.size();
            fileObject["sha256"] = sha256Hex(data);
            fileObject["data"] = encodeBase64(data);
            files.push_back(fileObject);
            paths.push_back(filePath);
        }

        if (!paths.empty()) {
            std::ostringstream signature;
            signature << "files:";
            for (const auto& path : paths) {
                signature << path << "|";
            }

            if (signature.str() != state.signature) {
                state.signature = signature.str();
                return json{
                    {"type", "file_bundle"},
                    {"files", files}
                };
            }
            return std::nullopt;
        }
    }

    const std::string text = runCommand("wl-paste 2>/dev/null");
    const std::string signature = "text:" + text;
    if (signature != state.signature) {
        state.signature = signature;
        return json{
            {"type", "clipboard_text"},
            {"content", text}
        };
    }

    return std::nullopt;
}

class ClientConnection {
public:
    explicit ClientConnection(AppConfig config)
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

            if (!config_.tlsCaFile.empty()) {
                if (SSL_CTX_load_verify_locations(context_, config_.tlsCaFile.c_str(), nullptr) != 1) {
                    std::cerr << "Failed to load CA certificate: " << config_.tlsCaFile << std::endl;
                    close();
                    return false;
                }
            }

            if (config_.allowInsecureTls) {
                SSL_CTX_set_verify(context_, SSL_VERIFY_NONE, nullptr);
            } else {
                SSL_CTX_set_verify(context_, SSL_VERIFY_PEER, nullptr);
            }

            ssl_ = SSL_new(context_);
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
        return sendJson(json{
            {"type", "auth"},
            {"username", config_.username},
            {"password", config_.password}
        });
    }

    void close()
    {
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

    AppConfig config_;
    int socket_ = -1;
    SSL_CTX* context_ = nullptr;
    SSL* ssl_ = nullptr;
    bool authenticated_ = false;
    bool awaitingPong_ = false;
    bool authenticationRejected_ = false;
    std::string readBuffer_;
    std::chrono::steady_clock::time_point lastHeartbeatSentAt_ {};
};

void saveReceivedFiles(const AppConfig& config, const json& message)
{
    if (!message.contains("files") || !message["files"].is_array()) {
        return;
    }

    std::filesystem::create_directories(config.receiveDir);
    const auto batchDir = std::filesystem::path(config.receiveDir) /
        (currentTimestamp() + "-" + sanitizeFileName(message.value("sender", "peer")));
    std::filesystem::create_directories(batchDir);

    std::size_t savedCount = 0;
    for (const auto& item : message["files"]) {
        std::vector<unsigned char> decoded;
        if (!decodeBase64(item.value("data", ""), decoded)) {
            continue;
        }

        auto targetPath = batchDir / sanitizeFileName(item.value("name", "clipboard-file"));
        if (std::filesystem::exists(targetPath)) {
            targetPath = batchDir / (targetPath.stem().string() + "-" + currentTimestamp() + targetPath.extension().string());
        }

        std::ofstream stream(targetPath, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
        ++savedCount;
    }

    std::cout << "Saved " << savedCount << " file(s) to " << batchDir << std::endl;
}
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    AppConfig config = parseArgs(argc, argv);
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
                saveReceivedFiles(config, message);
            }
        }

        if (connection.authenticationRejected()) {
            return 1;
        }

        if (connection.isConnected() && connection.isAuthenticated() && now >= nextClipboardPollAt) {
            nextClipboardPollAt = now + kClipboardPollInterval;
            const auto clipboardMessage = pollClipboard(clipboardState);
            if (clipboardMessage.has_value()) {
                connection.sendJson(*clipboardMessage);
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
