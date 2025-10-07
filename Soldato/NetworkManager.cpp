#include "framework.h"
#include "NetworkManager.h"
#include "ChatForm.h"  // Include for custom message definitions
#include <iostream>
#include <sstream>
#include <ctime>

// Helper function to get a descriptive error string
void LogWinsockError(const std::string& context) {
    int error_code = WSAGetLastError();
    char* msg_buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg_buf, 0, NULL);

    std::string error_message = context + " failed with error " + std::to_string(error_code);
    if (msg_buf) {
        error_message += ": " + std::string(msg_buf);
        LocalFree(msg_buf);
    }

    // Use OutputDebugStringA for logging
    OutputDebugStringA(error_message.c_str());
}

WinsockManager NetworkManager::s_winsockManager;

NetworkManager::NetworkManager()
    : m_socket(INVALID_SOCKET)
    , m_connected(false)
    , m_port(0)
    , m_socketEvent(NULL)
    , m_shutdownEvent(NULL)
    , m_threadRunning(false)
    , m_hNotifyWnd(NULL)
    , m_messageTracker(nullptr)
    , m_peerTracker(nullptr)
    , m_sendWorkerRunning(false)
{
    // Winsock is now managed by the RAII wrapper
}

NetworkManager::~NetworkManager()
{
    Disconnect();
    CleanupSocketEvents();
    // Winsock cleanup is handled by the RAII wrapper
}

NetworkManager& NetworkManager::GetInstance()
{
    static NetworkManager instance; // Created on first call, destroyed at exit

    // Ensure Winsock is initialized
    if (!s_winsockManager.IsInitialized()) {
        // This should not happen, but if it does, we need to handle it
        OutputDebugStringA("NetworkManager: Winsock initialization failed!\n");
    }

    return instance;
}

void NetworkManager::SetMessageCallback(MessageCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = callback;
}

void NetworkManager::ClearMessageCallback()
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = nullptr;
}

void NetworkManager::SetSocketEventCallback(SocketEventCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_socketEventCallback = callback;
}

void NetworkManager::ClearSocketEventCallback()
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_socketEventCallback = nullptr;
}

void NetworkManager::SetNotificationWindow(HWND hWnd)
{
    std::lock_guard<std::mutex> lock(m_memberMutex);
    m_hNotifyWnd = hWnd;
}

bool NetworkManager::SetupSocketEvents()
{
    // Create socket event object
    m_socketEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_socketEvent == NULL)
    {
        return false;
    }

    // Create shutdown event object
    m_shutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_shutdownEvent == NULL)
    {
        CloseHandle(m_socketEvent);
        m_socketEvent = NULL;
        return false;
    }

    return true;
}

void NetworkManager::CleanupSocketEvents()
{
    if (m_socketEvent != NULL)
    {
        CloseHandle(m_socketEvent);
        m_socketEvent = NULL;
    }

    if (m_shutdownEvent != NULL)
    {
        CloseHandle(m_shutdownEvent);
        m_shutdownEvent = NULL;
    }
}

