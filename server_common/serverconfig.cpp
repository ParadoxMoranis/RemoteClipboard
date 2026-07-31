#include "serverconfig.h"

#include <cstdlib>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <sys/stat.h>
#endif

using Json = nlohmann::json;

namespace {
constexpr int kConfigVersion = 2;

std::filesystem::path homeDirectory()
{
    if (const char* homePath = std::getenv("HOME"); homePath != nullptr && *homePath != '\0') {
        return homePath;
    }
    return std::filesystem::current_path();
}

std::filesystem::path defaultConfigRoot()
{
    if (const char* configPath = std::getenv("XDG_CONFIG_HOME");
        configPath != nullptr && *configPath != '\0') {
        return std::filesystem::path(configPath) / "RemoteClipboard";
    }
    return homeDirectory() / ".config" / "RemoteClipboard";
}

void restrictPermissions(const std::filesystem::path& path)
{
#ifndef _WIN32
    chmod(path.c_str(), S_IRUSR | S_IWUSR);
#else
    (void)path;
#endif
}
} // namespace

ServerConfigStore::ServerConfigStore(std::string applicationName)
    : applicationName_(std::move(applicationName))
{}

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
    std::ifstream input(configPath());
    if (!input) {
        return config;
    }

    try {
        Json root;
        input >> root;
        const int port = root.value("port", static_cast<int>(config.port));
        if (port > 0 && port <= 65535) {
            config.port = static_cast<std::uint16_t>(port);
        }
        config.username = root.value("username", config.username);
        config.password = root.value("password", config.password);
        config.passwordSecretFile = root.value("password_secret_file", config.passwordSecretFile);
        config.storageDir = root.value("storage_directory", config.storageDir);
        config.sqlitePath = root.value("sqlite_path", config.sqlitePath);
        config.retentionDays = root.value("retention_days", config.retentionDays);
        config.tlsEnabled = root.value("tls_enabled", config.tlsEnabled);
        config.tlsCertificateFile = root.value("tls_certificate_file", config.tlsCertificateFile);
        config.tlsKeyFile = root.value("tls_key_file", config.tlsKeyFile);
        config.developmentMode = root.value("development_mode", config.developmentMode);
    } catch (const std::exception&) {
        return ServerAppConfig{};
    }
    return config;
}

bool ServerConfigStore::save(const ServerAppConfig& config, std::string* errorMessage) const
{
    std::error_code error;
    std::filesystem::create_directories(configDirectory(), error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = error.message();
        }
        return false;
    }

    const Json root = {
        {"version", kConfigVersion},
        {"port", config.port},
        {"username", config.username},
        {"password_secret_file", config.passwordSecretFile},
        {"storage_directory", config.storageDir},
        {"sqlite_path", config.sqlitePath},
        {"retention_days", config.retentionDays},
        {"tls_enabled", config.tlsEnabled},
        {"tls_certificate_file", config.tlsCertificateFile},
        {"tls_key_file", config.tlsKeyFile},
        {"development_mode", config.developmentMode},
    };

    const auto destination = configPath();
    auto temporary = destination;
    temporary += ".tmp";
    auto backup = destination;
    backup += ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output || !(output << root.dump(2) << '\n')) {
            if (errorMessage != nullptr) {
                *errorMessage = "Unable to write temporary server configuration";
            }
            return false;
        }
    }
    restrictPermissions(temporary);

    if (std::filesystem::exists(destination)) {
        std::filesystem::copy_file(destination, backup,
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            std::filesystem::remove(temporary);
            if (errorMessage != nullptr) {
                *errorMessage = "Unable to back up server configuration: " + error.message();
            }
            return false;
        }
        restrictPermissions(backup);
        std::filesystem::remove(destination, error);
        if (error) {
            std::filesystem::remove(temporary);
            if (errorMessage != nullptr) {
                *errorMessage = "Unable to replace server configuration: " + error.message();
            }
            return false;
        }
    }
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unable to install server configuration: " + error.message();
        }
        return false;
    }
    restrictPermissions(destination);
    return true;
}
