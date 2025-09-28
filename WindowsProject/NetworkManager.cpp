#include "framework.h"
#include "NetworkManager.h"
#include <iostream>
#include <sstream>

NetworkManager* NetworkManager::s_instance = nullptr;
bool NetworkManager::s_initialized = false;

NetworkManager::NetworkManager()
    : m_socket(INVALID_SOCKET)
    , m_connected(false)
    , m_port(0)
    , m_socketEvent(WSA_INVALID_EVENT)
    , m_shutdownEvent(WSA_INVALID_EVENT)
    , m_eventThread(nullptr)
    , m_threadRunning(false)
    , m_messageCallback(nullptr)
    , m_socketEventCallback(nullptr)
{
    InitializeWinsock();
}

NetworkManager::~NetworkManager()
{
    Disconnect();
    CleanupSocketEvents();
    CleanupWinsock();
}

NetworkManager* NetworkManager::GetInstance()
{
    if (!s_instance)
    {
        s_instance = new NetworkManager();
    }
    return s_instance;
}

void NetworkManager::DestroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool NetworkManager::InitializeWinsock()
{
    if (!s_initialized)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            return false;
        }
        s_initialized = true;
    }
    return true;
}

void NetworkManager::CleanupWinsock()
{
    if (s_initialized)
    {
        WSACleanup();
        s_initialized = false;
    }
}

bool NetworkManager::SetupSocketEvents()
{
    // Create socket event object
    m_socketEvent = WSACreateEvent();
    if (m_socketEvent == WSA_INVALID_EVENT)
    {
        return false;
    }

    // Create shutdown event object
    m_shutdownEvent = WSACreateEvent();
    if (m_shutdownEvent == WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_socketEvent);
        m_socketEvent = WSA_INVALID_EVENT;
        return false;
    }

    return true;
}

void NetworkManager::CleanupSocketEvents()
{
    if (m_socketEvent != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_socketEvent);
        m_socketEvent = WSA_INVALID_EVENT;
    }

    if (m_shutdownEvent != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_shutdownEvent);
        m_shutdownEvent = WSA_INVALID_EVENT;
    }
}

bool NetworkManager::Connect(const std::string& multicastIP, int port, const std::string& username)
{
    if (m_connected)
    {
        Disconnect();
    }

    m_multicastIP = multicastIP;
    m_port = port;
    m_username = username;

    // Setup socket events
    if (!SetupSocketEvents())
    {
        return false;
    }

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        CleanupSocketEvents();
        return false;
    }

    // Set socket to allow reuse of address
    int reuse = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    // Bind to local address
    sockaddr_in localAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

    if (bind(m_socket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    // Set up multicast address
    m_multicastAddr.sin_family = AF_INET;
    m_multicastAddr.sin_port = htons(port);
    inet_pton(AF_INET, multicastIP.c_str(), &m_multicastAddr.sin_addr);

    // Join multicast group
    ip_mreq multicastRequest = {};
    inet_pton(AF_INET, multicastIP.c_str(), &multicastRequest.imr_multiaddr);
    multicastRequest.imr_interface.s_addr = INADDR_ANY;

    if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&multicastRequest, sizeof(multicastRequest)) == SOCKET_ERROR)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    // Associate socket with event object for read events
    if (WSAEventSelect(m_socket, m_socketEvent, FD_READ) == SOCKET_ERROR)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    m_connected = true;

    // Start event monitoring thread
    m_threadRunning = true;
    m_eventThread = CreateThread(nullptr, 0, EventThread, this, 0, nullptr);

    // Notify connection established
    if (m_socketEventCallback)
    {
        m_socketEventCallback(1, "Connected to " + multicastIP + ":" + std::to_string(port));
    }

    // Debug: Add a test message to verify the callback system
    if (m_messageCallback)
    {
        m_messageCallback("System", "Network connection established");
    }

    return true;
}