bool NetworkManager::Connect(const std::string& multicastIP, int port, const std::string& username)
{
    // Fail fast if already connected.
    if (m_connected.load()) {
        OutputDebugStringA("Connect() called while already connected. Ignoring.");
        return false;
    }

    // Protect member variable access with mutex
    std::lock_guard<std::mutex> lock(m_memberMutex);

    m_multicastIP = multicastIP;
    m_port = port;
    m_username = username;
    m_senderId = Message::generateUUID();  // Generate UUID for Teflon compatibility

    // Initialize message tracker with our sender ID
    m_messageTracker = std::make_unique<MessageTracker>(m_senderId);

    // Initialize peer tracker with our sender ID
    m_peerTracker = std::make_unique<PeerTracker>(m_senderId);

    // Setup socket events
    if (!SetupSocketEvents())
    {
        return false;
    }

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        LogWinsockError("socket()");
        CleanupSocketEvents();
        return false;
    }

    // Set socket to allow reuse of address
    int reuse = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR)
    {
        LogWinsockError("setsockopt(SO_REUSEADDR)");
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
        LogWinsockError("bind()");
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
        LogWinsockError("setsockopt(IP_ADD_MEMBERSHIP)");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    // Associate socket with event object for read events
    if (WSAEventSelect(m_socket, m_socketEvent, FD_READ) == SOCKET_ERROR)
    {
        LogWinsockError("WSAEventSelect()");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    m_connected.store(true);

    // Start event monitoring thread
    m_threadRunning = true;

    // Ensure the old event thread is properly stopped before creating a new one
    if (m_eventThread.joinable()) {
        m_threadRunning = false;
        if (m_shutdownEvent != NULL) {
            SetEvent(m_shutdownEvent);
        }
        m_eventThread.join();
    }

    // Reset thread running flag and create new event thread
    m_threadRunning = true;
    OutputDebugStringA("Connect: Creating new event thread\n");
    m_eventThread = std::thread(&NetworkManager::EventThreadFunction, this);
    OutputDebugStringA("Connect: Event thread created\n");

    // Start send worker thread
    m_sendWorkerRunning = true;
    OutputDebugStringA("Connect: Set m_sendWorkerRunning = true\n");

    // Clear any old messages in the send queue
    {
        std::lock_guard<std::mutex> lock(m_sendQueueMutex);
        while (!m_sendQueue.empty()) {
            m_sendQueue.pop();
        }
    }

    // Ensure the old thread is properly cleaned up before creating a new one
    if (m_sendWorkerThread.joinable()) {
        m_sendWorkerThread.join();
    }

    m_sendWorkerThread = std::thread(&NetworkManager::SendWorkerThreadFunction, this);

    // Notify connection established
    SocketEventCallback socketCallback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        socketCallback = m_socketEventCallback;
    }
    if (socketCallback)
    {
        socketCallback(1, "Connected to " + multicastIP + ":" + std::to_string(port));
    }

    // Debug: Add a test message to verify the callback system
    MessageCallback messageCallback;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        messageCallback = m_messageCallback;
    }
    if (messageCallback)
    {
        messageCallback("System", "Network connection established");
    }

    return true;
}

void NetworkManager::Disconnect()
{
    if (m_connected.load())
    {
        // Clear callbacks first to prevent race conditions during shutdown
        ClearMessageCallback();
        ClearSocketEventCallback();

        m_threadRunning = false;

        // Stop send worker thread
        {
            std::lock_guard<std::mutex> lock(m_sendQueueMutex);
            m_sendWorkerRunning = false;
            OutputDebugStringA("Disconnect: Set m_sendWorkerRunning = false\n");
        }
        m_sendQueueCondition.notify_all();

        if (m_sendWorkerThread.joinable())
        {
            m_sendWorkerThread.join();
        }

        // Signal shutdown event
        if (m_shutdownEvent != NULL)
        {
            SetEvent(m_shutdownEvent);
        }

        // Wait for event thread to finish
        if (m_eventThread.joinable())
        {
            m_eventThread.join();
        }

        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        CleanupSocketEvents();
        m_connected.store(false);

        // Shutdown message tracker
        if (m_messageTracker) {
            m_messageTracker->shutdown();
            m_messageTracker.reset();
        }

        // Shutdown peer tracker
        if (m_peerTracker) {
            m_peerTracker->shutdown();
            m_peerTracker.reset();
        }
    }
}

