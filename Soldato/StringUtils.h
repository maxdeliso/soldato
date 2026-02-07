#pragma once

#include <string>
#include <windows.h>

namespace StringUtils {

  // Optimized version: pre-allocate based on UTF-8->UTF-16 conversion rule
  // UTF-8 can have 1-4 bytes per character, UTF-16 uses 1-2 code units per character
  // Worst case: 4-byte UTF-8 -> 2 UTF-16 code units, so we can pre-allocate: str.length() * 2
  inline std::wstring to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    // Pre-allocate based on the worst-case UTF-8 to UTF-16 expansion
    size_t prealloc_size = str.length() * 2; // Worst case: each byte could be part of a 4-byte UTF-8 sequence
    std::wstring wstrTo(prealloc_size, 0);

    int result = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
      &wstrTo[0], (int)prealloc_size);

    if (result == 0) {
      // Fallback to the safe method if pre-allocation wasn't enough
      int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
      wstrTo.resize(size_needed);
      MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
      return wstrTo;
    }

    // Resize to actual length
    wstrTo.resize(result);
    return wstrTo;
  }

  // Optimized version: pre-allocate based on UTF-16->UTF-8 expansion rule
  // Each UTF-16 character (2 bytes) can expand to max 3 UTF-8 bytes
  inline std::string to_string(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    // Pre-allocate based on the 2-byte UTF-16 -> max 3-byte UTF-8 rule
    size_t prealloc_size = wstr.length() * 3; // No need for +1 since we're not using null terminator
    std::string strTo(prealloc_size, 0);

    int result = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
      &strTo[0], (int)prealloc_size, NULL, NULL);

    if (result == 0) {
      // Fallback to the safe method if pre-allocation wasn't enough
      int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
      strTo.resize(size_needed);
      WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
      return strTo;
    }

    // Resize to actual length
    strTo.resize(result);
    return strTo;
  }

} // namespace StringUtils
