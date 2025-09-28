#include "Message.h"
#include <random>
#include <sstream>
#include <iomanip>

// Generate a simple UUID-like string (for compatibility with Teflon)
std::string Message::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
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

// JSON deserialization function
void from_json(const nlohmann::json& j, Message& msg) {
    j.at("senderId").get_to(msg.senderId);
    j.at("body").get_to(msg.body);
    j.at("messageId").get_to(msg.messageId);

    std::string typeStr;
    j.at("type").get_to(typeStr);
    msg.type = Message::stringToMessageType(typeStr);

    j.at("checksum").get_to(msg.checksum);

    // Handle optional originalMessageId
    if (j.contains("originalMessageId") && !j["originalMessageId"].is_null()) {
        std::string originalId;
        j.at("originalMessageId").get_to(originalId);
        msg.originalMessageId = originalId;
    } else {
        msg.originalMessageId = std::nullopt;
    }
}
