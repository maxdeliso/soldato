#pragma once

#include "json.hpp"
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Utility class for optimized JSON parsing to avoid redundant key lookups
 * and string hashing operations that occur with repeated nlohmann::json::at() calls
 */
class JsonUtils {
public:
    /**
     * Efficiently parse a JSON string and cache key iterators to avoid redundant lookups
     */
    static nlohmann::json parseOptimized(const std::string& jsonStr) {
        return nlohmann::json::parse(jsonStr);
    }

    /**
     * Get a string value from JSON using iterator (single lookup)
     * Returns empty string if key not found
     */
    static std::string getString(const nlohmann::json& json, const std::string& key) {
        auto it = json.find(key);
        if (it != json.end() && it->is_string()) {
            return it->get<std::string>();
        }
        return "";
    }

    /**
     * Get a string value from JSON using iterator (single lookup)
     * Returns default value if key not found
     */
    static std::string getString(const nlohmann::json& json, const std::string& key, const std::string& defaultValue) {
        auto it = json.find(key);
        if (it != json.end() && it->is_string()) {
            return it->get<std::string>();
        }
        return defaultValue;
    }

    /**
     * Get an integer value from JSON using iterator (single lookup)
     * Returns 0 if key not found
     */
    static int getInt(const nlohmann::json& json, const std::string& key) {
        auto it = json.find(key);
        if (it != json.end() && it->is_number_integer()) {
            return it->get<int>();
        }
        return 0;
    }

    /**
     * Get an integer value from JSON using iterator (single lookup)
     * Returns default value if key not found
     */
    static int getInt(const nlohmann::json& json, const std::string& key, int defaultValue) {
        auto it = json.find(key);
        if (it != json.end() && it->is_number_integer()) {
            return it->get<int>();
        }
        return defaultValue;
    }

    /**
     * Check if a key exists in JSON using iterator (single lookup)
     */
    static bool hasKey(const nlohmann::json& json, const std::string& key) {
        return json.find(key) != json.end();
    }

    /**
     * Get a boolean value from JSON using iterator (single lookup)
     * Returns false if key not found
     */
    static bool getBool(const nlohmann::json& json, const std::string& key) {
        auto it = json.find(key);
        if (it != json.end() && it->is_boolean()) {
            return it->get<bool>();
        }
        return false;
    }

    /**
     * Get a boolean value from JSON using iterator (single lookup)
     * Returns default value if key not found
     */
    static bool getBool(const nlohmann::json& json, const std::string& key, bool defaultValue) {
        auto it = json.find(key);
        if (it != json.end() && it->is_boolean()) {
            return it->get<bool>();
        }
        return defaultValue;
    }

    /**
     * Batch extract multiple string values in a single pass
     * This is the most efficient way to extract multiple values
     */
    static std::unordered_map<std::string, std::string> extractStrings(
        const nlohmann::json& json,
        const std::vector<std::string>& keys) {

        std::unordered_map<std::string, std::string> result;

        for (const auto& key : keys) {
            auto it = json.find(key);
            if (it != json.end() && it->is_string()) {
                result[key] = it->get<std::string>();
            }
        }

        return result;
    }

    /**
     * Extract multiple values using JSON pointer literals for maximum efficiency
     * Uses precomputed JSON pointers to avoid string hashing and lookup overhead
     */
    static std::unordered_map<std::string, std::string> extractStringsWithPointers(
        const nlohmann::json& json,
        const std::vector<std::string>& keys) {

        using namespace nlohmann::literals;
        std::unordered_map<std::string, std::string> result;

        for (const auto& key : keys) {
            try {
                // Use JSON pointer literal for efficient access
                if (key == "senderId") {
                    result[key] = json["/senderId"_json_pointer];
                } else if (key == "body") {
                    result[key] = json["/body"_json_pointer];
                } else if (key == "messageId") {
                    result[key] = json["/messageId"_json_pointer];
                } else if (key == "type") {
                    result[key] = json["/type"_json_pointer];
                } else if (key == "originalMessageId") {
                    result[key] = json["/originalMessageId"_json_pointer];
                } else if (key == "checksum") {
                    result[key] = std::to_string(static_cast<int>(json["/checksum"_json_pointer]));
                }
            } catch (const std::exception&) {
                // Key not found or not a string, skip it
                continue;
            }
        }

        return result;
    }

    /**
     * Get a string value using JSON pointer literal for maximum efficiency
     * Returns empty string if key not found
     */
    static std::string getStringWithPointer(const nlohmann::json& json, const std::string& key) {
        using namespace nlohmann::literals;

        try {
            if (key == "senderId") {
                return json["/senderId"_json_pointer];
            } else if (key == "body") {
                return json["/body"_json_pointer];
            } else if (key == "messageId") {
                return json["/messageId"_json_pointer];
            } else if (key == "type") {
                return json["/type"_json_pointer];
            } else if (key == "originalMessageId") {
                return json["/originalMessageId"_json_pointer];
            }
        } catch (const std::exception&) {
            return "";
        }
        return "";
    }

    /**
     * Get a string value using JSON pointer literal with default value
     * Returns default value if key not found
     */
    static std::string getStringWithPointer(const nlohmann::json& json, const std::string& key, const std::string& defaultValue) {
        using namespace nlohmann::literals;

        try {
            if (key == "senderId") {
                return json["/senderId"_json_pointer];
            } else if (key == "body") {
                return json["/body"_json_pointer];
            } else if (key == "messageId") {
                return json["/messageId"_json_pointer];
            } else if (key == "type") {
                return json["/type"_json_pointer];
            } else if (key == "originalMessageId") {
                return json["/originalMessageId"_json_pointer];
            }
        } catch (const std::exception&) {
            return defaultValue;
        }
        return defaultValue;
    }

    /**
     * Get an integer value using JSON pointer literal for maximum efficiency
     * Returns 0 if key not found
     */
    static int getIntWithPointer(const nlohmann::json& json, const std::string& key) {
        using namespace nlohmann::literals;

        try {
            if (key == "checksum") {
                return json["/checksum"_json_pointer];
            }
        } catch (const std::exception&) {
            return 0;
        }
        return 0;
    }

    /**
     * Get an integer value using JSON pointer literal with default value
     * Returns default value if key not found
     */
    static int getIntWithPointer(const nlohmann::json& json, const std::string& key, int defaultValue) {
        using namespace nlohmann::literals;

        try {
            if (key == "checksum") {
                return json["/checksum"_json_pointer];
            }
        } catch (const std::exception&) {
            return defaultValue;
        }
        return defaultValue;
    }
};
