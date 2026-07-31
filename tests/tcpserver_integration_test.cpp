#include "test_support.h"

#include "client_common/protocol.h"
#include "server_common/filetransfer.h"
#include "server_common/tcpserver.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <sqlite3.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;
#endif

namespace {
using Json = nlohmann::json;
using namespace std::chrono_literals;

void closeTestSocket(TestSocket socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

std::filesystem::path uniqueTemporaryDirectory()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("remoteclipboard-server-test-" + std::to_string(suffix));
}

TestSocket connectClient(std::uint16_t port)
{
    const TestSocket socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == kInvalidTestSocket) {
        return socket;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closeTestSocket(socket);
        return kInvalidTestSocket;
    }
    return socket;
}

bool sendAll(TestSocket socket, const std::string& payload)
{
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const int sent = static_cast<int>(
            ::send(socket, payload.data() + offset, static_cast<int>(payload.size() - offset), 0));
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

bool sendJson(TestSocket socket, const Json& message)
{
    return sendAll(socket, message.dump() + "\n");
}

std::optional<std::string> receiveLine(TestSocket socket, std::chrono::milliseconds timeout)
{
    std::string line;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - std::chrono::steady_clock::now());
        timeval wait{};
        wait.tv_sec = static_cast<long>(remaining.count() / 1000000);
        wait.tv_usec = static_cast<long>(remaining.count() % 1000000);
#ifdef _WIN32
        const int ready = select(0, &readSet, nullptr, nullptr, &wait);
#else
        const int ready = select(socket + 1, &readSet, nullptr, nullptr, &wait);
#endif
        if (ready <= 0) {
            return std::nullopt;
        }
        char value = '\0';
        const int count = static_cast<int>(recv(socket, &value, 1, 0));
        if (count <= 0) {
            return std::nullopt;
        }
        if (value == '\n') {
            return line;
        }
        line.push_back(value);
    }
    return std::nullopt;
}

std::optional<Json> receiveJson(TestSocket socket, std::chrono::milliseconds timeout = 2s)
{
    const auto line = receiveLine(socket, timeout);
    if (!line) {
        return std::nullopt;
    }
    try {
        return Json::parse(*line);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool authenticate(TestSocket socket)
{
    return sendJson(socket, remoteclipboard::v1::makeAuth("test-user", "test-secret")) &&
           receiveJson(socket).value_or(Json::object()).value("status", "") == "ok";
}

bool writeTestCertificate(const std::filesystem::path& certificatePath,
                          const std::filesystem::path& keyPath)
{
    EVP_PKEY_CTX* keyContext = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY* key = nullptr;
    if (keyContext == nullptr || EVP_PKEY_keygen_init(keyContext) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext, 2048) <= 0 ||
        EVP_PKEY_keygen(keyContext, &key) <= 0) {
        EVP_PKEY_CTX_free(keyContext);
        EVP_PKEY_free(key);
        return false;
    }
    EVP_PKEY_CTX_free(keyContext);

    X509* certificate = X509_new();
    bool valid = certificate != nullptr && X509_set_version(certificate, 2) == 1 &&
                 ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1) == 1 &&
                 X509_gmtime_adj(X509_get_notBefore(certificate), 0) != nullptr &&
                 X509_gmtime_adj(X509_get_notAfter(certificate), 3600) != nullptr &&
                 X509_set_pubkey(certificate, key) == 1;
    if (valid) {
        X509_NAME* name = X509_get_subject_name(certificate);
        valid = name != nullptr &&
                X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                           reinterpret_cast<const unsigned char*>("localhost"), -1,
                                           -1, 0) == 1 &&
                X509_set_issuer_name(certificate, name) == 1 &&
                X509_sign(certificate, key, EVP_sha256()) > 0;
    }

    FILE* keyFile = valid ? std::fopen(keyPath.string().c_str(), "wb") : nullptr;
    FILE* certificateFile = valid ? std::fopen(certificatePath.string().c_str(), "wb") : nullptr;
    valid = keyFile != nullptr && certificateFile != nullptr &&
            PEM_write_PrivateKey(keyFile, key, nullptr, nullptr, 0, nullptr, nullptr) == 1 &&
            PEM_write_X509(certificateFile, certificate) == 1;
    if (keyFile != nullptr) {
        std::fclose(keyFile);
    }
    if (certificateFile != nullptr) {
        std::fclose(certificateFile);
    }
    X509_free(certificate);
    EVP_PKEY_free(key);
    return valid;
}

