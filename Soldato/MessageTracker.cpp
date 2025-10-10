#include "framework.h"
#include "MessageTracker.h"
#include "DebugUtils.h"
#include <algorithm>

MessageTracker::MessageTracker()
    : MessageTracker(Message::generateUUID()) {
}

MessageTracker::MessageTracker(const std::string& instanceId)
    : m_instanceId(instanceId)
    , m_cleanupRunning(true) {

    // Start cleanup thread
    m_cleanupThread = std::thread(&MessageTracker::cleanupThreadFunction, this);
}

MessageTracker::~MessageTracker() {
    shutdown();
}

void MessageTracker::trackMessage(const std::string& messageId, const std::string& senderId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if message already exists (upsert behavior)
    auto it = m_messageMap.find(messageId);
    if (it != m_messageMap.end()) {
        DEBUG_LOG("[MessageTracker] Message already tracked, updating: " + messageId + " (sender: " + senderId + ")");
        // Update existing message info but preserve acknowledgments
        it->second->senderId = senderId;
        it->second->timestamp = std::chrono::steady_clock::now();
        return;
    }

    m_messageMap[messageId] = std::make_unique<MessageInfo>(
        messageId,
        senderId,
        std::chrono::steady_clock::now()
    );
    m_totalMessagesSent++;

    DEBUG_LOG("[MessageTracker] Tracking new message: " + messageId + " (sender: " + senderId + ")");
}

void MessageTracker::processAcknowledgment(const Message& ack) {
    if (!ack.isAcknowledgment() || !ack.originalMessageId.has_value()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.find(ack.originalMessageId.value());
    if (it == m_messageMap.end()) {
        DEBUG_LOG("[MessageTracker] Ignoring acknowledgment for unknown or timed out message: " + ack.originalMessageId.value());
        return; // Message not found or already timed out
    }

    MessageInfo* info = it->second.get();

    // Store the acknowledgment regardless of who sent the original message
    // This allows us to track ACKs for our own messages to show proper UI status
    info->acknowledgments[ack.senderId] = ack;

    if (ack.type == MessageType::ACK) {
        m_totalAcksReceived++;
        DEBUG_LOG("[MessageTracker] Received ACK for message: " + ack.originalMessageId.value() + " from: " + ack.senderId + " (original sender: " + info->senderId + ")");
    } else {
        m_totalNacksReceived++;
        DEBUG_LOG("[MessageTracker] Received NACK for message: " + ack.originalMessageId.value() + " from: " + ack.senderId + " (original sender: " + info->senderId + ")");
    }
}

std::unordered_set<std::string> MessageTracker::getAcknowledgingParties(const std::string& messageId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.find(messageId);
    if (it == m_messageMap.end()) {
        return {};
    }

    std::unordered_set<std::string> parties;
    for (const auto& pair : it->second->acknowledgments) {
        parties.insert(pair.first);
    }
    return parties;
}

size_t MessageTracker::getAcknowledgmentCount(const std::string& messageId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.find(messageId);
    if (it == m_messageMap.end()) {
        return 0;
    }

    return it->second->acknowledgments.size();
}

bool MessageTracker::hasAcknowledgment(const std::string& messageId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.find(messageId);
    if (it == m_messageMap.end()) {
        return false;
    }

    return !it->second->acknowledgments.empty();
}

std::unordered_map<std::string, long long> MessageTracker::getDeliveryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return {
        {"messagesSent", m_totalMessagesSent},
        {"acksReceived", m_totalAcksReceived},
        {"nacksReceived", m_totalNacksReceived},
        {"messagesTimedOut", m_totalMessagesTimedOut},
        {"pendingMessages", static_cast<long long>(m_messageMap.size())}
    };
}

void MessageTracker::cleanupTimedOutMessages() {
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(MESSAGE_TIMEOUT_SECONDS);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.begin();
    while (it != m_messageMap.end()) {
        if (it->second->timestamp < cutoff) {
            DEBUG_LOG("[MessageTracker] Message timed out: " + it->first);
            m_totalMessagesTimedOut++;
            it = m_messageMap.erase(it);
        } else {
            ++it;
        }
    }
}

void MessageTracker::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_cleanupMutex);
        m_cleanupRunning = false;
    }
    m_cleanupCondition.notify_all();

    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }
}

void MessageTracker::cleanupThreadFunction() {
    std::unique_lock<std::mutex> lock(m_cleanupMutex);

    while (m_cleanupRunning) {
        lock.unlock();
        cleanupTimedOutMessages();
        lock.lock();

        // Wait for timeout or shutdown signal
        m_cleanupCondition.wait_for(lock, std::chrono::seconds(MESSAGE_TIMEOUT_SECONDS), [this] {
            return !m_cleanupRunning;
        });
    }
}
