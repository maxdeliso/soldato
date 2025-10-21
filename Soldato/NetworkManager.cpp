#include "framework.h"
#include "NetworkManager.h"
#include "ChatForm.h"  // Include for custom message definitions
#include "JsonUtils.h"
#include "DebugUtils.h"
#include <iostream>
#include <sstream>
#include <ctime>

// Helper function to get a descriptive error string
static void LogWinsockError(const std::string& context) {
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

    // Use DEBUG_LOG for logging
    DEBUG_LOG(error_message);
}

WinsockManager NetworkManager::s_winsockManager;

NetworkManager::NetworkManager()
    : m_socket(INVALID_SOCKET)
    , m_multicastAddr{}
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
        DEBUG_LOG("NetworkManager: Winsock initialization failed!");
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

bool NetworkManager::Connect(const std::string& multicastIP, int port)
{
    // Fail fast if already connected.
    if (m_connected.load()) {
        DEBUG_LOG("Connect() called while already connected. Ignoring.");
        return false;
    }

    // Protect member variable access with mutex
    std::lock_guard<std::mutex> lock(m_memberMutex);

    m_multicastIP = multicastIP;
    m_port = port;
    m_senderId = Message::generateUUID();  // Generate UUID for Teflon compatibility

    // Initialize message tracker with our sender ID
    m_messageTracker = std::make_unique<MessageTracker>(m_senderId);

    // Set the notification window for the message tracker
    if (m_messageTracker && m_hNotifyWnd) {
        m_messageTracker->SetNotificationWindow(m_hNotifyWnd);
    }

    // Initialize peer tracker with our sender ID
    m_peerTracker = std::make_unique<PeerTracker>(m_senderId);

    // Setup socket events
    if (!SetupSocketEvents())
    {
        return false;
    }

    // Create a dual-stack UDP socket
    m_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        LogWinsockError("socket()");
        CleanupSocketEvents();
        return false;
    }

    // Disable IPv6-only mode to enable dual-stack (support both IPv4 and IPv6)
    int no = 0;
    if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no)) == SOCKET_ERROR)
    {
        LogWinsockError("setsockopt(IPV6_V6ONLY)");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
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
    sockaddr_in6 localAddr = {};
    localAddr.sin6_family = AF_INET6;
    localAddr.sin6_addr = in6addr_any; // The IPv6 equivalent of INADDR_ANY
    localAddr.sin6_port = htons(static_cast<u_short>(port));

    if (bind(m_socket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
    {
        LogWinsockError("bind()");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        CleanupSocketEvents();
        return false;
    }

    // Set up multicast address - detect IPv4 vs IPv6
    memset(&m_multicastAddr, 0, sizeof(m_multicastAddr));

    // Try to parse as IPv6 first
    struct in6_addr testV6;
    if (inet_pton(AF_INET6, multicastIP.c_str(), &testV6) == 1)
    {
        // It's an IPv6 address
        sockaddr_in6* multicastAddrV6 = (sockaddr_in6*)&m_multicastAddr;
        multicastAddrV6->sin6_family = AF_INET6;
        multicastAddrV6->sin6_port = htons(static_cast<u_short>(port));
        multicastAddrV6->sin6_addr = testV6;

        // Join IPv6 multicast group
        ipv6_mreq multicastRequest = {};
        multicastRequest.ipv6mr_multiaddr = testV6;
        multicastRequest.ipv6mr_interface = 0; // Default interface

        if (setsockopt(m_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&multicastRequest, sizeof(multicastRequest)) == SOCKET_ERROR)
        {
            LogWinsockError("setsockopt(IPV6_ADD_MEMBERSHIP)");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            CleanupSocketEvents();
            return false;
        }
    }
    else
    {
        // Try IPv4
        sockaddr_in* multicastAddrV4 = (sockaddr_in*)&m_multicastAddr;
        multicastAddrV4->sin_family = AF_INET;
        multicastAddrV4->sin_port = htons(static_cast<u_short>(port));

        if (inet_pton(AF_INET, multicastIP.c_str(), &multicastAddrV4->sin_addr) != 1)
        {
            DEBUG_LOG("Invalid multicast IP address format");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            CleanupSocketEvents();
            return false;
        }

        // Join IPv4 multicast group
        ip_mreq multicastRequest = {};
        multicastRequest.imr_multiaddr = multicastAddrV4->sin_addr;
        multicastRequest.imr_interface.s_addr = INADDR_ANY;

        if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&multicastRequest, sizeof(multicastRequest)) == SOCKET_ERROR)
        {
            LogWinsockError("setsockopt(IP_ADD_MEMBERSHIP)");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            CleanupSocketEvents();
            return false;
        }
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
    DEBUG_LOG("Connect: Creating new event thread");
    m_eventThread = std::thread(&NetworkManager::EventThreadFunction, this);
    DEBUG_LOG("Connect: Event thread created");

    // Start send worker thread
    m_sendWorkerRunning = true;
    DEBUG_LOG("Connect: Set m_sendWorkerRunning = true");

    // Clear any old messages in the send queue
    {
        std::lock_guard<std::mutex> sendQueueLock(m_sendQueueMutex);
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
        std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
        socketCallback = m_socketEventCallback;
    }
    if (socketCallback)
    {
        socketCallback(SocketEventType::Connected, "Connected to " + multicastIP + ":" + std::to_string(port));
    }

    // Debug: Add a test message to verify the callback system
    MessageCallback messageCallback;
    {
        std::lock_guard<std::mutex> messageCallbackLock(m_callbackMutex);
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
            DEBUG_LOG("Disconnect: Set m_sendWorkerRunning = false");
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

bool NetworkManager::SendMessage(const Message& message)
{
    DEBUG_LOG("SendMessage: Called with message ID: " + message.messageId);

    if (!m_connected.load() || m_socket == INVALID_SOCKET)
    {
        DEBUG_LOG("SendMessage: Blocked - not connected or invalid socket");
        return false;
    }

    // Create a copy and ensure senderId is set correctly
    Message teflonMessage = message;
    teflonMessage.senderId = m_senderId; // Ensure sender ID is our actual UUID
    DEBUG_LOG("SendMessage: Using Message object, senderId: " + teflonMessage.senderId);

    // Track the message for ACK/NACK (lightweight - only UUID and sender)
    if (m_messageTracker) {
        DEBUG_LOG("SendMessage: Tracking message");
        m_messageTracker->trackMessage(teflonMessage.messageId, teflonMessage.senderId);
    }

    // Serialize message as JSON (single serialization point)
    DEBUG_LOG("SendMessage: Serializing message");
    std::string jsonMessage = JsonUtils::SerializeMessage(teflonMessage);

    DEBUG_LOG("SendMessage: Calling sendto with " + std::to_string(jsonMessage.length()) + " bytes");
    int result = sendto(m_socket, jsonMessage.c_str(), (int)jsonMessage.length(), 0,
                       (sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (result != SOCKET_ERROR)
    {
        DEBUG_LOG("SendMessage: sendto succeeded");
        // Notify message sent
        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback)
        {
            DEBUG_LOG("SendMessage: Calling socket callback");
            socketCallback(SocketEventType::MessageSent, "Message sent: " + message.body);
        }
        else
        {
            DEBUG_LOG("SendMessage: socketCallback is null");
        }
        return true;
    }
    else
    {
        DEBUG_LOG("SendMessage: sendto failed with error: " + std::to_string(WSAGetLastError()));
    }

    return false;
}

void NetworkManager::SendMessageAsync(const Message& message)
{
    DEBUG_LOG("SendMessageAsync: Called with message ID: " + message.messageId);

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
            socketCallback(SocketEventType::SendBlocked, "SendMessageAsync blocked: " + reason);
        }
        DEBUG_LOG("SendMessageAsync: Blocked - not connected");
        return;
    }

    // Add message to queue for async processing
    {
        std::lock_guard<std::mutex> lock(m_sendQueueMutex);
        m_sendQueue.push(message);
        DEBUG_LOG("SendMessageAsync: Message queued, queue size: " + std::to_string(m_sendQueue.size()));
    }
    m_sendQueueCondition.notify_one();
    DEBUG_LOG("SendMessageAsync: Notified worker thread");
}

void NetworkManager::SendWorkerThreadFunction()
{
    // Simple debug: Log that thread started
    DEBUG_LOG("SendWorkerThreadFunction: Thread started");

    while (m_sendWorkerRunning)
    {
        DEBUG_LOG("SendWorkerThreadFunction: Starting loop iteration");
        std::optional<Message> messageOpt;

        // Wait for messages in queue
        {
            std::unique_lock<std::mutex> lock(m_sendQueueMutex);
            m_sendQueueCondition.wait(lock, [this] {
                return !m_sendQueue.empty() || !m_sendWorkerRunning;
            });

            if (!m_sendWorkerRunning) {
                DEBUG_LOG("SendWorkerThreadFunction: Exiting because m_sendWorkerRunning is false");
                break;
            }

            if (!m_sendQueue.empty())
            {
                messageOpt = m_sendQueue.front();
                m_sendQueue.pop();
                DEBUG_LOG("SendWorkerThreadFunction: Processing message ID: " + messageOpt->messageId);
            }
        }

        if (messageOpt.has_value())
        {
            // Process the message (this is now on a background thread)
            SendMessage(*messageOpt);
            DEBUG_LOG("SendWorkerThreadFunction: Message processing completed");
        }

        DEBUG_LOG(std::string("SendWorkerThreadFunction: End of loop iteration, m_sendWorkerRunning = ") + (m_sendWorkerRunning.load() ? "true" : "false"));
    }

    DEBUG_LOG("SendWorkerThreadFunction: Thread exiting");
}

void NetworkManager::EventThreadFunction()
{
    DEBUG_LOG("EventThread: Thread started");
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
                socketCallback(SocketEventType::Error, "Socket event wait failed");
            }
            break;
        }
    }

    DEBUG_LOG("EventThread: Thread exiting");
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
            socketCallback(SocketEventType::Error, "Failed to enumerate network events");
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
            socketCallback(SocketEventType::ReadEvent, "FD_READ event received");
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
            socketCallback(SocketEventType::SocketClosed, "Socket closed by remote");
        }
    }
}

