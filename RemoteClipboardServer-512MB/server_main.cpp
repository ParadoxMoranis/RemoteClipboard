#include "../server_common/tcpserver.h"

#include <csignal>
#include <iostream>
#include <string>
#include <thread>

namespace {
volatile sig_atomic_t g_running = 1;

void signalHandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        g_running = 0;
    }
}

struct ServerConfig {
    uint16_t port = 8080;
    std::string username = "admin";
    std::string password = "admin";
    std::string storageDir = "received_files";
    int retentionDays = 7;
    bool tlsEnabled = false;
    std::string tlsCertificateFile;
    std::string tlsKeyFile;
};

void printHelp()
{
    std::cout
        << "RemoteClipboardServer (512MB buffer)\n"
        << "Usage: RemoteClipboardServer [options]\n\n"
        << "Options:\n"
        << "  -h, --help                  Show this help message\n"
        << "  -p, --port <port>           Port to listen on (default: 8080)\n"
        << "  -u, --username <username>   Username for authentication (default: admin)\n"
        << "  -w, --password <password>   Password for authentication (default: admin)\n"
        << "  -d, --storage-dir <path>    Directory for received files (default: received_files)\n"
        << "  -r, --retention-days <n>    Retain server-side files for n days (default: 7)\n"
        << "      --tls                   Enable TLS for client connections\n"
        << "      --tls-cert <path>       PEM certificate file for TLS mode\n"
        << "      --tls-key <path>        PEM private key file for TLS mode\n";
}

ServerConfig parseCommandLine(int argc, char* argv[])
{
    ServerConfig config;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            std::exit(0);
        }
        if ((arg == "-p" || arg == "--port") && index + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++index]));
            continue;
        }
        if ((arg == "-u" || arg == "--username") && index + 1 < argc) {
            config.username = argv[++index];
            continue;
        }
        if ((arg == "-w" || arg == "--password") && index + 1 < argc) {
            config.password = argv[++index];
            continue;
        }
        if ((arg == "-d" || arg == "--storage-dir") && index + 1 < argc) {
            config.storageDir = argv[++index];
            continue;
        }
        if ((arg == "-r" || arg == "--retention-days") && index + 1 < argc) {
            config.retentionDays = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--tls") {
            config.tlsEnabled = true;
            continue;
        }
        if (arg == "--tls-cert" && index + 1 < argc) {
            config.tlsCertificateFile = argv[++index];
            continue;
        }
        if (arg == "--tls-key" && index + 1 < argc) {
            config.tlsKeyFile = argv[++index];
            continue;
        }
    }

    return config;
}
}

int main(int argc, char* argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const auto config = parseCommandLine(argc, argv);

    try {
        TcpServer server(512ull * 1024ull * 1024ull);
        server.setCredentials(config.username, config.password);
        server.configureStorage(config.storageDir, config.retentionDays);
        server.configureTls(config.tlsEnabled, config.tlsCertificateFile, config.tlsKeyFile);

        if (!server.startServer(config.port)) {
            std::cerr << "Failed to start server on port " << config.port << std::endl;
            return 1;
        }

        std::cout << "Username: " << config.username << std::endl;
        std::cout << "Password: " << config.password << std::endl;
        std::cout << "Storage directory: " << config.storageDir << std::endl;
        std::cout << "Retention days: " << config.retentionDays << std::endl;
        std::cout << "TLS: " << (config.tlsEnabled ? "enabled" : "disabled") << std::endl;

        while (g_running) {
            server.processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        server.stopServer();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal server error: " << error.what() << std::endl;
        return 1;
    }
}
