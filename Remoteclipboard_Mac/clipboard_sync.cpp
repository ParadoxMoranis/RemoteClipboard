#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <memory>

using json = nlohmann::json;

class TCPClient {
private:
    int sock;
    std::string ip;
    int port;
    bool connected;

public:
    TCPClient() : sock(-1), connected(false) {}

    bool connect(const std::string& server_ip, int server_port) {
        ip = server_ip;
        port = server_port;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            std::cerr << "Could not create socket" << std::endl;
            return false;
        }

        struct sockaddr_in server;
        server.sin_addr.s_addr = inet_addr(ip.c_str());
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (::connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
            std::cerr << "Connect failed" << std::endl;
            return false;
        }

        connected = true;
        return true;
    }

    bool send_data(const std::string& message) {
        if (!connected) return false;
        if (send(sock, message.c_str(), message.length(), 0) < 0) {
            std::cerr << "Send failed" << std::endl;
            return false;
        }
        return true;
    }

    std::string receive_data() {
        if (!connected) return "";
        char buffer[4096] = {0};
        int received = recv(sock, buffer, 4095, 0);
        if (received < 0) {
            std::cerr << "Receive failed" << std::endl;
            return "";
        }
        return std::string(buffer);
    }

    void close_connection() {
        if (connected) {
            close(sock);
            connected = false;
        }
    }

    ~TCPClient() {
        close_connection();
    }
};

class ClipboardMonitor {
private:
    std::string last_content;
    TCPClient& client;

    std::string get_clipboard_content() {
        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen("wl-paste", "r"),
            pclose
        );
        if (!pipe) {
            return "";
        }
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result += buffer;
        }
        return result;
    }

    void set_clipboard_content(const std::string& content) {
        std::string cmd = "echo -n '" + content + "' | wl-copy";
        system(cmd.c_str());
    }

public:
    ClipboardMonitor(TCPClient& tcp_client) : client(tcp_client) {
        last_content = get_clipboard_content();
    }

    void monitor_clipboard() {
        while (true) {
            std::string current_content = get_clipboard_content();
            if (current_content != last_content && !current_content.empty()) {
                json message = {
                    {"type", "clipboard"},
                    {"content", current_content}
                };
                client.send_data(message.dump());
                last_content = current_content;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void handle_server_messages() {
        while (true) {
            std::string response = client.receive_data();
            if (!response.empty()) {
                try {
                    json j = json::parse(response);
                    if (j["type"] == "clipboard") {
                        std::string content = j["content"];
                        last_content = content;
                        set_clipboard_content(content);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

int main() {
    std::string server_ip;
    int server_port;
    std::string username;
    std::string password;

    std::cout << "Enter server IP: ";
    std::cin >> server_ip;
    std::cout << "Enter server port: ";
    std::cin >> server_port;
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    TCPClient client;
    if (!client.connect(server_ip, server_port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Send authentication
    json auth_message = {
        {"type", "auth"},
        {"username", username},
        {"password", password}
    };
    client.send_data(auth_message.dump());

    // Wait for auth response
    std::string auth_response = client.receive_data();
    try {
        json response = json::parse(auth_response);
        if (response["type"] != "auth_response" || response["status"] != "ok") {
            std::cerr << "Authentication failed" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing auth response: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Authentication successful" << std::endl;

    // Start clipboard monitoring
    ClipboardMonitor monitor(client);
    
    // Create threads for monitoring clipboard and handling server messages
    std::thread clipboard_thread(&ClipboardMonitor::monitor_clipboard, &monitor);
    std::thread server_thread(&ClipboardMonitor::handle_server_messages, &monitor);

    clipboard_thread.join();
    server_thread.join();

    return 0;
} 