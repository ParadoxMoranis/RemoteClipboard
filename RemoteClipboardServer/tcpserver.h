#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <chrono>

// 使用 nlohmann::json
using json = nlohmann::json;

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int SocketType;
    #define INVALID_SOCKET_VALUE (-1)
#endif

class Client {
public:
    SocketType socket;
    std::string buffer;
    bool authenticated = false;  // 确保初始化为 false
    
    Client() : socket(INVALID_SOCKET_VALUE), authenticated(false) {}
};

class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    void setCredentials(const std::string& username, const std::string& password);
    bool startServer(uint16_t port);
    void stopServer();
    void processEvents();

private:
    void handleNewConnection();
    void handleClientData(SocketType clientSocket);
    void removeClient(SocketType clientSocket);
    bool authenticateClient(Client& client, const json& jsonData);
    void broadcastClipboardData(const std::string& data, SocketType excludeSocket);
    void handleClientThread(SocketType clientSocket);

    SocketType serverSocket;
    std::map<SocketType, std::unique_ptr<Client>> clients;
    std::mutex clientsMutex;
    std::string username;
    std::string password;
    bool running;
}; 