#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include "tcpserver.h"

// 全局变量用于处理信号
volatile sig_atomic_t g_running = 1;

// 信号处理函数
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\nShutdown signal received. Stopping server...\n";
        g_running = 0;
    }
}

void printHelp() {
    std::cout << "RemoteClipboardServer v1.0\n"
              << "Usage: RemoteClipboardServer [options]\n\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -p, --port <port>          Port to listen on (default: 8080)\n"
              << "  -u, --username <username>  Username for authentication (default: admin)\n"
              << "  -w, --password <password>  Password for authentication (default: admin)\n";
}

struct ServerConfig {
    uint16_t port = 8080;
    std::string username = "admin";
    std::string password = "admin";
};

ServerConfig parseCommandLine(int argc, char* argv[]) {
    ServerConfig config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printHelp();
            exit(0);
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) {
                config.username = argv[++i];
            }
        } else if (arg == "-w" || arg == "--password") {
            if (i + 1 < argc) {
                config.password = argv[++i];
            }
        }
    }
    
    return config;
}

int main(int argc, char *argv[])
{
    // 设置信号处理
    signal(SIGINT, signalHandler);  // 处理 Ctrl+C
    signal(SIGTERM, signalHandler); // 处理终止信号

    auto config = parseCommandLine(argc, argv);

    TcpServer server;
    server.setCredentials(config.username, config.password);
    
    if (!server.startServer(config.port)) {
        std::cerr << "Failed to start server on port " << config.port << std::endl;
        return 1;
    }

    std::cout << "Server started on port " << config.port << std::endl;
    std::cout << "Username: " << config.username << std::endl;
    std::cout << "Password: " << config.password << std::endl;
    std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;

    // 持续运行，直到收到终止信号
    while (g_running) {
        server.processEvents(); // 这个方法需要在 TcpServer 类中实现
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 避免CPU占用过高
    }

    std::cout << "Server stopped." << std::endl;
    return 0;
} 