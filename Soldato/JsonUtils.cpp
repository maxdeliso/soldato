#include "JsonUtils.h"
#include "Message.h"
#include <cstdlib>
#include <windows.h>

// yyjson-based serialization function
std::string JsonUtils::SerializeMessage(const Message& msg) {
    // 1. Create a mutable document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    // 2. Add key-value pairs for each member (using strcpy to avoid lifetime issues)
    yyjson_mut_obj_add_strcpy(doc, root, "senderId", msg.senderId.c_str());
    yyjson_mut_obj_add_strcpy(doc, root, "body", msg.body.c_str());
    yyjson_mut_obj_add_strcpy(doc, root, "messageId", msg.messageId.c_str());

    yyjson_mut_obj_add_strcpy(doc, root, "type", Message::messageTypeToString(msg.type).c_str());
    yyjson_mut_obj_add_uint(doc, root, "checksum", msg.checksum);

    // Handle optional fields
    if (msg.originalMessageId.has_value()) {
        yyjson_mut_obj_add_strcpy(doc, root, "originalMessageId", msg.originalMessageId->c_str());
    }

    // 3. Write to a string (yyjson manages memory for the write)
    const char *json_c_str = yyjson_mut_write(doc, 0, nullptr);
    std::string json_string(json_c_str);

    // 4. Clean up
    free((void *)json_c_str);
    yyjson_mut_doc_free(doc);

    return json_string;
}

// yyjson-based deserialization function
std::optional<Message> JsonUtils::DeserializeMessage(const std::string& json_str) {
    // 1. Read the JSON string into an immutable document
    yyjson_doc *doc = yyjson_read(json_str.c_str(), json_str.length(), 0);
    if (!doc) {
        return std::nullopt; // Parse failed
    }
    yyjson_val *root = yyjson_doc_get_root(doc);

    // 2. Extract values safely
    yyjson_val *senderId_val = yyjson_obj_get(root, "senderId");
    yyjson_val *body_val = yyjson_obj_get(root, "body");
    yyjson_val *messageId_val = yyjson_obj_get(root, "messageId");
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    yyjson_val *checksum_val = yyjson_obj_get(root, "checksum");

    // Basic validation
    if (!senderId_val || !body_val || !messageId_val || !type_val || !checksum_val) {
        yyjson_doc_free(doc);
        return std::nullopt;
    }

    Message msg;
    msg.senderId = yyjson_get_str(senderId_val);
    msg.body = yyjson_get_str(body_val);
    msg.messageId = yyjson_get_str(messageId_val);

    // Handle type conversion
    std::string typeStr = yyjson_get_str(type_val);
    if (!typeStr.empty()) {
        msg.type = Message::stringToMessageType(typeStr);
    }

    // Extract checksum (unsigned integer)
    msg.checksum = static_cast<uint32_t>(yyjson_get_uint(checksum_val));

    // Handle optional fields
    yyjson_val *orig_id_val = yyjson_obj_get(root, "originalMessageId");
    if (orig_id_val) {
        msg.originalMessageId = yyjson_get_str(orig_id_val);
    } else {
        msg.originalMessageId = std::nullopt;
    }

    // 3. Clean up
    yyjson_doc_free(doc);

    return msg;
}
