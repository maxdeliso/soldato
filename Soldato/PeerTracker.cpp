#include "framework.h"
#include "PeerTracker.h"
#include <iostream>

PeerTracker::PeerTracker()
    : PeerTracker("") {
    // Generate a simple UUID-like string for this instance
    m_instanceId = "peer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

PeerTracker::PeerTracker(const std::string& instanceId)
    : m_instanceId(instanceId)
    , m_cleanupRunning(true) {

    // Start cleanup thread
    m_cleanupThread = std::thread(&PeerTracker::cleanupThreadFunction, this);
}

PeerTracker::~PeerTracker() {
    shutdown();
}

void PeerTracker::updatePeer(const std::string& senderId, const std::string& senderAddress) {
    if (senderId == m_instanceId) {
        return; // Don't track ourselves
    }

    if (senderAddress.empty()) {
        std::cout << "[PeerTracker] Could not extract IP address from sender" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_peers[senderId] = PeerInfo(senderId, senderAddress, std::chrono::steady_clock::now());

    std::cout << "[PeerTracker] Updated peer: " << senderId << " at " << senderAddress << std::endl;
}

std::unordered_map<std::string, PeerInfo> PeerTracker::getPeers() const {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    return m_peers;
}

int PeerTracker::getPeerCount() const {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    return static_cast<int>(m_peers.size());
}

void PeerTracker::cleanupInactivePeers() {
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(PEER_TIMEOUT_SECONDS);

    std::lock_guard<std::mutex> lock(m_peersMutex);

    auto it = m_peers.begin();
    while (it != m_peers.end()) {
        if (it->second.lastSeen < cutoff) {
            std::cout << "[PeerTracker] Removing inactive peer: " << it->first << std::endl;
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerTracker::reset() {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_peers.clear();
    std::cout << "[PeerTracker] Peer tracker reset - cleared all peers" << std::endl;
}

void PeerTracker::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_cleanupMutex);
        m_cleanupRunning = false;
    }
    m_cleanupCondition.notify_all();

    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }
}

void PeerTracker::cleanupThreadFunction() {
    std::unique_lock<std::mutex> lock(m_cleanupMutex);

    while (m_cleanupRunning) {
        lock.unlock();
        cleanupInactivePeers();
        lock.lock();

        // Wait for timeout or shutdown signal
        m_cleanupCondition.wait_for(lock, std::chrono::seconds(PEER_TIMEOUT_SECONDS), [this] {
            return !m_cleanupRunning;
        });
    }
}
