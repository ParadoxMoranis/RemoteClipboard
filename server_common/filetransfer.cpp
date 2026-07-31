#include "filetransfer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/sha.h>

namespace filetransfer {

std::string sanitizeFileName(const std::string& fileName)
{
    std::string sanitized = std::filesystem::path(fileName).filename().string();
    if (sanitized.empty() || sanitized == "." || sanitized == "..") {
        sanitized = "clipboard-file";
    }
    std::replace_if(
        sanitized.begin(), sanitized.end(),
        [](char value) {
            const auto byte = static_cast<unsigned char>(value);
            return byte < 0x20 || value == '/' || value == '\\' || value == ':' || value == '*' ||
                   value == '?' || value == '"' || value == '<' || value == '>' || value == '|';
        },
        '_');
    constexpr std::size_t kMaxFileNameBytes = 240;
    if (sanitized.size() > kMaxFileNameBytes) {
        sanitized.resize(kMaxFileNameBytes);
    }
    return sanitized;
}

std::string currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return stream.str();
}

std::filesystem::path makeFilePathUnique(const std::filesystem::path& requestedPath)
{
    if (!std::filesystem::exists(requestedPath)) {
        return requestedPath;
    }

    const auto stem = requestedPath.stem().string();
    const auto extension = requestedPath.extension().string();
    for (int index = 1; index < 10000; ++index) {
        const auto candidate =
            requestedPath.parent_path() / (stem + "-" + std::to_string(index) + extension);
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return requestedPath.parent_path() / (stem + "-" + currentTimestamp() + extension);
}

bool decodeBase64(const std::string& input, std::vector<unsigned char>& output)
{
    std::string compact;
    compact.reserve(input.size());
    for (const char character : input) {
        if (!std::isspace(static_cast<unsigned char>(character))) {
            compact.push_back(character);
        }
    }

    if (compact.empty()) {
        output.clear();
        return true;
    }

    if (compact.size() % 4 != 0) {
        output.clear();
        return false;
    }
    std::size_t padding = 0;
    if (compact.back() == '=') {
        ++padding;
    }
    if (compact.size() > 1 && compact[compact.size() - 2] == '=') {
        ++padding;
    }
    for (std::size_t index = 0; index < compact.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(compact[index]);
        const bool alphabet = std::isalnum(value) || value == '+' || value == '/';
        const bool validPadding = value == '=' && index >= compact.size() - padding;
        if (!alphabet && !validPadding) {
            output.clear();
            return false;
        }
    }

    output.assign((compact.size() / 4) * 3, 0);
    const int decodedLength =
        EVP_DecodeBlock(output.data(), reinterpret_cast<const unsigned char*>(compact.data()),
                        static_cast<int>(compact.size()));
    if (decodedLength < 0) {
        output.clear();
        return false;
    }

    std::size_t actualLength = static_cast<std::size_t>(decodedLength);
    if (!compact.empty() && compact.back() == '=') {
        --actualLength;
    }
    if (compact.size() > 1 && compact[compact.size() - 2] == '=') {
        --actualLength;
    }
    output.resize(actualLength);
    return true;
}

std::string encodeBase64(const std::vector<unsigned char>& input)
{
    if (input.empty()) {
        return {};
    }

    std::string output(((input.size() + 2) / 3) * 4, '\0');
    const int encodedLength = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(output.data()),
                                              input.data(), static_cast<int>(input.size()));
    output.resize(static_cast<std::size_t>(encodedLength));
    return output;
}

std::string sha256Hex(const std::vector<unsigned char>& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
    SHA256(data.data(), data.size(), hash);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const unsigned char byte : hash) {
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

std::string sha256HexForFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) {
        return {};
    }
    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return {};
    }

    std::vector<char> buffer(64 * 1024);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            if (EVP_DigestUpdate(context, buffer.data(), static_cast<std::size_t>(count)) != 1) {
                EVP_MD_CTX_free(context);
                return {};
            }
        }
    }

    unsigned char hash[SHA256_DIGEST_LENGTH] = {0};
    unsigned int hashLength = 0;
    if (EVP_DigestFinal_ex(context, hash, &hashLength) != 1) {
        EVP_MD_CTX_free(context);
        return {};
    }
    EVP_MD_CTX_free(context);

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < hashLength; ++index) {
        const unsigned char byte = hash[index];
        stream << std::setw(2) << static_cast<int>(byte);
    }
    return stream.str();
}

bool ensureDirectory(const std::filesystem::path& directory)
{
    try {
        std::filesystem::create_directories(directory);
        return true;
    } catch (...) {
        return false;
    }
}

bool isValidSha256(const std::string& value)
{
    return value.size() == SHA256_DIGEST_LENGTH * 2 &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char character) { return std::isxdigit(character) != 0; });
}

} // namespace filetransfer
