#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// RAII wrapper for Winsock initialization
class WinsockManager {
public:
    WinsockManager() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            // In a real application, you might want to throw an exception here
            // For now, we'll just mark it as failed
            m_initialized = false;
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
