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

// 添加 Cocoa 头文件
#import <Cocoa/Cocoa.h>

using json = nlohmann::json;

// TCPClient 类保持不变
// ... existing code ...

class ClipboardMonitor {
private:
    std::string last_content;
    TCPClient& client;

    std::string get_clipboard_content() {
        @autoreleasepool {
            NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
            NSString *string = [pasteboard stringForType:NSPasteboardTypeString];
            if (string) {
                return std::string([string UTF8String]);
            }
            return "";
        }
    }

    void set_clipboard_content(const std::string& content) {
        @autoreleasepool {
            NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            NSString *nsString = [NSString stringWithUTF8String:content.c_str()];
            [pasteboard setString:nsString forType:NSPasteboardTypeString];
        }
    }

public:
    ClipboardMonitor(TCPClient& tcp_client) : client(tcp_client) {
        last_content = get_clipboard_content();
    }

    void monitor_clipboard() {
        while (true) {
            @autoreleasepool {
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
    }

    void handle_server_messages() {
        while (true) {
            @autoreleasepool {
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
    }
};

int main() {
    @autoreleasepool {
        // 确保在主线程初始化 Cocoa
        [NSApplication sharedApplication];
        
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
} 