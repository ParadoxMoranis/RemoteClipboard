#include "tcpserver.h"
#include <iostream>
#include <cstring>
#include <nlohmann/json.hpp>  // 添加这个头文件
using json = nlohmann::json; // 使用 nlohmann::json

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
    #define CLOSE_SOCKET close
#endif

TcpServer::TcpServer() : serverSocket(INVALID_SOCKET_VALUE), running(false) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize WinSock");
    }
#endif
}

TcpServer::~TcpServer() {
    stopServer();
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpServer::setCredentials(const std::string& username, const std::string& password) {
    this->username = username;
    this->password = password;
}

bool TcpServer::startServer(uint16_t port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET_VALUE) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 允许地址重用
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // 在bind之前添加以下代码
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(serverSocket, FIONBIO, &mode) != 0) {
        std::cerr << "Failed to set server socket to non-blocking mode" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }
#else
    int flags = fcntl(serverSocket, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "Failed to get server socket flags" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }
    if (fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Failed to set server socket to non-blocking mode" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有接口
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        std::cerr << "Listen failed" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }

    std::cout << "Server is listening on port " << port << std::endl;
    running = true;
    return true;
}

void TcpServer::stopServer() {
    if (serverSocket != INVALID_SOCKET_VALUE) {
        running = false;
        
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& pair : clients) {
            CLOSE_SOCKET(pair.first);
        }
        clients.clear();
        
        CLOSE_SOCKET(serverSocket);
        serverSocket = INVALID_SOCKET_VALUE;
    }
}

void TcpServer::processEvents() {
    if (!running) return;

    // 只处理新的连接请求，因为每个客户端都有自己的处理线程
    for (int i = 0; i < 5; i++) {
        handleNewConnection();
    }
}

void TcpServer::handleNewConnection() {
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
#ifdef _WIN32
    int clientAddrLen = sizeof(clientAddr);
#else
    socklen_t clientAddrLen = sizeof(clientAddr);
#endif
    
    SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket != INVALID_SOCKET_VALUE) {
        // 设置非阻塞模式
#ifdef _WIN32
        unsigned long mode = 1;
        if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
            std::cerr << "Failed to set non-blocking mode" << std::endl;
            CLOSE_SOCKET(clientSocket);
            return;
        }
#else
        int flags = fcntl(clientSocket, F_GETFL, 0);
        if (flags == -1) {
            std::cerr << "Failed to get socket flags" << std::endl;
            CLOSE_SOCKET(clientSocket);
            return;
        }
        if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "Failed to set non-blocking mode" << std::endl;
            CLOSE_SOCKET(clientSocket);
            return;
        }
#endif

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients[clientSocket] = std::make_unique<Client>();
            clients[clientSocket]->socket = clientSocket;
        }
        
        char clientIP[INET_ADDRSTRLEN];
        memset(clientIP, 0, INET_ADDRSTRLEN);
        if (inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN) != nullptr) {
            std::cout << "New client connected from " 
                     << clientIP
                     << ":" << ntohs(clientAddr.sin_port) 
                     << " (socket: " << clientSocket << ")" << std::endl;
        } else {
            std::cout << "New client connected (address conversion failed) with socket: " 
                     << clientSocket << std::endl;
        }

        // 为新客户端创建专门的处理线程
        std::thread clientThread(&TcpServer::handleClientThread, this, clientSocket);
        clientThread.detach(); // 分离线程，让它独立运行
    }
}

void TcpServer::handleClientThread(SocketType clientSocket) {
    try {
        while (running) {
            if (clients.find(clientSocket) == clients.end()) {
                break;
            }

            try {
                handleClientData(clientSocket);
            } catch (const std::exception& e) {
                std::cerr << "Error in client thread for socket " << clientSocket 
                          << ": " << e.what() << std::endl;
                break;
            }

            // 添加短暂的休眠以避免CPU过度使用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (const std::exception& e) {
        std::cerr << "Client thread error for socket " << clientSocket 
                  << ": " << e.what() << std::endl;
    }

    // 线程结束时清理客户端连接
    removeClient(clientSocket);
}

void TcpServer::handleClientData(SocketType clientSocket) {
    if (clients.find(clientSocket) == clients.end()) {
        std::cerr << "Error: Client socket " << clientSocket << " not found in clients map" << std::endl;
        return;
    }

    char buffer[4096];
    int bytesRead;

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    // 添加详细的错误处理和调试信息
#ifdef _WIN32
    if (bytesRead == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            std::cerr << "Recv failed for socket " << clientSocket 
                      << " with error: " << error << std::endl;
            throw std::runtime_error("Connection error");
        }
        return;
    }
#else
    if (bytesRead < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "Recv failed for socket " << clientSocket 
                      << " with error: " << errno << std::endl;
            throw std::runtime_error("Connection error");
        }
        return;
    }
