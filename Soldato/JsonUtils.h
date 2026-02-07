#pragma once

#include "yyjson.h"
#include "Message.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

class JsonUtils {
public:

  /**
   * Serialize a Message object to JSON string using yyjson
   * This replaces the nlohmann::json to_json function
   */
  static std::string SerializeMessage(const Message& msg);

  /**
   * Deserialize a JSON string to Message object using yyjson
   * This replaces the nlohmann::json from_json function
   */
  static std::optional<Message> DeserializeMessage(const std::string& json_str);
};
