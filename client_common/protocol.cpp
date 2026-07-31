#include "protocol.h"

#include <limits>

namespace remoteclipboard::v1 {
namespace {

bool isBoundedString(const Json& message, const char* key, std::size_t maxLength,
                     bool allowEmpty = false)
{
    if (!message.contains(key) || !message[key].is_string()) {
        return false;
    }
    const auto& value = message[key].get_ref<const std::string&>();
    return value.size() <= maxLength && (allowEmpty || !value.empty());
}

ValidationResult invalid(std::string error) { return {false, std::move(error)}; }

} // namespace

Json makeAuth(const std::string& username, const std::string& password)
{
    return {{"type", type::kAuth}, {"username", username}, {"password", password}};
}

Json makeClipboardText(const std::string& content)
{
    return {{"type", type::kClipboardText}, {"content", content}};
}

Json makeTransferStart(const std::string& transferId, const std::string& name, std::uint64_t size,
                       const std::string& sha256, const std::string& mime)
{
    Json message = {
        {"type", type::kTransferStart},
        {"transfer_id", transferId},
        {"name", name},
        {"size", size},
        {"sha256", sha256},
    };
    if (!mime.empty()) {
        message["mime"] = mime;
    }
    return message;
}

Json makeTransferChunk(const std::string& transferId, std::uint64_t sequence,
                       const std::string& base64Data)
{
    return {
        {"type", type::kTransferChunk},
        {"transfer_id", transferId},
        {"seq", sequence},
        {"data", base64Data},
    };
}

Json makeTransferComplete(const std::string& transferId)
{
    return {{"type", type::kTransferComplete}, {"transfer_id", transferId}};
}

ValidationResult validateClientMessage(const Json& message, std::size_t maxPayloadBytes)
{
    if (!message.is_object() || !isBoundedString(message, "type", 64)) {
        return invalid("Missing or invalid message type");
    }

    const std::string messageType = message["type"].get<std::string>();
    if (messageType == type::kAuth) {
        if (!isBoundedString(message, "username", 256) ||
            !isBoundedString(message, "password", 4096)) {
            return invalid("auth requires bounded username and password strings");
        }
        return {true, {}};
    }
    if (messageType == type::kPing) {
        return {true, {}};
    }
    if (messageType == type::kClipboardText) {
        if (!isBoundedString(message, "content", maxPayloadBytes, true)) {
            return invalid("clipboard_text requires bounded string content");
        }
        return {true, {}};
    }
    if (messageType == type::kFileBundle) {
        if (!message.contains("files") || !message["files"].is_array() ||
            message["files"].empty()) {
            return invalid("file_bundle requires a non-empty files array");
        }
        return {true, {}};
    }
    if (messageType == type::kTransferStart) {
        if (!isBoundedString(message, "transfer_id", 128) ||
            !isBoundedString(message, "name", 1024) || !message.contains("size") ||
            !message["size"].is_number_unsigned()) {
            return invalid("file_transfer_start requires transfer_id, name and unsigned size");
        }
        if (message.contains("sha256") && !message["sha256"].is_string()) {
            return invalid("file_transfer_start sha256 must be a string");
        }
        if (message.contains("mime") && !isBoundedString(message, "mime", 256, true)) {
            return invalid("file_transfer_start mime must be a bounded string");
        }
        return {true, {}};
    }
    if (messageType == type::kTransferChunk) {
        if (!isBoundedString(message, "transfer_id", 128) || !message.contains("seq") ||
            !message["seq"].is_number_unsigned() ||
            !isBoundedString(message, "data", maxPayloadBytes, true)) {
            return invalid("file_transfer_chunk requires transfer_id, unsigned seq and data");
        }
        return {true, {}};
    }
    if (messageType == type::kTransferComplete) {
        if (!isBoundedString(message, "transfer_id", 128)) {
            return invalid("file_transfer_complete requires transfer_id");
        }
        return {true, {}};
    }

    return invalid("Unsupported message type");
}

} // namespace remoteclipboard::v1
