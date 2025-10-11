#pragma once

#include <string>
#include <optional>

/**
 * Message types supported by the system (matching Teflon's MessageType enum)
 */
enum class MessageType {
    CHAT,           // Regular chat message
    ACK,            // Positive acknowledgment
    NACK,           // Negative acknowledgment
    SYSTEM_EVENT    // System events (connect/disconnect/etc)
};

/**
 * Represents a chat message in the system (matching Teflon's Message record)
 * Contains the sender's ID, message body, and acknowledgment metadata.
 */
struct Message {
    std::string senderId;           // The unique identifier of the message sender
    std::string body;               // The content of the message
    std::string messageId;          // UUID as string for JSON compatibility
    MessageType type;               // Message type
    uint32_t checksum;              // CRC32 checksum (using uint32_t to match Java long)
    std::optional<std::string> originalMessageId; // UUID of original message for ACK/NACK

    // Default constructor
    Message() : type(MessageType::CHAT), checksum(0) {}

    // Constructor for regular chat messages
    Message(const std::string& sender, const std::string& content)
        : senderId(sender), body(content), type(MessageType::CHAT) {
        messageId = generateUUID();
        checksum = calculateChecksum(content);
    }

    // Constructor for acknowledgment messages
    Message(const std::string& sender, const std::string& originalMsgId, bool isPositive)
        : senderId(sender), type(isPositive ? MessageType::ACK : MessageType::NACK) {
        messageId = generateUUID();
        originalMessageId = originalMsgId;
        body = isPositive ? "Message received" : "Message validation failed";
        checksum = calculateChecksum(body);
    }

    // Generate a simple UUID-like string (for compatibility with Teflon)
    static std::string generateUUID();

    // Calculate CRC32 checksum for message body
    static uint32_t calculateChecksum(const std::string& content);

    // Check if this message is an acknowledgment
    bool isAcknowledgment() const {
        return type == MessageType::ACK || type == MessageType::NACK;
    }

    // Convert MessageType enum to string for JSON
    static std::string messageTypeToString(MessageType type);

    // Convert string to MessageType enum from JSON
    static MessageType stringToMessageType(const std::string& typeStr);
};

// JSON serialization functions are now in JsonUtils.cpp using yyjson