void NetworkManager::Disconnect()
{
    if (m_connected)
    {
        m_threadRunning = false;

        // Signal shutdown event
        if (m_shutdownEvent != WSA_INVALID_EVENT)
        {
            WSASetEvent(m_shutdownEvent);
        }

        // Wait for event thread to finish
        if (m_eventThread)
        {
            WaitForSingleObject(m_eventThread, 2000);
            CloseHandle(m_eventThread);
            m_eventThread = nullptr;
        }

        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        CleanupSocketEvents();
        m_connected = false;

        // Notify disconnection
        if (m_socketEventCallback)
        {
            m_socketEventCallback(0, "Disconnected from network");
        }
    }
}

bool NetworkManager::SendMessage(const std::string& message)
{
    if (!m_connected || m_socket == INVALID_SOCKET)
    {
        return false;
    }

    std::string formattedMessage = "[" + m_username + "]: " + message;

    int result = sendto(m_socket, formattedMessage.c_str(), (int)formattedMessage.length(), 0,
                       (sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result != SOCKET_ERROR)
    {
        // Notify message sent
        if (m_socketEventCallback)
        {
            m_socketEventCallback(2, "Message sent: " + message);
        }
        return true;
    }

    return false;
}

DWORD WINAPI NetworkManager::EventThread(LPVOID lpParam)
{
    NetworkManager* manager = static_cast<NetworkManager*>(lpParam);
    WSAEVENT events[2] = { manager->m_socketEvent, manager->m_shutdownEvent };

    while (manager->m_threadRunning)
    {
        DWORD result = WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE);

        if (result == WSA_WAIT_EVENT_0) // Socket event
        {
            manager->HandleSocketEvent(manager->m_socketEvent);
        }
        else if (result == WSA_WAIT_EVENT_0 + 1) // Shutdown event
        {
            break;
        }
        else if (result == WSA_WAIT_FAILED)
        {
            // Handle error
            if (manager->m_socketEventCallback)
            {
                manager->m_socketEventCallback(-1, "Socket event wait failed");
            }
            break;
        }
    }

    return 0;
}

void NetworkManager::HandleSocketEvent(WSAEVENT event)
{
    WSANETWORKEVENTS networkEvents;
    if (WSAEnumNetworkEvents(m_socket, event, &networkEvents) == SOCKET_ERROR)
    {
        if (m_socketEventCallback)
        {
            m_socketEventCallback(-1, "Failed to enumerate network events");
        }
        return;
    }

    if (networkEvents.lNetworkEvents & FD_READ)
    {
        // Debug: Notify that we received a read event
        if (m_socketEventCallback)
        {
            m_socketEventCallback(5, "FD_READ event received");
        }
        ProcessSocketData();
    }

    if (networkEvents.lNetworkEvents & FD_CLOSE)
    {
        if (m_socketEventCallback)
        {
            m_socketEventCallback(3, "Socket closed by remote");
        }
    }
}

void NetworkManager::ProcessSocketData()
{
    char buffer[1024];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&fromAddr, &fromLen);

    if (bytesReceived > 0)
    {
        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        // Debug: Notify raw message received
        if (m_socketEventCallback)
        {
            m_socketEventCallback(6, "Raw message: " + message);
        }

        // Parse sender and message
        size_t colonPos = message.find(": ");
        if (colonPos != std::string::npos)
        {
            std::string sender = message.substr(1, colonPos - 1); // Remove [ and ]
            std::string msg = message.substr(colonPos + 2);

            if (m_messageCallback)
            {
                m_messageCallback(sender, msg);
            }

            // Notify message received
            if (m_socketEventCallback)
            {
                m_socketEventCallback(4, "Message received from " + sender);
            }
        }
        else
        {
            // Debug: Message format issue
            if (m_socketEventCallback)
            {
                m_socketEventCallback(7, "Message format error: " + message);
            }
        }
    }
    else if (bytesReceived == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            if (m_socketEventCallback)
            {
                m_socketEventCallback(-1, "Socket receive error: " + std::to_string(error));
            }
        }
    }
}
