#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>
#include "Message.h"

/**
 * Tracks message delivery status and acknowledgments.
 * Handles message timeouts and maintains delivery statistics.
 * Equivalent to the Java MessageTracker class.
 */
class MessageTracker {
public:
    /**
     * Default timeout for message acknowledgment in seconds.
     */
    static constexpr int MESSAGE_TIMEOUT_SECONDS = 5;

    /**
     * Creates a new message tracker.
     */
    MessageTracker();

    /**
     * Creates a new message tracker with a specific instance ID.
     *
     * @param instanceId The ID to use for this instance
     */
    explicit MessageTracker(const std::string& instanceId);

    /**
     * Destructor - shuts down cleanup thread
     */
    ~MessageTracker();

    // Disable copy constructor and assignment operator
    MessageTracker(const MessageTracker&) = delete;
    MessageTracker& operator=(const MessageTracker&) = delete;

    /**
     * Tracks a new outgoing message.
     *
     * @param messageId The UUID of the message to track
     * @param senderId The sender ID of the message
     */
    void trackMessage(const std::string& messageId, const std::string& senderId);

    /**
     * Processes an acknowledgment message.
     *
     * @param ack The acknowledgment message
     */
    void processAcknowledgment(const Message& ack);

    /**
     * Gets the acknowledgment status for a message.
     *
     * @param messageId The ID of the message to check
     * @return The set of sender IDs that have acknowledged the message
     */
    std::unordered_set<std::string> getAcknowledgingParties(const std::string& messageId) const;

    /**
     * Gets the number of acknowledgments for a message.
     *
     * @param messageId The ID of the message to check
     * @return The number of acknowledgments received for the message
     */
    size_t getAcknowledgmentCount(const std::string& messageId) const;

    /**
     * Checks if a message has been acknowledged by any peer.
     *
     * @param messageId The ID of the message to check
     * @return True if the message has been acknowledged by at least one peer
     */
    bool hasAcknowledgment(const std::string& messageId) const;

    /**
     * Gets delivery statistics.
     *
     * @return A map of statistic names to their values
     */
    std::unordered_map<std::string, long long> getDeliveryStats() const;

    /**
     * Cleans up messages that have timed out.
     */
    void cleanupTimedOutMessages();

    /**
     * Shuts down the tracker's cleanup thread.
     */
    void shutdown();

    /**
     * Gets the instance ID of this tracker.
     */
    std::string getInstanceId() const { return m_instanceId; }

private:
    /**
     * Lightweight record class to hold message tracking information.
     * Only stores essential data for tracking, not full message content.
     */
    struct MessageInfo {
        std::string messageId;
        std::string senderId;
        std::chrono::steady_clock::time_point timestamp;
        std::unordered_map<std::string, Message> acknowledgments;

        MessageInfo(const std::string& msgId, const std::string& sender, std::chrono::steady_clock::time_point ts)
            : messageId(msgId), senderId(sender), timestamp(ts) {}
    };

    /**
     * The ID of this instance.
     */
    std::string m_instanceId;

    /**
     * Map of message IDs to their tracking information.
     */
    mutable std::mutex m_mutex; // Single mutex for all shared data
    std::unordered_map<std::string, std::unique_ptr<MessageInfo>> m_messageMap;

    /**
     * Cleanup thread and control
     */
    std::atomic<bool> m_cleanupRunning;
    std::thread m_cleanupThread;
    std::mutex m_cleanupMutex;
    std::condition_variable m_cleanupCondition;

    /**
     * Statistics
     */
    long long m_totalMessagesSent = 0;
    long long m_totalAcksReceived = 0;
    long long m_totalNacksReceived = 0;
    long long m_totalMessagesTimedOut = 0;

    /**
     * Cleanup thread function
     */
    void cleanupThreadFunction();
};