void NetworkManager::ProcessSocketData()
{
    // Use a larger buffer to handle larger UDP packets (max UDP payload size)
    // Allocate on heap to avoid stack overflow warning
    constexpr int MAX_UDP_SIZE = 65507;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(MAX_UDP_SIZE);
    sockaddr_storage fromAddr; // Use generic storage to handle both IPv4 and IPv6
    int fromLen = sizeof(fromAddr); // Size of the storage

    int bytesReceived = recvfrom(m_socket, buffer.get(), MAX_UDP_SIZE, 0,
                               (sockaddr*)&fromAddr, &fromLen);

    if (bytesReceived > 0)
    {
        // Construct string using the exact number of bytes received
        std::string jsonData(buffer.get(), bytesReceived);

        // Try to deserialize Teflon-compatible JSON message
        std::optional<Message> teflonMessageOpt = JsonUtils::DeserializeMessage(jsonData);
        if (!teflonMessageOpt) {
            // JSON parse error
            DEBUG_LOG("JSON parse error: Failed to deserialize message");
            DEBUG_LOG("Failed JSON data: " + jsonData);

            SocketEventCallback parseErrorCallback;
            {
                std::lock_guard<std::mutex> parseErrorLock(m_callbackMutex);
                parseErrorCallback = m_socketEventCallback;
            }
            if (parseErrorCallback)
            {
                parseErrorCallback(SocketEventType::JsonParseError, "JSON parse error: " + jsonData);
            }
            return;
        }
        Message teflonMessage = *teflonMessageOpt;

        // Extract sender IP address for peer tracking
        char senderIP[INET6_ADDRSTRLEN]; // Use the larger IPv6 buffer
        std::string senderIPString = "unknown";

        // Check which address family we received
        if (fromAddr.ss_family == AF_INET)
        {
            // It's an IPv4 address
            auto* ipv4Addr = (sockaddr_in*)&fromAddr;
            inet_ntop(AF_INET, &ipv4Addr->sin_addr, senderIP, sizeof(senderIP));
            senderIPString = std::string(senderIP);
        }
        else if (fromAddr.ss_family == AF_INET6)
        {
            // It's an IPv6 address
            auto* ipv6Addr = (sockaddr_in6*)&fromAddr;
            inet_ntop(AF_INET6, &ipv6Addr->sin6_addr, senderIP, sizeof(senderIP));
            senderIPString = std::string(senderIP);
        }

        // Update peer tracker with sender information
        if (m_peerTracker) {
            m_peerTracker->updatePeer(teflonMessage.senderId, senderIPString);

            // Notify the UI that the peer list has changed
            if (m_hNotifyWnd) {
                PostMessage(m_hNotifyWnd, WM_APP_PEERS_UPDATED, 0, 0);
            }
        }

        // Process the incoming message (handles ACK/NACK and validation)
        ProcessIncomingMessage(teflonMessage);
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
                socketCallback(SocketEventType::Error, "Socket receive error: " + std::to_string(error));
            }
        }
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

    // DO NOT track ACK messages - they are responses to other messages and should not be tracked
    // This prevents ACK feedback loops where ACKs get ACKed

    // Serialize and send
    std::string jsonMessage = JsonUtils::SerializeMessage(ackMessage);

    // Debug: Log the acknowledgment JSON being sent
    DEBUG_LOG("SendAcknowledgment: Sending ACK JSON: " + jsonMessage);

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
            socketCallback(SocketEventType::AckSent, "Sent " + ackType + " for message: " + originalMessageId);
        }
    }
}

