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
        return;
    }

    // 使用类中定义的常量
    std::vector<char> buffer;
    buffer.reserve(INITIAL_BUFFER_SIZE);
    buffer.resize(INITIAL_BUFFER_SIZE);

    int bytesRead;
    try {
        // 首次读取数据
        bytesRead = recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
        
        if (bytesRead < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                throw std::runtime_error("Connection error");
            }
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                throw std::runtime_error("Connection error");
            }
#endif
            return;
        }

        if (bytesRead == 0) {
            throw std::runtime_error("Connection closed by peer");
        }

        // 如果数据接近缓冲区大小，继续读取
        while (bytesRead >= static_cast<int>(buffer.size() - 1)) {
            size_t newSize = std::min(buffer.size() + CHUNK_SIZE, MAX_BUFFER_SIZE);
            if (newSize == buffer.size()) {
                // 已达到最大缓冲区大小
                break;
            }
            
            std::cout << "Expanding receive buffer to " << (newSize / (1024 * 1024)) 
                      << "MB for client " << clientSocket << std::endl;
            
            buffer.resize(newSize);
            int additionalBytes = recv(clientSocket, buffer.data() + bytesRead, 
                                     buffer.size() - bytesRead - 1, 0);
            if (additionalBytes <= 0) break;
            bytesRead += additionalBytes;
        }

        buffer[bytesRead] = '\0';
        auto& client = clients[clientSocket];

        // 检查客户端累积缓冲区大小
        if (client->buffer.length() + bytesRead > MAX_BUFFER_SIZE) {
            std::cout << "Warning: Client " << clientSocket 
                      << " buffer would exceed " << (MAX_BUFFER_SIZE / (1024 * 1024)) 
                      << "MB limit. Processing existing data first." << std::endl;
            
            // 处理现有数据
            processClientBuffer(*client);
            
            // 如果处理后缓冲区仍然太大，清空它
            if (client->buffer.length() > MAX_BUFFER_SIZE / 2) {
                std::cout << "Clearing client buffer due to size limit" << std::endl;
                client->buffer.clear();
                client->buffer.shrink_to_fit();
            }
        }

        // 添加新数据到客户端缓冲区
        client->buffer.append(buffer.data(), bytesRead);
        std::cout << "Client " << clientSocket << " buffer size: " 
                  << (client->buffer.length() / (1024.0 * 1024.0)) << "MB" << std::endl;

        // 处理接收到的数据
        processClientBuffer(*client);

    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
        // 尝试恢复
        if (auto it = clients.find(clientSocket); it != clients.end()) {
            it->second->buffer.clear();
            it->second->buffer.shrink_to_fit();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing data for client " << clientSocket 
                  << ": " << e.what() << std::endl;
        throw;
    }
}

