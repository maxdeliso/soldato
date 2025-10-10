#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <functional>
#include "json.hpp"
#include "Message.h"
#include "MessageTracker.h"
#include "PeerTracker.h"
#include "WinsockManager.h"

#pragma comment(lib, "ws2_32.lib")

// Socket event types for callback notifications
enum class SocketEventType {
    Connected = 1,              // Connection established
    MessageSent = 2,            // Message successfully sent
    SocketClosed = 3,           // Socket closed by remote
    MessageReceived = 4,        // Message received (legacy or Teflon)
    ReadEvent = 5,              // FD_READ event received
    RawJsonReceived = 6,        // Raw JSON data received
    JsonParseError = 7,         // JSON parsing failed
    AckSent = 8,                // Acknowledgment sent
    SendBlocked = 9,            // SendMessageAsync blocked
    Error = -1                  // General error condition
};

class NetworkManager
{
private:
    static WinsockManager s_winsockManager;

    SOCKET m_socket;
    sockaddr_in m_multicastAddr;
    std::string m_multicastIP;
    int m_port;
    std::atomic<bool> m_connected;
    std::string m_username;
    std::string m_senderId;  // UUID for Teflon compatibility
    std::unique_ptr<MessageTracker> m_messageTracker;  // Message tracking for ACK/NACK
    std::unique_ptr<PeerTracker> m_peerTracker;        // Peer tracking for known peers

    // Thread safety for member variables
    mutable std::mutex m_memberMutex;

    // Windows event objects
    HANDLE m_socketEvent;
    HANDLE m_shutdownEvent;
    std::thread m_eventThread;
    std::atomic<bool> m_threadRunning;

    // UI notification window handle
    HWND m_hNotifyWnd;

    // Synchronization for callbacks
    mutable std::mutex m_callbackMutex;

    // Message sending queue and worker thread
    std::queue<std::string> m_sendQueue;
    std::mutex m_sendQueueMutex;
    std::condition_variable m_sendQueueCondition;
    std::thread m_sendWorkerThread;
    std::atomic<bool> m_sendWorkerRunning;

    // Thread-safe message queue for UI notifications (notify-and-pull pattern)
    struct QueuedMessage {
        std::string sender;
        std::string message;
        QueuedMessage(const std::string& s, const std::string& m) : sender(s), message(m) {}
    };
    std::queue<QueuedMessage> m_messageQueue;
    std::mutex m_messageQueueMutex;

    NetworkManager();
    ~NetworkManager();

    bool SetupSocketEvents();
    void CleanupSocketEvents();

public:
    static NetworkManager& GetInstance();

    bool Connect(const std::string& multicastIP, int port, const std::string& username);
    void Disconnect();
    bool SendMessage(const std::string& message);
    void SendMessageAsync(const std::string& message);
    bool IsConnected() const { return m_connected.load(); }

    // Teflon-compatible message helpers
    Message CreateChatMessage(const std::string& sender, const std::string& content);
    Message CreateAcknowledgment(const std::string& sender, const std::string& originalMessageId, bool isPositive);

    // Message tracking and ACK/NACK functionality
    void SendAcknowledgment(const std::string& originalMessageId, bool isPositive);
    void ProcessIncomingMessage(const Message& message);
    MessageTracker* GetMessageTracker() const { return m_messageTracker.get(); }
    const std::string& GetSenderId() const { return m_senderId; }
    std::unordered_map<std::string, long long> GetDeliveryStats() const;

    // Peer tracking functionality
    std::unordered_map<std::string, PeerInfo> GetKnownPeers() const;
    int GetPeerCount() const;

    std::string GetUsername() const {
        std::lock_guard<std::mutex> lock(m_memberMutex);
        return m_username;
    }
    std::string GetMulticastIP() const {
        std::lock_guard<std::mutex> lock(m_memberMutex);
        return m_multicastIP;
    }
    int GetPort() const {
        std::lock_guard<std::mutex> lock(m_memberMutex);
        return m_port;
    }

    // Callback for received messages - using std::function for safer callback management
    typedef std::function<void(const std::string& sender, const std::string& message)> MessageCallback;
    void SetMessageCallback(MessageCallback callback);
    void ClearMessageCallback();

    // Windows event callbacks - using std::function for safer callback management
    typedef std::function<void(SocketEventType eventType, const std::string& data)> SocketEventCallback;
    void SetSocketEventCallback(SocketEventCallback callback);
    void ClearSocketEventCallback();

    // UI notification methods
    void SetNotificationWindow(HWND hWnd);

    // Thread-safe message queue access (notify-and-pull pattern)
    std::vector<QueuedMessage> PopAllMessages();

private:
    MessageCallback m_messageCallback;
    SocketEventCallback m_socketEventCallback;

    void EventThreadFunction();
    void HandleSocketEvent(WSAEVENT event);
    void ProcessSocketData();
    void SendWorkerThreadFunction();
};