bool authenticateTls(TestSocket socket, SSL*& ssl, SSL_CTX*& context)
{
    context = SSL_CTX_new(TLS_client_method());
    if (context == nullptr || SSL_CTX_set_min_proto_version(context, TLS1_3_VERSION) != 1) {
        return false;
    }
    SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
    ssl = SSL_new(context);
    if (ssl == nullptr || SSL_set_fd(ssl, static_cast<int>(socket)) != 1 || SSL_connect(ssl) != 1) {
        return false;
    }
    const std::string authentication =
        remoteclipboard::v1::makeAuth("test-user", "test-secret").dump() + "\n";
    if (SSL_write(ssl, authentication.data(), static_cast<int>(authentication.size())) <= 0) {
        return false;
    }
    std::string line;
    while (line.size() < 4096) {
        char value = '\0';
        if (SSL_read(ssl, &value, 1) <= 0) {
            return false;
        }
        if (value == '\n') {
            break;
        }
        line.push_back(value);
    }
    try {
        return Json::parse(line).value("status", "") == "ok";
    } catch (const std::exception&) {
        return false;
    }
}

int eventCount(const std::filesystem::path& databasePath)
{
    sqlite3* database = nullptr;
    if (sqlite3_open(databasePath.string().c_str(), &database) != SQLITE_OK) {
        return -1;
    }
    sqlite3_stmt* statement = nullptr;
    int count = -1;
    if (sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM clipboard_events;", -1, &statement,
                           nullptr) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_ROW) {
        count = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return count;
}
} // namespace

