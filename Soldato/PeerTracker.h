#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>

/**
 * Information about a peer in the network.
 * Equivalent to the Java PeerTracker.PeerInfo record.
 */
struct PeerInfo {
  std::string uuid;                                    // Peer's unique identifier
  std::string ipAddress;                               // Peer's IP address
  std::chrono::steady_clock::time_point lastSeen;      // When this peer was last seen

  PeerInfo() = default;

  PeerInfo(
    const std::string& id,
    const std::string& ip,
    std::chrono::steady_clock::time_point seen)
    : uuid(id), ipAddress(ip), lastSeen(seen) {
  }
};

/**
 * Tracks known peers in the network.
 * Maintains a list of peers with their UUIDs and IP addresses.
 * Equivalent to the Java PeerTracker class.
 */
class PeerTracker {
public:
  /**
   * Default timeout for peer activity in seconds.
   */
  static constexpr int PEER_TIMEOUT_SECONDS = 30;

  /**
   * Creates a new peer tracker.
   */
  PeerTracker();

  /**
   * Creates a new peer tracker with a specific instance ID.
   *
   * @param instanceId The ID to use for this instance
   */
  explicit PeerTracker(const std::string& instanceId);

  /**
   * Destructor - shuts down cleanup thread
   */
  ~PeerTracker();

  // Disable copy constructor and assignment operator
  PeerTracker(const PeerTracker&) = delete;
  PeerTracker& operator=(const PeerTracker&) = delete;

  /**
   * Updates peer information when a message is received.
   *
   * @param senderId     The sender's UUID
   * @param senderAddress The sender's IP address
   */
  void updatePeer(const std::string& senderId, const std::string& senderAddress);

  /**
   * Gets all known peers.
   *
   * @return A map of peer UUIDs to their information
   */
  std::unordered_map<std::string, PeerInfo> getPeers() const;

  /**
   * Gets the number of known peers.
   *
   * @return The number of peers
   */
  int getPeerCount() const;

  /**
   * Cleans up peers that haven't been seen recently.
   */
  void cleanupInactivePeers();

  /**
   * Resets the peer tracker by clearing all peers.
   */
  void reset();

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
   * The ID of this instance.
   */
  std::string m_instanceId;

  /**
   * Map of peer UUIDs to their information.
   */
  mutable std::mutex m_peersMutex;
  std::unordered_map<std::string, PeerInfo> m_peers;

  /**
   * Cleanup thread and control
   */
  std::atomic<bool> m_cleanupRunning;
  std::thread m_cleanupThread;
  std::mutex m_cleanupMutex;
  std::condition_variable m_cleanupCondition;

  /**
   * Cleanup thread function
   */
  void cleanupThreadFunction();
};
