#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <chrono>
#include "NetworkManager.h"
#include "ConnectDialog.h"
#include "PeerPanel.h"
#include "MessageTracker.h"

// Custom messages for network events
#define WM_APP_PEERS_UPDATED (WM_APP + 1)
#define WM_APP_STATS_UPDATED (WM_APP + 2)
#define WM_APP_NEW_MESSAGE   (WM_APP + 3)
#define WM_APP_SYSTEM_EVENT  (WM_APP + 4)
#define WM_APP_UPDATE_ACK    (WM_APP + 5)

// Helper struct for passing message data between threads
struct MessageData {
    std::wstring sender;
    std::wstring message;
};

struct SystemEventData {
    int eventType;
    std::wstring data;
};

// Chat message structure for owner-drawn ListBox
struct ChatMessage {
    std::wstring sender;
    std::wstring message;
    std::string messageId;           // For tracking acknowledgments
    COLORREF senderColor;
    COLORREF messageColor;
    bool isOwnMessage;               // True if sent by this user
    bool hasAck;                     // True if acknowledged
    bool hasNack;                    // True if negatively acknowledged
    bool isTimedOut;                 // True if message timed out
    std::unordered_set<std::string> acknowledgingParties; // Who has acknowledged
    std::chrono::steady_clock::time_point timestamp;

    ChatMessage() : senderColor(RGB(255, 255, 0)), messageColor(RGB(0, 255, 0)),
                   isOwnMessage(false), hasAck(false), hasNack(false), isTimedOut(false),
                   timestamp(std::chrono::steady_clock::now()) {}
};

class ChatForm
{
private:
    HWND m_hWnd;
    HWND m_hChatListBox;             // Changed from m_hChatHistory to owner-drawn ListBox
    HWND m_hMessageInput;
    HWND m_hSendButton;
    HWND m_hParent;
    HINSTANCE m_hInstance;

    std::vector<ChatMessage> m_chatMessages;  // Changed from m_messages to ChatMessage vector
    NetworkManager* m_networkManager;
    std::unique_ptr<ConnectDialog> m_connectDialog;
    std::unique_ptr<PeerPanel> m_peerPanel;
    std::unique_ptr<MessageTracker> m_messageTracker;  // Add message tracker
    HFONT m_hFont;

    static LRESULT CALLBACK ChatFormProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // Helper function to resolve proper module handle
    static HINSTANCE ResolveModuleHandle(HINSTANCE hInstance);

    // Helper function to bring window to foreground using proper Windows pattern
    static void BringWindowToForeground(HWND hWnd);

    void InitializeControls();
    void SendChatMessage();
    void CenterWindow() const;
    void OnAbout() const;
    void OnConnect();
    void OnDisconnect();
    void OnNetworkMessage(const std::string& sender, const std::string& message);
    void OnSocketEvent(int eventType, const std::string& data);

    // Owner-drawn ListBox handlers
    void OnMeasureItem(MEASUREITEMSTRUCT* pMeasureItem);
    void OnDrawItem(DRAWITEMSTRUCT* pDrawItem);
    void UpdateMessageAckStatus(const std::string& messageId);
    void DrawMessageBackground(HDC hdc, const RECT& rect, const ChatMessage& message);
    void DrawMessageText(HDC hdc, const RECT& rect, const ChatMessage& message);
    void DrawAckIndicator(HDC hdc, const RECT& rect, const ChatMessage& message);

public:
    void UpdateConnectionUI();
    void EnableDisconnectControls(bool enable);
    HWND GetConnectDialogHandle() const;

public:
    ChatForm(HWND parent, HINSTANCE hInstance = nullptr);
    ~ChatForm();

    bool Show() const;
    void Hide() const;
    bool IsVisible() const;

    void AddChatMessage(const std::wstring& sender, const std::wstring& message, const std::string& messageId = "");
    void ClearChat();
    void FocusMessageInput();
    void UpdatePeerDisplay();
};
