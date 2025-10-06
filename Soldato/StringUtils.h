#pragma once

#include <string>
#include <windows.h>

// Utility functions for proper UTF-8 string conversion
namespace StringUtils {

    // Converts a UTF-8 std::string to a std::wstring
    inline std::wstring to_wstring(const std::string& str) {
        if (str.empty()) return std::wstring();
        // UTF-8 to UTF-16: worst case is 1:1 ratio (ASCII), but we need to account for multi-byte UTF-8
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    // Converts a std::wstring to a UTF-8 std::string
    inline std::string to_string(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        // UTF-16 to UTF-8: each 2-byte UTF-16 char can expand to max 3 UTF-8 bytes
        // So we can pre-allocate: wstr.length() * 3 + 1 for null terminator
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    // Optimized version: pre-allocate based on UTF-16->UTF-8 expansion rule
    // Each UTF-16 character (2 bytes) can expand to max 3 UTF-8 bytes
    inline std::string to_string_optimized(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();

        // Pre-allocate based on the 2-byte UTF-16 -> max 3-byte UTF-8 rule
        size_t prealloc_size = wstr.length() * 3 + 1; // +1 for null terminator
        std::string strTo(prealloc_size, 0);

        int result = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                       &strTo[0], (int)prealloc_size, NULL, NULL);

        if (result == 0) {
            // Fallback to the safe method if pre-allocation wasn't enough
            return to_string(wstr);
        }

        // Resize to actual length (result includes null terminator)
        strTo.resize(result);
        return strTo;
    }

} // namespace StringUtils
