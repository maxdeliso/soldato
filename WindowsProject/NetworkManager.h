#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

class NetworkManager
{
private:
    static NetworkManager* s_instance;
    static bool s_initialized;

    SOCKET m_socket;
    sockaddr_in m_multicastAddr;
    std::string m_multicastIP;
    int m_port;
    bool m_connected;
    std::string m_username;

    // Windows event objects
    WSAEVENT m_socketEvent;
    WSAEVENT m_shutdownEvent;
    HANDLE m_eventThread;
    bool m_threadRunning;

    NetworkManager();
    ~NetworkManager();

    bool InitializeWinsock();
    void CleanupWinsock();
    bool SetupSocketEvents();
    void CleanupSocketEvents();

public:
    static NetworkManager* GetInstance();
    static void DestroyInstance();

    bool Connect(const std::string& multicastIP, int port, const std::string& username);
    void Disconnect();
    bool SendMessage(const std::string& message);
    bool IsConnected() const { return m_connected; }

    std::string GetUsername() const { return m_username; }
    std::string GetMulticastIP() const { return m_multicastIP; }
    int GetPort() const { return m_port; }

    // Callback for received messages
    typedef void (*MessageCallback)(const std::string& sender, const std::string& message);
    void SetMessageCallback(MessageCallback callback) { m_messageCallback = callback; }

    // Windows event callbacks
    typedef void (*SocketEventCallback)(int eventType, const std::string& data);
    void SetSocketEventCallback(SocketEventCallback callback) { m_socketEventCallback = callback; }

private:
    MessageCallback m_messageCallback;
    SocketEventCallback m_socketEventCallback;

    static DWORD WINAPI EventThread(LPVOID lpParam);
    void HandleSocketEvent(WSAEVENT event);
    void ProcessSocketData();
};