int main()
{
    TestContext test;
    const auto directory = uniqueTemporaryDirectory();
    const auto storageDirectory = directory / "files";
    const auto databasePath = directory / "events.sqlite3";

    TcpServer server(1024 * 1024);
    server.setCredentials("test-user", "test-secret");
    server.configureStorage(storageDirectory.string(), 7);
    server.configureDatabase(databasePath.string());
    const bool started = server.startServer(0);
    RC_EXPECT(test, started);
    if (!started) {
        std::filesystem::remove_all(directory);
        return test.result();
    }
    RC_EXPECT(test, server.boundPort() != 0);

    TestSocket first = connectClient(server.boundPort());
    TestSocket second = connectClient(server.boundPort());
    RC_EXPECT(test, first != kInvalidTestSocket);
    RC_EXPECT(test, second != kInvalidTestSocket);
    RC_EXPECT(test, authenticate(first));
    RC_EXPECT(test, authenticate(second));

    Json text = remoteclipboard::v1::makeClipboardText("integration hello");
    text["event_id"] = "event'; SELECT 1; --";
    text["device_id"] = "device'; DROP TABLE clipboard_events; --";
    RC_EXPECT(test, sendJson(first, text));
    const auto deliveredText = receiveJson(second);
    RC_EXPECT(test, deliveredText.has_value());
    RC_EXPECT_EQ(test, deliveredText.value_or(Json::object()).value("content", ""),
                 "integration hello");

    const Json invalidBundle = {
        {"type", remoteclipboard::v1::type::kFileBundle},
        {"files", Json::array({Json{{"name", "bad.txt"}, {"data", "%%=="}}})},
    };
    RC_EXPECT(test, sendJson(first, invalidBundle));
    RC_EXPECT_EQ(test, receiveJson(first).value_or(Json::object()).value("code", ""),
                 "INVALID_FILE");
    RC_EXPECT(test, !receiveJson(second, 200ms).has_value());

    const Json checksumMismatchBundle = {
        {"type", remoteclipboard::v1::type::kFileBundle},
        {"files", Json::array({Json{{"name", "mismatch.txt"},
                                    {"size", 3},
                                    {"sha256", std::string(64, '0')},
                                    {"data", "YWJj"}}})},
    };
    RC_EXPECT(test, sendJson(first, checksumMismatchBundle));
    RC_EXPECT_EQ(test, receiveJson(first).value_or(Json::object()).value("code", ""),
                 "CHECKSUM_MISMATCH");
    RC_EXPECT(test, !receiveJson(second, 200ms).has_value());

    const std::vector<unsigned char> abc = {'a', 'b', 'c'};
    const std::string abcHash = filetransfer::sha256Hex(abc);
    RC_EXPECT(test, sendJson(first, remoteclipboard::v1::makeTransferStart(
                                        "transfer-ok", "../safe.txt", 3, abcHash)));
    RC_EXPECT_EQ(test, receiveJson(second).value_or(Json::object()).value("type", ""),
                 remoteclipboard::v1::type::kTransferStart);

    RC_EXPECT(test,
              sendJson(first, remoteclipboard::v1::makeTransferChunk("transfer-ok", 1, "YWJj")));
    RC_EXPECT_EQ(test, receiveJson(first).value_or(Json::object()).value("code", ""),
                 "INVALID_SEQUENCE");
    RC_EXPECT(test, !receiveJson(second, 200ms).has_value());

    RC_EXPECT(test,
              sendJson(first, remoteclipboard::v1::makeTransferChunk("transfer-ok", 0, "YWJj")));
    RC_EXPECT_EQ(test, receiveJson(second).value_or(Json::object()).value("type", ""),
                 remoteclipboard::v1::type::kTransferChunk);
    RC_EXPECT(test,
              sendJson(first, remoteclipboard::v1::makeTransferChunk("transfer-ok", 0, "YWJj")));
    RC_EXPECT_EQ(test, receiveJson(first).value_or(Json::object()).value("code", ""),
                 "INVALID_SEQUENCE");
    RC_EXPECT(test, !receiveJson(second, 200ms).has_value());

    RC_EXPECT(test, sendJson(first, remoteclipboard::v1::makeTransferComplete("transfer-ok")));
    RC_EXPECT_EQ(test, receiveJson(second).value_or(Json::object()).value("type", ""),
                 remoteclipboard::v1::type::kTransferComplete);

    bool foundCommittedFile = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(storageDirectory)) {
        if (entry.is_regular_file() && entry.path().filename() == "safe.txt") {
            foundCommittedFile = true;
        }
    }
    RC_EXPECT(test, foundCommittedFile);

    TestSocket oversizedWithNewline = connectClient(server.boundPort());
    RC_EXPECT(test, authenticate(oversizedWithNewline));
    RC_EXPECT(test, sendAll(oversizedWithNewline, std::string(1024 * 1024 + 1, 'x') + "\n"));
    const auto oversizedResponse = receiveJson(oversizedWithNewline, 2s);
    RC_EXPECT(test, !oversizedResponse.has_value() ||
                        oversizedResponse->value("code", "") == "FRAME_TOO_LARGE");
    closeTestSocket(oversizedWithNewline);

    TestSocket oversizedWithoutNewline = connectClient(server.boundPort());
    RC_EXPECT(test, authenticate(oversizedWithoutNewline));
    RC_EXPECT(test, sendAll(oversizedWithoutNewline, std::string(1024 * 1024 + 1, 'y')));
    const auto noNewlineResponse = receiveJson(oversizedWithoutNewline, 2s);
    RC_EXPECT(test, !noNewlineResponse.has_value() ||
                        noNewlineResponse->value("code", "") == "FRAME_TOO_LARGE");
    closeTestSocket(oversizedWithoutNewline);

    closeTestSocket(first);
    closeTestSocket(second);
    const auto stopStarted = std::chrono::steady_clock::now();
    server.stopServer();
    RC_EXPECT(test, std::chrono::steady_clock::now() - stopStarted < 3s);
    RC_EXPECT_EQ(test, server.activeSessionCount(), 0U);
    RC_EXPECT(test, eventCount(databasePath) >= 4);

    for (int attempt = 0; attempt < 3; ++attempt) {
        RC_EXPECT(test, server.startServer(0));
        server.stopServer();
    }

    const auto certificatePath = directory / "test-certificate.pem";
    const auto keyPath = directory / "test-key.pem";
    RC_EXPECT(test, writeTestCertificate(certificatePath, keyPath));
    TcpServer tlsServer(64 * 1024);
    tlsServer.setCredentials("test-user", "test-secret");
    tlsServer.configureStorage((directory / "tls-files").string(), 7);
    tlsServer.configureTls(true, certificatePath.string(), keyPath.string());
    RC_EXPECT(test, tlsServer.startServer(0));

    TestSocket slowHandshake = connectClient(tlsServer.boundPort());
    RC_EXPECT(test, slowHandshake != kInvalidTestSocket);
    std::this_thread::sleep_for(50ms);
    TestSocket tlsSocket = connectClient(tlsServer.boundPort());
    SSL* ssl = nullptr;
    SSL_CTX* sslContext = nullptr;
    RC_EXPECT(test, tlsSocket != kInvalidTestSocket);
    RC_EXPECT(test, authenticateTls(tlsSocket, ssl, sslContext));
    RC_EXPECT(test, tlsServer.activeSessionCount() >= 2);
    if (ssl != nullptr) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (sslContext != nullptr) {
        SSL_CTX_free(sslContext);
    }
    closeTestSocket(tlsSocket);
    closeTestSocket(slowHandshake);
    const auto tlsStopStarted = std::chrono::steady_clock::now();
    tlsServer.stopServer();
    RC_EXPECT(test, std::chrono::steady_clock::now() - tlsStopStarted < 3s);

    std::filesystem::remove_all(directory);
    return test.result();
}
