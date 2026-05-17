#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

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

std::string trim(const std::string& value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}
}

CliConfigStore::CliConfigStore(std::string applicationName)
    : applicationName_(std::move(applicationName))
{
}

std::filesystem::path CliConfigStore::configDirectory() const
{
    return defaultConfigRoot() / applicationName_;
}

std::filesystem::path CliConfigStore::configPath() const
{
    return configDirectory() / "config.json";
}

CliAppConfig CliConfigStore::load() const
{
    CliAppConfig config;
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

    config.host = root.value("host", config.host);
    config.port = static_cast<uint16_t>(root.value("port", static_cast<int>(config.port)));
    config.username = root.value("username", config.username);
    config.password = root.value("password", config.password);
    config.receiveDir = root.value("receive_directory", config.receiveDir);
    config.tlsEnabled = root.value("tls_enabled", config.tlsEnabled);
    config.tlsCaFile = root.value("tls_ca_file", config.tlsCaFile);
    config.allowInsecureTls = root.value("allow_insecure_tls", config.allowInsecureTls);
    return config;
}

bool CliConfigStore::save(const CliAppConfig& config, std::string* errorMessage) const
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
        {"host", config.host},
        {"port", config.port},
        {"username", config.username},
        {"password", config.password},
        {"receive_directory", config.receiveDir},
        {"tls_enabled", config.tlsEnabled},
        {"tls_ca_file", config.tlsCaFile},
        {"allow_insecure_tls", config.allowInsecureTls}
    };

    std::ofstream output(configPath());
    if (!output) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unable to write CLI config file";
        }
        return false;
    }

    output << root.dump(2) << std::endl;
    return true;
}

bool ensureDirectoryInteractive(const std::string& path, bool interactive, std::string& resolvedPath)
{
    const std::string trimmed = trim(path);
    if (trimmed.empty()) {
        return false;
    }

    const std::filesystem::path candidate = std::filesystem::absolute(trimmed);
    if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
        resolvedPath = candidate.string();
        if (interactive) {
            std::cout << "Receive directory found: " << resolvedPath << std::endl;
        }
        return true;
    }

    if (!interactive) {
        try {
            std::filesystem::create_directories(candidate);
            resolvedPath = candidate.string();
            return true;
        } catch (...) {
            return false;
        }
    }

    std::cout << "Receive directory not found: " << candidate << std::endl;
    std::cout << "Create it now? [y/N]: ";
    std::string answer;
    std::getline(std::cin, answer);
    answer = trim(answer);
    if (answer != "y" && answer != "Y" && answer != "yes" && answer != "YES") {
        std::cout << "Directory was not created." << std::endl;
        return false;
    }

    try {
        std::filesystem::create_directories(candidate);
        resolvedPath = candidate.string();
        std::cout << "Directory created successfully: " << resolvedPath << std::endl;
        return true;
    } catch (const std::exception& error) {
        std::cout << "Failed to create directory: " << error.what() << std::endl;
        return false;
    }
}
