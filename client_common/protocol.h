#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace remoteclipboard::v1 {

using Json = nlohmann::json;

namespace type {
inline constexpr const char* kAuth = "auth";
inline constexpr const char* kAuthResponse = "auth_response";
inline constexpr const char* kClipboardText = "clipboard_text";
inline constexpr const char* kFileBundle = "file_bundle";
inline constexpr const char* kTransferStart = "file_transfer_start";
inline constexpr const char* kTransferChunk = "file_transfer_chunk";
inline constexpr const char* kTransferComplete = "file_transfer_complete";
inline constexpr const char* kPing = "ping";
inline constexpr const char* kPong = "pong";
inline constexpr const char* kError = "error";
} // namespace type

struct ValidationResult
{
    bool valid = false;
    std::string error;
};

Json makeAuth(const std::string& username, const std::string& password);
Json makeClipboardText(const std::string& content);
Json makeTransferStart(const std::string& transferId, const std::string& name, std::uint64_t size,
                       const std::string& sha256, const std::string& mime = {});
Json makeTransferChunk(const std::string& transferId, std::uint64_t sequence,
                       const std::string& base64Data);
Json makeTransferComplete(const std::string& transferId);

ValidationResult validateClientMessage(const Json& message, std::size_t maxPayloadBytes);

} // namespace remoteclipboard::v1
