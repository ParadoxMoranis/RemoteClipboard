#include "serverconfig.h"

#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
std::filesystem::path homeDirectory()
{
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return home;
    }
    return std::filesystem::current_path();
}

std::filesystem::path defaultConfigRoot()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "RemoteClipboard";
    }
    return homeDirectory() / ".config" / "RemoteClipboard";
}
}

ServerConfigStore::ServerConfigStore(std::string applicationName)
    : applicationName_(std::move(applicationName))
{
}

std::filesystem::path ServerConfigStore::configDirectory() const
{
    return defaultConfigRoot() / applicationName_;
}

std::filesystem::path ServerConfigStore::configPath() const
{
    return configDirectory() / "config.json";
}

ServerAppConfig ServerConfigStore::load() const
{
    ServerAppConfig config;
    const auto path = configPath();
    if (!std::filesystem::exists(path)) {
        return config;
    }

    std::ifstream input(path);
    if (!input) {
        return config;
    }

    json root;
    try {
        input >> root;
    } catch (...) {
        return config;
    }

    config.port = static_cast<uint16_t>(root.value("port", static_cast<int>(config.port)));
    config.username = root.value("username", config.username);
    config.password = root.value("password", config.password);
    config.storageDir = root.value("storage_directory", config.storageDir);
    config.retentionDays = root.value("retention_days", config.retentionDays);
    config.tlsEnabled = root.value("tls_enabled", config.tlsEnabled);
    config.tlsCertificateFile = root.value("tls_certificate_file", config.tlsCertificateFile);
    config.tlsKeyFile = root.value("tls_key_file", config.tlsKeyFile);
    return config;
}

bool ServerConfigStore::save(const ServerAppConfig& config, std::string* errorMessage) const
{
    try {
        std::filesystem::create_directories(configDirectory());
    } catch (const std::exception& error) {
        if (errorMessage != nullptr) {
            *errorMessage = error.what();
        }
        return false;
    }

    json root = {
        {"version", 1},
        {"port", config.port},
        {"username", config.username},
        {"password", config.password},
        {"storage_directory", config.storageDir},
        {"retention_days", config.retentionDays},
        {"tls_enabled", config.tlsEnabled},
        {"tls_certificate_file", config.tlsCertificateFile},
        {"tls_key_file", config.tlsKeyFile}
    };

    std::ofstream output(configPath());
    if (!output) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unable to write server config file";
        }
        return false;
    }

    output << root.dump(2) << std::endl;
    return true;
}
