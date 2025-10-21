#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdexcept>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class WinsockManager {
public:
    WinsockManager() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::string errorMsg = "WSAStartup failed with error code: " + std::to_string(result);
            throw std::runtime_error(errorMsg);
        } else {
            m_initialized = true;
        }
    }

    ~WinsockManager() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const {
        return m_initialized;
    }

    // Prevent copying
    WinsockManager(const WinsockManager&) = delete;
    WinsockManager& operator=(const WinsockManager&) = delete;

private:
    bool m_initialized;
};