#endif

    if (bytesRead == 0) {
        std::cout << "Client " << clientSocket << " closed connection" << std::endl;
        throw std::runtime_error("Connection closed by peer");
    }

    buffer[bytesRead] = '\0';
    auto& client = clients[clientSocket];
    client->buffer += buffer;

    std::cout << "Received " << bytesRead << " bytes from client " << clientSocket 
              << ", Current buffer size: " << client->buffer.length() << std::endl;
    std::cout << "Raw data: " << buffer << std::endl;
    std::cout << "Client authentication status: " 
              << (client->authenticated ? "authenticated" : "not authenticated") << std::endl;

    // 尝试处理完整的JSON消息
    try {
        size_t start = client->buffer.find_first_of('{');
        while (start != std::string::npos) {
            size_t end = client->buffer.find_first_of('}', start);
            if (end == std::string::npos) break;

            std::string jsonStr = client->buffer.substr(start, end - start + 1);
            
            std::cout << "Processing JSON from client " << clientSocket 
                      << ": " << jsonStr << std::endl;
            
            try {
                auto jsonData = json::parse(jsonStr);
                
                if (!jsonData.contains("type")) {
                    std::cerr << "Message missing 'type' field from client " 
                              << clientSocket << std::endl;
                    continue;
                }

                std::string msgType = jsonData["type"].get<std::string>();
                std::cout << "Message type: " << msgType << " from client " 
                          << clientSocket << std::endl;
                
                if (msgType == "auth") {
                    bool authResult = authenticateClient(*client, jsonData);
                    std::cout << "Authentication attempt for client " << clientSocket 
                              << " result: " << (authResult ? "success" : "failed") << std::endl;
                } 
                else if (msgType == "clipboard") {
                    if (!client->authenticated) {
                        std::cout << "Warning: Unauthenticated client " << clientSocket 
                                  << " trying to send clipboard data. Sending auth required message." << std::endl;
                        
                        // 发送认证要求消息
                        json authRequiredMsg;
                        authRequiredMsg["type"] = "error";
                        authRequiredMsg["message"] = "Authentication required";
                        std::string response = authRequiredMsg.dump() + "\n";
                        send(clientSocket, response.c_str(), response.length(), 0);
                        
                        client->buffer.erase(0, end + 1);
                        return;
                    }
                    
                    if (jsonData.contains("content")) {
                        std::string content = jsonData["content"].get<std::string>();
                        std::cout << "\n=== Clipboard Update ===\n"
                                  << "From client: " << clientSocket << "\n"
                                  << "Content: " << content << "\n"
                                  << "=====================\n" << std::endl;
                        
                        broadcastClipboardData(jsonStr, clientSocket);
                    }
                }
            } catch (const json::parse_error& e) {
                std::cerr << "JSON parse error for client " << clientSocket 
                          << ": " << e.what() << std::endl;
                std::cerr << "Invalid message: " << jsonStr << std::endl;
            }

            // 移除已处理的消息
            client->buffer.erase(0, end + 1);
            start = client->buffer.find_first_of('{');
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing data for client " << clientSocket 
                  << ": " << e.what() << std::endl;
    }
}

void TcpServer::removeClient(SocketType clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    if (clients.find(clientSocket) != clients.end()) {
        CLOSE_SOCKET(clientSocket);
        clients.erase(clientSocket);
        std::cout << "Client disconnected" << std::endl;
    }
}

bool TcpServer::authenticateClient(Client& client, const json& jsonData) {
    try {
        if (!jsonData.contains("username") || !jsonData.contains("password")) {
            std::cerr << "Missing username or password in JSON" << std::endl;
            return false;
        }
        
        std::string receivedUsername = jsonData["username"].get<std::string>();
        std::string receivedPassword = jsonData["password"].get<std::string>();
        
        std::cout << "Socket " << client.socket << " - Received credentials - Username: " 
                  << receivedUsername << ", Password length: " << receivedPassword.length() << std::endl;
        
        bool authenticated = (receivedUsername == username && receivedPassword == password);
        
        // 设置客户端的认证状态
        client.authenticated = authenticated;
        
        // 构造JSON响应
        json responseJson;
        responseJson["type"] = "auth_response";
        responseJson["status"] = authenticated ? "ok" : "failed";
        std::string response = responseJson.dump() + "\n";
        
        std::cout << "Socket " << client.socket << " - Authentication " 
                  << (authenticated ? "successful" : "failed") << std::endl;
        
        // 确保响应被完整发送
        int totalSent = 0;
        int len = response.length();
        while (totalSent < len) {
            int sent = send(client.socket, response.c_str() + totalSent, len - totalSent, 0);
            if (sent < 0) {
                std::cerr << "Failed to send auth response to client " << client.socket << std::endl;
                return false;
            }
            totalSent += sent;
        }

        return authenticated;
    } catch (const std::exception& e) {
        std::cerr << "Authentication error for socket " << client.socket << ": " << e.what() << std::endl;
        return false;
    }
}

void TcpServer::broadcastClipboardData(const std::string& data, SocketType excludeSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    
    try {
        auto jsonData = json::parse(data);
        std::string content = jsonData["content"].get<std::string>();
        
        std::cout << "\n=== Broadcasting Clipboard ===\n"
                  << "Content: " << content << "\n"
                  << "To clients: ";
        
        for (const auto& pair : clients) {
            if (pair.first != excludeSocket && pair.second->authenticated) {
                std::string message = data + "\n";
                send(pair.first, message.c_str(), message.length(), 0);
                std::cout << pair.first << " ";
            }
        }
        std::cout << "\n===========================\n" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error broadcasting clipboard data: " << e.what() << std::endl;
    }
}