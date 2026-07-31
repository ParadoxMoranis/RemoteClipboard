#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct ServerAppConfig
{
    uint16_t port = 8080;
    std::string username;
    std::string password;
    std::string passwordSecretFile;
    std::string storageDir = "received_files";
    std::string sqlitePath = "remote_clipboard.sqlite3";
    int retentionDays = 7;
    bool tlsEnabled = true;
    std::string tlsCertificateFile;
    std::string tlsKeyFile;
    bool developmentMode = false;
};

class ServerConfigStore
{
  public:
    explicit ServerConfigStore(std::string applicationName);

    std::filesystem::path configPath() const;
    ServerAppConfig load() const;
    bool save(const ServerAppConfig& config, std::string* errorMessage = nullptr) const;

  private:
    std::filesystem::path configDirectory() const;

    std::string applicationName_;
};
