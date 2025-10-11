#include "Message.h"
#include "JsonUtils.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")

// Generate a proper UUID using Windows RPC
std::string Message::generateUUID() {
    UUID uuid;
    UuidCreate(&uuid);

    // Convert UUID to string
    RPC_CSTR uuidString;
    UuidToStringA(&uuid, &uuidString);

    std::string result(reinterpret_cast<char*>(uuidString));
    RpcStringFreeA(&uuidString);

    return result;
}

// Calculate CRC32 checksum for message body (matching Java's CRC32)
uint32_t Message::calculateChecksum(const std::string& content) {
    // Simple CRC32 implementation (matching Java's CRC32 behavior)
    uint32_t crc = 0xFFFFFFFF;
    for (char c : content) {
        crc ^= static_cast<uint8_t>(c);
        for (int i = 0; i < 8; i++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// Convert MessageType enum to string for JSON
std::string Message::messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::CHAT: return "CHAT";
        case MessageType::ACK: return "ACK";
        case MessageType::NACK: return "NACK";
        case MessageType::SYSTEM_EVENT: return "SYSTEM_EVENT";
        default: return "CHAT";
    }
}

// Convert string to MessageType enum from JSON
MessageType Message::stringToMessageType(const std::string& typeStr) {
    if (typeStr == "CHAT") return MessageType::CHAT;
    if (typeStr == "ACK") return MessageType::ACK;
    if (typeStr == "NACK") return MessageType::NACK;
    if (typeStr == "SYSTEM_EVENT") return MessageType::SYSTEM_EVENT;
    return MessageType::CHAT; // Default fallback
}

// JSON serialization function
void to_json(nlohmann::json& j, const Message& msg) {
    j = nlohmann::json{
        {"senderId", msg.senderId},
        {"body", msg.body},
        {"messageId", msg.messageId},
        {"type", Message::messageTypeToString(msg.type)},
        {"checksum", msg.checksum}
    };

    // Add originalMessageId only if it has a value
    if (msg.originalMessageId.has_value()) {
        j["originalMessageId"] = msg.originalMessageId.value();
    }
}

// JSON deserialization function - optimized using JSON pointer literals
void from_json(const nlohmann::json& j, Message& msg) {
    using namespace nlohmann::literals;

    // Use JSON pointer literals for efficient property access
    msg.senderId = j["/senderId"_json_pointer];
    msg.body = j["/body"_json_pointer];
    msg.messageId = j["/messageId"_json_pointer];

    // Handle type conversion
    std::string typeStr = j["/type"_json_pointer];
    if (!typeStr.empty()) {
        msg.type = Message::stringToMessageType(typeStr);
    }

    // Handle optional originalMessageId
    if (j.contains("originalMessageId")) {
        msg.originalMessageId = j["/originalMessageId"_json_pointer];
    } else {
        msg.originalMessageId = std::nullopt;
    }

    // Extract checksum (integer)
    msg.checksum = j["/checksum"_json_pointer];
}
