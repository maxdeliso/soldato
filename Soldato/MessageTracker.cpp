#include "framework.h"
#include "MessageTracker.h"
#include <iostream>
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

void MessageTracker::trackMessage(const Message& message) {
    if (message.type != MessageType::CHAT) {
        return; // Only track chat messages
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    m_messageMap[message.messageId] = std::make_unique<MessageInfo>(
        message,
        std::chrono::steady_clock::now()
    );
    m_totalMessagesSent++;

    std::cout << "[MessageTracker] Tracking new message: " << message.messageId << std::endl;
}

void MessageTracker::processAcknowledgment(const Message& ack) {
    if (!ack.isAcknowledgment() || !ack.originalMessageId.has_value()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_messageMap.find(ack.originalMessageId.value());
    if (it == m_messageMap.end()) {
        std::cout << "[MessageTracker] Ignoring acknowledgment for unknown or timed out message: "
                  << ack.originalMessageId.value() << std::endl;
        return; // Message not found or already timed out
    }

    MessageInfo* info = it->second.get();

    // Don't process acknowledgments for messages we sent ourselves
    if (info->message.senderId == m_instanceId) {
        return;
    }

    info->acknowledgments[ack.senderId] = ack;

    if (ack.type == MessageType::ACK) {
        m_totalAcksReceived++;
        std::cout << "[MessageTracker] Received ACK for message: " << ack.originalMessageId.value()
                  << " from: " << ack.senderId << std::endl;
    } else {
        m_totalNacksReceived++;
        std::cout << "[MessageTracker] Received NACK for message: " << ack.originalMessageId.value()
                  << " from: " << ack.senderId << std::endl;
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
            std::cout << "[MessageTracker] Message timed out: " << it->first << std::endl;
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