void NetworkManager::ProcessIncomingMessage(const Message& message)
{
    // Don't process our own messages
    if (message.senderId == m_senderId) {
        return;
    }

    uint32_t calculatedChecksum = Message::calculateChecksum(message.body);
    bool checksumValid = (calculatedChecksum == message.checksum);

    // Handle different message types
    if (message.isAcknowledgment()) {
        if (m_messageTracker) {
            m_messageTracker->processAcknowledgment(message);
        }

        // Notify UI thread to update ACK status
        if (m_hNotifyWnd) {
            PostMessage(m_hNotifyWnd, WM_APP_UPDATE_ACK, 0, 0);
        }

        SocketEventCallback socketCallback;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            socketCallback = m_socketEventCallback;
        }
        if (socketCallback) {
            std::string ackType = (message.type == MessageType::ACK) ? "ACK" : "NACK";
            socketCallback(SocketEventType::MessageReceived, "Teflon " + ackType + " received from " + message.senderId);
        }
        return; // Explicit return to prevent any further processing and ACK feedback loops
    } else if (message.type == MessageType::CHAT) {
        // Handle chat message
        if (checksumValid) {
            // Send ACK for valid message
            SendAcknowledgment(message.messageId, true);

            // Queue the message for UI thread processing (notify-and-pull pattern)
            {
                std::lock_guard<std::mutex> lock(m_messageQueueMutex);
                m_messageQueue.push(message);
            }

            // Notify UI thread that new messages are available
            if (m_hNotifyWnd) {
                PostMessage(m_hNotifyWnd, WM_APP_NEW_MESSAGES_AVAILABLE, 0, 0);
            }

            SocketEventCallback socketCallback;
            {
                std::lock_guard<std::mutex> lock(m_callbackMutex);
                socketCallback = m_socketEventCallback;
            }
            if (socketCallback) {
                socketCallback(SocketEventType::MessageReceived, "Teflon message received from " + message.senderId);
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
                socketCallback(SocketEventType::JsonParseError, "Checksum validation failed for message from " + message.senderId);
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

std::vector<Message> NetworkManager::PopAllMessages()
{
    std::lock_guard<std::mutex> lock(m_messageQueueMutex);
    std::vector<Message> messages;

    while (!m_messageQueue.empty()) {
        messages.push_back(m_messageQueue.front());
        m_messageQueue.pop();
    }

    return messages;
}
