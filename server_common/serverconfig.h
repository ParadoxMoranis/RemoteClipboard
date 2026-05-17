#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct ServerAppConfig {
    uint16_t port = 8080;
    std::string username = "admin";
    std::string password = "admin";
    std::string storageDir = "received_files";
    int retentionDays = 7;
    bool tlsEnabled = false;
    std::string tlsCertificateFile;
    std::string tlsKeyFile;
};

class ServerConfigStore {
public:
    explicit ServerConfigStore(std::string applicationName);

    std::filesystem::path configPath() const;
    ServerAppConfig load() const;
    bool save(const ServerAppConfig& config, std::string* errorMessage = nullptr) const;

private:
    std::filesystem::path configDirectory() const;

    std::string applicationName_;
};