bool NetworkManager::SendMessage(const std::string& message)
{
    OutputDebugStringA(("SendMessage: Called with message: " + message + "\n").c_str());

    if (!m_connected.load() || m_socket == INVALID_SOCKET)
    {
        OutputDebugStringA("SendMessage: Blocked - not connected or invalid socket\n");
        return false;
    }

    OutputDebugStringA("SendMessage: Creating Teflon message\n");
    // Create Teflon-compatible message
    Message teflonMessage = CreateChatMessage(m_senderId, message);

    // Track the message for ACK/NACK
    if (m_messageTracker) {
        OutputDebugStringA("SendMessage: Tracking message\n");
        m_messageTracker->trackMessage(teflonMessage);
    }

    // Serialize message as JSON
    OutputDebugStringA("SendMessage: Serializing message\n");
    std::string jsonMessage = SerializeMessage(teflonMessage);

    OutputDebugStringA(("SendMessage: Calling sendto with " + std::to_string(jsonMessage.length()) + " bytes\n").c_str());
    int result = sendto(m_socket, jsonMessage.c_str(), (int)jsonMessage.length(), 0,
                       (sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result != SOCKET_ERROR)
    {
        OutputDebugStringA("SendMessage: sendto succeeded\n");
        // Notify message sent
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            OutputDebugStringA("SendMessage: Calling socket callback\n");
            socketCallback(2, "Message sent: " + message);
        }
        else
        {
            OutputDebugStringA("SendMessage: socketCallback is null\n");
        }
        return true;
    }
    else
    {
        OutputDebugStringA(("SendMessage: sendto failed with error: " + std::to_string(WSAGetLastError()) + "\n").c_str());
    }

    return false;
}

void NetworkManager::SendMessageAsync(const std::string& message)
{
    OutputDebugStringA(("SendMessageAsync: Called with message: " + message + "\n").c_str());

    if (!m_connected.load() || m_socket == INVALID_SOCKET)
    {
        // Debug: Log why we're not sending
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback) {
            std::string reason = !m_connected.load() ? "not connected" : "invalid socket";
            socketCallback(9, "SendMessageAsync blocked: " + reason);
        }
        OutputDebugStringA("SendMessageAsync: Blocked - not connected\n");
        return;
    }

    // Add message to queue for async processing
    {
        std::lock_guard<std::mutex> lock(m_sendQueueMutex);
        m_sendQueue.push(message);
        OutputDebugStringA(("SendMessageAsync: Message queued, queue size: " + std::to_string(m_sendQueue.size()) + "\n").c_str());
    }
    m_sendQueueCondition.notify_one();
    OutputDebugStringA("SendMessageAsync: Notified worker thread\n");
}

void NetworkManager::SendWorkerThreadFunction()
{
    // Simple debug: Log that thread started
    OutputDebugStringA("SendWorkerThreadFunction: Thread started\n");

    while (m_sendWorkerRunning)
    {
        OutputDebugStringA("SendWorkerThreadFunction: Starting loop iteration\n");
        std::string message;

        // Wait for messages in queue
        {
            std::unique_lock<std::mutex> lock(m_sendQueueMutex);
            m_sendQueueCondition.wait(lock, [this] {
                return !m_sendQueue.empty() || !m_sendWorkerRunning;
            });

            if (!m_sendWorkerRunning) {
                OutputDebugStringA("SendWorkerThreadFunction: Exiting because m_sendWorkerRunning is false\n");
                break;
            }

            if (!m_sendQueue.empty())
            {
                message = m_sendQueue.front();
                m_sendQueue.pop();
                OutputDebugStringA(("SendWorkerThreadFunction: Processing message: " + message + "\n").c_str());
            }
        }

        if (!message.empty())
        {
            // Process the message (this is now on a background thread)
            SendMessage(message);
            OutputDebugStringA("SendWorkerThreadFunction: Message processing completed\n");
        }

        OutputDebugStringA("SendWorkerThreadFunction: End of loop iteration, m_sendWorkerRunning = ");
        OutputDebugStringA(m_sendWorkerRunning.load() ? "true" : "false");
        OutputDebugStringA("\n");
    }

    OutputDebugStringA("SendWorkerThreadFunction: Thread exiting\n");
}

