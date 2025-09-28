#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include "NetworkManager.h"
#include "ConnectDialog.h"
#include "PeerPanel.h"

// Custom messages for network events
#define WM_APP_PEERS_UPDATED (WM_APP + 1)
#define WM_APP_STATS_UPDATED (WM_APP + 2)
#define WM_APP_NEW_MESSAGE   (WM_APP + 3)
#define WM_APP_SYSTEM_EVENT  (WM_APP + 4)

// Helper struct for passing message data between threads
struct MessageData {
    std::wstring sender;
    std::wstring message;
};

struct SystemEventData {
    int eventType;
    std::wstring data;
};

class ChatForm
{
private:
    HWND m_hWnd;
    HWND m_hChatHistory;
    HWND m_hMessageInput;
    HWND m_hSendButton;
    HWND m_hParent;

    std::vector<std::wstring> m_messages;
    NetworkManager* m_networkManager;
    std::unique_ptr<ConnectDialog> m_connectDialog;
    std::unique_ptr<PeerPanel> m_peerPanel;
    HFONT m_hFont;

    static LRESULT CALLBACK ChatFormProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void SendChatMessage();
    void CenterWindow() const;
    void OnAbout() const;
    void OnConnect();
    void OnDisconnect();
    void OnNetworkMessage(const std::string& sender, const std::string& message);
    void OnSocketEvent(int eventType, const std::string& data);

public:
    void UpdateConnectionUI();
    void EnableDisconnectControls(bool enable);

public:
    ChatForm(HWND parent);
    ~ChatForm();

    bool Show() const;
    void Hide() const;
    bool IsVisible() const;

    void AddChatMessage(const std::wstring& sender, const std::wstring& message);
    void ClearChat();
    void FocusMessageInput();
    void UpdatePeerDisplay();
};
