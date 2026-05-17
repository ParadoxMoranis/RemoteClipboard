#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace filetransfer {

constexpr std::size_t kChunkTransferThresholdBytes = 4ull * 1024ull * 1024ull;
constexpr std::size_t kChunkSizeBytes = 512ull * 1024ull;

struct IncomingTransferState {
    std::string transferId;
    std::string sender;
    std::string fileName;
    std::string mimeType;
    std::string sha256;
    std::filesystem::path directory;
    std::filesystem::path targetPath;
    std::unique_ptr<std::ofstream> output;
    std::size_t expectedSize = 0;
    std::size_t receivedSize = 0;
};

std::string sanitizeFileName(const std::string& fileName);
std::string currentTimestamp();
std::filesystem::path makeFilePathUnique(const std::filesystem::path& requestedPath);
bool decodeBase64(const std::string& input, std::vector<unsigned char>& output);
std::string encodeBase64(const std::vector<unsigned char>& input);
std::string sha256Hex(const std::vector<unsigned char>& data);
std::string sha256HexForFile(const std::filesystem::path& path);
bool ensureDirectory(const std::filesystem::path& directory);

} // namespace filetransfer