void NetworkManager::EventThreadFunction()
{
    OutputDebugStringA("EventThread: Thread started\n");
    WSAEVENT events[2] = { m_socketEvent, m_shutdownEvent };

    while (m_threadRunning)
    {
        DWORD result = WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE);

        if (result == WSA_WAIT_EVENT_0) // Socket event
        {
            HandleSocketEvent(m_socketEvent);
        }
        else if (result == WSA_WAIT_EVENT_0 + 1) // Shutdown event
        {
            break;
        }
        else if (result == WSA_WAIT_FAILED)
        {
            // Handle error
            SocketEventCallback socketCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                socketCallback = m_socketEventCallback;
            }
            if (socketCallback)
            {
                socketCallback(-1, "Socket event wait failed");
            }
            break;
        }
    }

    OutputDebugStringA("EventThread: Thread exiting\n");
}

void NetworkManager::HandleSocketEvent(WSAEVENT event)
{
    WSANETWORKEVENTS networkEvents;
    if (WSAEnumNetworkEvents(m_socket, event, &networkEvents) == SOCKET_ERROR)
    {
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            socketCallback(-1, "Failed to enumerate network events");
        }
        return;
    }

    if (networkEvents.lNetworkEvents & FD_READ)
    {
        // Debug: Notify that we received a read event
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            socketCallback(5, "FD_READ event received");
        }
        ProcessSocketData();
    }

    if (networkEvents.lNetworkEvents & FD_CLOSE)
    {
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            socketCallback(3, "Socket closed by remote");
        }
    }
}

void NetworkManager::ProcessSocketData()
{
    // Use a larger buffer to handle larger UDP packets (max UDP payload size)
    constexpr int MAX_UDP_SIZE = 65507;
    char buffer[MAX_UDP_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    int bytesReceived = recvfrom(m_socket, buffer, MAX_UDP_SIZE, 0,
                               (sockaddr*)&fromAddr, &fromLen);

    if (bytesReceived > 0)
    {
        // Construct string using the exact number of bytes received
        std::string jsonData(buffer, bytesReceived);

        // Debug: Notify raw message received
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            socketCallback(6, "Raw JSON: " + jsonData);
        }

        // Try to deserialize Teflon-compatible JSON message
        Message teflonMessage;
        if (DeserializeMessage(jsonData, teflonMessage))
        {
            // Extract sender IP address for peer tracking
            char senderIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &fromAddr.sin_addr, senderIP, INET_ADDRSTRLEN);

            // Update peer tracker with sender information
            if (m_peerTracker) {
                m_peerTracker->updatePeer(teflonMessage.senderId, std::string(senderIP));

                // Notify the UI that the peer list has changed
                if (m_hNotifyWnd) {
                    PostMessage(m_hNotifyWnd, WM_APP_PEERS_UPDATED, 0, 0);
                }
            }

            // Process the incoming message (handles ACK/NACK and validation)
            ProcessIncomingMessage(teflonMessage);
        }
        else
        {
            // Fallback: try to parse as old format for backward compatibility
            size_t colonPos = jsonData.find(": ");
            if (colonPos != std::string::npos && jsonData[0] == '[')
            {
                std::string oldSender = jsonData.substr(1, colonPos - 1); // Remove [ and ]
                std::string oldMsg = jsonData.substr(colonPos + 2);

                MessageCallback messageCallback;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    messageCallback = m_messageCallback;
                }
                if (messageCallback)
                {
                    messageCallback(oldSender, oldMsg);
                }

                SocketEventCallback socketCallback;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    socketCallback = m_socketEventCallback;
                }
                if (socketCallback)
                {
                    socketCallback(4, "Legacy message received from " + oldSender);
                }
            }
            else
            {
                // Debug: Message format issue
                SocketEventCallback socketCallback;
                {
                    std::lock_guard<std::mutex> lock(m_callbackMutex);
                    socketCallback = m_socketEventCallback;
                }
                if (socketCallback)
                {
                    socketCallback(7, "JSON parse error: " + jsonData);
                }
            }
        }
    }
    else if (bytesReceived == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            SocketEventCallback socketCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                socketCallback = m_socketEventCallback;
            }
            if (socketCallback)
            {
                socketCallback(-1, "Socket receive error: " + std::to_string(error));
            }
        }
    }
}

