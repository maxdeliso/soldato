#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "PeerTracker.h"

/**
 * Panel for displaying known peers in the network.
 * Shows peer UUIDs, IP addresses, and last seen timestamps.
 * Equivalent to the Java PeerPanel class.
 */
class PeerPanel {
private:
    HWND m_hWnd;
    HWND m_hPeerList;
    HWND m_hPeerCountLabel;
    HWND m_hParent;
    HFONT m_hFont; // Member variable for the font
    HBRUSH m_hBkgBrush; // Member variable for the background brush
    HINSTANCE m_hInstance;

    std::vector<std::wstring> m_peerEntries;
    std::unordered_map<std::string, PeerInfo> m_currentPeers;

    // Thread safety
    mutable std::mutex m_peersMutex;

    static LRESULT CALLBACK PeerPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void CenterWindow() const;
    void UpdatePeerList();
    void UpdatePeerCount();
    void UpdatePeerListInternal();
    void UpdatePeerCountInternal();
    std::wstring FormatPeerEntry(const PeerInfo& peer) const;
    std::wstring FormatTimestamp(const std::chrono::steady_clock::time_point& timestamp) const;

public:
    PeerPanel(HWND parent, HINSTANCE hInstance = nullptr);
    ~PeerPanel();

    bool Show() const;
    void Hide() const;
    bool IsVisible() const;

    /**
     * Updates the peer list with current peer information.
     *
     * @param peers Map of peer UUIDs to their information
     */
    void updatePeers(const std::unordered_map<std::string, PeerInfo>& peers);

    /**
     * Gets the window handle for this panel.
     */
    HWND GetHandle() const { return m_hWnd; }
};