// 处理客户端缓冲区数据
void TcpServer::processClientBuffer(Client& client) {
    size_t start = 0;
    while ((start = client.buffer.find_first_of('{', start)) != std::string::npos) {
        // 查找匹配的结束括号
        size_t depth = 1;
        size_t end = start + 1;
        while (end < client.buffer.length() && depth > 0) {
            if (client.buffer[end] == '{') depth++;
            if (client.buffer[end] == '}') depth--;
            end++;
        }

        if (depth > 0) {
            // JSON 不完整，等待更多数据
            break;
        }

        end--; // 回退到最后一个 '}'
        std::string jsonStr = client.buffer.substr(start, end - start + 1);
        
        try {
            auto jsonData = json::parse(jsonStr);
            if (jsonData.contains("type")) {
                std::string msgType = jsonData["type"].get<std::string>();
                
                if (msgType == "auth") {
                    client.authenticated = authenticateClient(client, jsonData);
                } 
                else if (msgType == "clipboard") {
                    if (!client.authenticated) {
                        json authRequiredMsg;
                        authRequiredMsg["type"] = "error";
                        authRequiredMsg["message"] = "Authentication required";
                        std::string response = authRequiredMsg.dump() + "\n";
                        send(client.socket, response.c_str(), response.length(), 0);
                    } else if (jsonData.contains("content")) {
                        broadcastClipboardData(jsonStr, client.socket);
                    }
                }
            }
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        // 移除已处理的数据
        client.buffer.erase(0, end + 1);
        start = 0;
    }

    // 如果缓冲区已经很大但没有完整的JSON，可能是无效数据
    if (client.buffer.length() > MAX_BUFFER_SIZE / 2) {
        std::cout << "Warning: Large incomplete buffer detected. Clearing." << std::endl;
        client.buffer.clear();
    }

    // 定期释放未使用的内存
    if (client.buffer.capacity() > client.buffer.length() * 2) {
        client.buffer.shrink_to_fit();
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
        // 验证JSON数据中是否包含必要的认证字段
        if (!jsonData.contains("username") || !jsonData.contains("password")) {
            std::cerr << "Missing username or password in JSON" << std::endl;
            return false;
        }
        
        // 获取认证信息
        std::string receivedUsername = jsonData["username"].get<std::string>();
        std::string receivedPassword = jsonData["password"].get<std::string>();
        
        std::cout << "Socket " << client.socket << " - Received credentials - Username: " 
                  << receivedUsername << ", Password length: " << receivedPassword.length() << std::endl;
        
        // 验证用户名和密码
        bool authenticated = (receivedUsername == username && receivedPassword == password);
        
        // 设置客户端的认证状态
        client.authenticated = authenticated;
        
        // 构造认证响应
        json responseJson;
        responseJson["type"] = "auth_response";
        responseJson["status"] = authenticated ? "ok" : "failed";
        std::string response = responseJson.dump() + "\n";
        
        std::cout << "Socket " << client.socket << " - Authentication " 
                  << (authenticated ? "successful" : "failed") << std::endl;
        
        // 发送认证响应
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
    
    // 添加换行符到消息末尾
    std::string message = data + "\n";
    
    // 设置发送缓冲区大小（64MB）
    const int SEND_BUFFER_SIZE = 64 * 1024 * 1024;
    for (const auto& pair : clients) {
        // 跳过发送源客户端和未认证的客户端
        if (pair.first != excludeSocket && pair.second->authenticated) {
            // 设置socket发送缓冲区大小
#ifdef _WIN32
            if (setsockopt(pair.first, SOL_SOCKET, SO_SNDBUF, 
                          (char*)&SEND_BUFFER_SIZE, sizeof(SEND_BUFFER_SIZE)) != 0) {
#else
            if (setsockopt(pair.first, SOL_SOCKET, SO_SNDBUF, 
                          &SEND_BUFFER_SIZE, sizeof(SEND_BUFFER_SIZE)) != 0) {
#endif
                std::cerr << "Failed to set send buffer size for client " 
                          << pair.first << std::endl;
            }
            
            // 分块发送大型数据
            size_t totalSent = 0;
            const size_t chunkSize = 1024 * 1024;  // 1MB chunks
            
            while (totalSent < message.length()) {
                size_t remaining = message.length() - totalSent;
                size_t currentChunkSize = std::min(chunkSize, remaining);
                
                // 发送数据块
                int sent = send(pair.first, 
                              message.c_str() + totalSent, 
                              currentChunkSize, 
                              0);
                
                if (sent <= 0) {
                    std::cerr << "Failed to send to client " << pair.first << std::endl;
                    break;
                }
                
                totalSent += sent;
                
                // 显示发送进度（仅针对大于1MB的消息）
                if (message.length() > 1024 * 1024) {
                    float progress = (totalSent * 100.0f) / message.length();
                    std::cout << "\rProgress for client " << pair.first 
                              << ": " << std::fixed << std::setprecision(2) 
                              << progress << "%" << std::flush;
                }
            }
            
            // 大消息发送完成后换行
            if (message.length() > 1024 * 1024) {
                std::cout << std::endl;
            }
            
            // 输出发送统计信息
            std::cout << "Broadcasted " << (totalSent / 1024.0) << "KB to client " 
                      << pair.first << std::endl;
        }
    }
}