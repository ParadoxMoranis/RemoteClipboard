#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct CliAppConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string username;
    std::string password;
    std::string passwordSecretFile;
    bool passwordFromCommandLine = false;
    std::string receiveDir;
    bool tlsEnabled = true;
    std::string tlsCaFile;
    bool allowInsecureTls = false;
    bool developmentMode = false;
};

class CliConfigStore {
public:
    explicit CliConfigStore(std::string applicationName);

    std::filesystem::path configPath() const;
    CliAppConfig load() const;
    bool save(const CliAppConfig& config, std::string* errorMessage = nullptr) const;

private:
    std::filesystem::path configDirectory() const;

    std::string applicationName_;
};

bool ensureDirectoryInteractive(const std::string& path, bool interactive, std::string& resolvedPath);
