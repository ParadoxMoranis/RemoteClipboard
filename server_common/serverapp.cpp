#include "serverapp.h"

#include "parseutils.h"
#include "serverconfig.h"
#include "tcpserver.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile sig_atomic_t gRunning = 1;

void signalHandler(int signalNumber)
{
    if (signalNumber == SIGINT || signalNumber == SIGTERM) {
        gRunning = 0;
    }
}

std::string readSecretFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to read password secret file");
    }
    std::string secret;
    std::getline(input, secret);
    while (!secret.empty() && (secret.back() == '\r' || secret.back() == '\n')) {
        secret.pop_back();
    }
    return secret;
}

void applyEnvironment(ServerAppConfig& config)
{
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_PORT")) {
        config.port = parsePortArg(value, "REMOTE_CLIPBOARD_PORT");
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_USERNAME")) {
        config.username = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_PASSWORD")) {
        config.password = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_PASSWORD_FILE")) {
        config.passwordSecretFile = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_STORAGE_DIR")) {
        config.storageDir = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_SQLITE")) {
        config.sqlitePath = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_TLS_CERT")) {
        config.tlsCertificateFile = value;
    }
    if (const char* value = std::getenv("REMOTE_CLIPBOARD_TLS_KEY")) {
        config.tlsKeyFile = value;
    }
}

void printHelp(const std::string& displayName)
{
    std::cout
        << displayName << "\nUsage: " << displayName << " [options]\n\n"
        << "  -p, --port <port>           Listening port\n"
        << "  -u, --username <name>       Authentication username\n"
        << "      --password-file <path>  File containing the authentication secret\n"
        << "  -d, --storage-dir <path>    Received-file directory\n"
        << "      --sqlite <path>         SQLite event metadata database\n"
        << "  -r, --retention-days <n>    Retention days; -1 disables expiry\n"
        << "      --tls-cert <path>       PEM TLS certificate\n"
        << "      --tls-key <path>        PEM TLS private key\n"
        << "      --development           Explicitly allow plaintext and development defaults\n"
        << "      --no-tls                Disable TLS; requires --development\n"
        << "      --config-name <name>    Configuration namespace\n"
        << "  -h, --help                  Show help\n";
}

std::string preliminaryConfigName(int argc, char* argv[], const std::string& defaultName)
{
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string(argv[index]) == "--config-name") {
            return argv[index + 1];
        }
    }
    return defaultName;
}

ServerAppConfig parseCommandLine(int argc, char* argv[], const std::string& displayName,
                                 ServerAppConfig config)
{
    bool passwordOnCommandLine = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto nextValue = [&](const char* option) -> std::string {
            if (index + 1 >= argc) {
                throw std::invalid_argument(std::string("Missing value for ") + option);
            }
            return argv[++index];
        };
        if (argument == "-h" || argument == "--help") {
            printHelp(displayName);
            std::exit(0);
        } else if (argument == "-p" || argument == "--port") {
            config.port = parsePortArg(nextValue("--port"), "--port");
        } else if (argument == "-u" || argument == "--username") {
            config.username = nextValue("--username");
        } else if (argument == "-w" || argument == "--password") {
            config.password = nextValue("--password");
            passwordOnCommandLine = true;
        } else if (argument == "--password-file") {
            config.passwordSecretFile = nextValue("--password-file");
        } else if (argument == "-d" || argument == "--storage-dir") {
            config.storageDir = nextValue("--storage-dir");
        } else if (argument == "--sqlite") {
            config.sqlitePath = nextValue("--sqlite");
        } else if (argument == "-r" || argument == "--retention-days") {
            config.retentionDays = parseIntArg(nextValue("--retention-days"), "--retention-days");
        } else if (argument == "--tls") {
            config.tlsEnabled = true;
        } else if (argument == "--no-tls") {
            config.tlsEnabled = false;
        } else if (argument == "--tls-cert") {
            config.tlsCertificateFile = nextValue("--tls-cert");
        } else if (argument == "--tls-key") {
            config.tlsKeyFile = nextValue("--tls-key");
        } else if (argument == "--development") {
            config.developmentMode = true;
        } else if (argument == "--config-name") {
            nextValue("--config-name");
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }

    if (passwordOnCommandLine && !config.developmentMode) {
        throw std::invalid_argument("--password is only accepted with --development; use a secret "
                                    "file or environment variable");
    }
    return config;
}

void validateConfiguration(ServerAppConfig& config)
{
    if (!config.passwordSecretFile.empty()) {
        config.password = readSecretFile(config.passwordSecretFile);
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
        throw std::invalid_argument("Username and password secret are required");
    }
    if (!config.developmentMode && !config.tlsEnabled) {
        throw std::invalid_argument(
            "Production mode requires TLS; use --development for an explicit plaintext profile");
    }
    if (config.tlsEnabled && (config.tlsCertificateFile.empty() || config.tlsKeyFile.empty())) {
        throw std::invalid_argument("TLS certificate and key are required");
    }
}
} // namespace

int runServerApplication(int argc, char* argv[], const std::string& configName,
                         const std::string& displayName, std::size_t maxMessageBytes)
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    try {
        const std::string selectedConfigName = preliminaryConfigName(argc, argv, configName);
        ServerConfigStore store(selectedConfigName);
        ServerAppConfig config = store.load();
        applyEnvironment(config);
        config = parseCommandLine(argc, argv, displayName, std::move(config));
        validateConfiguration(config);

        std::string saveError;
        if (!store.save(config, &saveError)) {
            throw std::runtime_error("Unable to save configuration: " + saveError);
        }

        TcpServer server(maxMessageBytes);
        server.setCredentials(config.username, config.password);
        server.configureStorage(config.storageDir, config.retentionDays);
        server.configureDatabase(config.sqlitePath);
        server.configureTls(config.tlsEnabled, config.tlsCertificateFile, config.tlsKeyFile);
        if (!server.startServer(config.port)) {
            return 1;
        }

        std::cout << "Storage directory: " << config.storageDir << '\n'
                  << "SQLite metadata: " << config.sqlitePath << '\n'
                  << "TLS: " << (config.tlsEnabled ? "required" : "development plaintext") << '\n'
                  << "Config file: " << store.configPath() << '\n';
        while (gRunning) {
            server.processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        server.stopServer();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal server error: " << error.what() << '\n';
        return 1;
    }
}
