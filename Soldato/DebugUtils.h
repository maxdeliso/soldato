#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <windows.h>

class DebugLogger {
private:
    static std::ofstream m_logFile;
    static std::mutex m_mutex;
    static bool m_initialized;

    static void Initialize() {
        if (!m_initialized) {
            m_logFile.open("soldato_debug.log", std::ios::app);
            m_initialized = true;
        }
    }

    static std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

public:
    static void Log(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Initialize();

        if (m_logFile.is_open()) {
            m_logFile << "[" << GetTimestamp() << "] " << message << std::endl;
            m_logFile.flush();
        }

        // Also output to debug console in debug builds
        #ifdef _DEBUG
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
        #endif
    }

    static void Log(const std::wstring& message) {
        // Convert wide string to narrow string for logging
        std::string narrowMessage;
        narrowMessage.reserve(message.length());
        for (wchar_t wc : message) {
            narrowMessage += static_cast<char>(wc);
        }
        Log(narrowMessage);
    }

    // Optimized version for string literals with compile-time length
    template<size_t N>
    static void Log(const char(&message)[N]) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Initialize();

        if (m_logFile.is_open()) {
            m_logFile << "[" << GetTimestamp() << "] " << message << std::endl;
            m_logFile.flush();
        }

        // Also output to debug console in debug builds
        #ifdef _DEBUG
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
        #endif
    }

    // Optimized version for wide string literals with compile-time length
    template<size_t N>
    static void Log(const wchar_t(&message)[N]) {
        // Convert wide string literal to narrow string for logging
        std::string narrowMessage;
        narrowMessage.reserve(N - 1); // -1 to exclude null terminator
        for (size_t i = 0; i < N - 1; ++i) {
            narrowMessage += static_cast<char>(message[i]);
        }
        Log(narrowMessage);
    }
};

#ifdef _DEBUG
    #define DEBUG_LOG(msg) DebugLogger::Log(msg)
#else
    #define DEBUG_LOG(msg) ((void)0)  // Compile out in release builds
#endif