std::string NetworkManager::SerializeMessage(const Message& message)
{
    try
    {
        nlohmann::json j = message;
        return j.dump();
    }
    catch (const std::exception&)
    {
        // Fallback to simple format if JSON serialization fails
        return "[" + message.senderId + "]: " + message.body;
    }
}

bool NetworkManager::DeserializeMessage(const std::string& jsonData, Message& message)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(jsonData);
        message = j.get<Message>();
        return true;
    }
    catch (const std::exception&)
    {
        // JSON parsing failed
        return false;
    }
}

Message NetworkManager::CreateChatMessage(const std::string& sender, const std::string& content)
{
    return Message(sender, content);
}

Message NetworkManager::CreateAcknowledgment(const std::string& sender, const std::string& originalMessageId, bool isPositive)
{
    return Message(sender, originalMessageId, isPositive);
}

void NetworkManager::SendAcknowledgment(const std::string& originalMessageId, bool isPositive)
{
    if (!m_connected.load() || m_socket == INVALID_SOCKET) {
        return;
    }

    // Create acknowledgment message
    Message ackMessage = CreateAcknowledgment(m_senderId, originalMessageId, isPositive);

    // Serialize and send
    std::string jsonMessage = SerializeMessage(ackMessage);

    int result = sendto(m_socket, jsonMessage.c_str(), (int)jsonMessage.length(), 0,
                       (sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result != SOCKET_ERROR) {
        std::string ackType = isPositive ? "ACK" : "NACK";
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback) {
            socketCallback(8, "Sent " + ackType + " for message: " + originalMessageId);
        }
    }
}

void NetworkManager::ProcessIncomingMessage(const Message& message)
{
    // Don't process our own messages
    if (message.senderId == m_senderId) {
        return;
    }

    // Validate checksum
    uint32_t calculatedChecksum = Message::calculateChecksum(message.body);
    bool checksumValid = (calculatedChecksum == message.checksum);

    // Handle different message types
    if (message.isAcknowledgment()) {
        // Process ACK/NACK
        if (m_messageTracker) {
            m_messageTracker->processAcknowledgment(message);
        }

        // Display acknowledgment in chat
        MessageCallback messageCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            messageCallback = m_messageCallback;
        }
        if (messageCallback) {
            messageCallback("System", message.body);
        }

        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback) {
            std::string ackType = (message.type == MessageType::ACK) ? "ACK" : "NACK";
            socketCallback(4, "Teflon " + ackType + " received from " + message.senderId);
        }
    } else if (message.type == MessageType::CHAT) {
        // Handle chat message
        if (checksumValid) {
            // Send ACK for valid message
            SendAcknowledgment(message.messageId, true);

            // Create a thread-safe local copy of the username
            std::string username_copy;
            {
                std::lock_guard<std::mutex> lock(m_memberMutex);
                username_copy = m_username;
            }

            // Display the message
            MessageCallback messageCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                messageCallback = m_messageCallback;
            }
            if (messageCallback) {
                messageCallback(username_copy, message.body);
            }

            SocketEventCallback socketCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                socketCallback = m_socketEventCallback;
            }
            if (socketCallback) {
                socketCallback(4, "Teflon message received from " + message.senderId);
            }
        } else {
            // Send NACK for invalid message
            SendAcknowledgment(message.messageId, false);

            SocketEventCallback socketCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                socketCallback = m_socketEventCallback;
            }
            if (socketCallback) {
                socketCallback(7, "Checksum validation failed for message from " + message.senderId);
            }
        }
    }
}

std::unordered_map<std::string, long long> NetworkManager::GetDeliveryStats() const
{
    if (m_messageTracker) {
        return m_messageTracker->getDeliveryStats();
    }
    return {};
}

std::unordered_map<std::string, PeerInfo> NetworkManager::GetKnownPeers() const
{
    if (m_peerTracker) {
        return m_peerTracker->getPeers();
    }
    return {};
}

int NetworkManager::GetPeerCount() const
{
    if (m_peerTracker) {
        return m_peerTracker->getPeerCount();
    }
    return 0;
}
