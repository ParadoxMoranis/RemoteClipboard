#pragma once

#include <chrono>
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

struct IncomingTransferState
{
    std::string transferId;
    std::string sender;
    std::string fileName;
    std::string mimeType;
    std::string sha256;
    std::filesystem::path directory;
    std::filesystem::path temporaryPath;
    std::filesystem::path targetPath;
    std::unique_ptr<std::ofstream> output;
    std::size_t expectedSize = 0;
    std::size_t receivedSize = 0;
    std::uint64_t nextSequence = 0;
    std::chrono::steady_clock::time_point expiresAt;
};

std::string sanitizeFileName(const std::string& fileName);
std::string currentTimestamp();
std::filesystem::path makeFilePathUnique(const std::filesystem::path& requestedPath);
bool decodeBase64(const std::string& input, std::vector<unsigned char>& output);
std::string encodeBase64(const std::vector<unsigned char>& input);
std::string sha256Hex(const std::vector<unsigned char>& data);
std::string sha256HexForFile(const std::filesystem::path& path);
bool ensureDirectory(const std::filesystem::path& directory);
bool isValidSha256(const std::string& value);

} // namespace filetransfer
