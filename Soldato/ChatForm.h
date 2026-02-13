#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_set>
#include <chrono>
#include "NetworkManager.h"
#include "ConnectDialog.h"
#include "PeerPanel.h"
#include "MessageTracker.h"

// Color packing macros for efficient storage of two COLORREF values in one uint64_t
// Pack format: [63:32] = senderColor, [31:0] = messageColor
#define PACK_COLORS(sender_color, message_color) \
    (((uint64_t)(sender_color) << 32) | ((uint64_t)(message_color) & 0xFFFFFFFFULL))

#define UNPACK_SENDER_COLOR(packed_colors) \
    ((COLORREF)((packed_colors) >> 32))

#define UNPACK_MESSAGE_COLOR(packed_colors) \
    ((COLORREF)((packed_colors) & 0xFFFFFFFFULL))

// Custom messages for network events
#define WM_APP_PEERS_UPDATED (WM_APP + 1)
#define WM_APP_STATS_UPDATED (WM_APP + 2)
#define WM_APP_NEW_MESSAGE   (WM_APP + 3)
#define WM_APP_SYSTEM_EVENT  (WM_APP + 4)
#define WM_APP_UPDATE_ACK    (WM_APP + 5)
#define WM_APP_NEW_MESSAGES_AVAILABLE (WM_APP + 6)

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
  uint64_t packedColors;           // Packed senderColor and messageColor for efficiency
  bool isOwnMessage;
  bool hasAck;
  bool hasNack;
  bool isTimedOut;
  std::unordered_set<std::string> acknowledgingParties;
  std::chrono::steady_clock::time_point timestamp;

  ChatMessage() : packedColors(PACK_COLORS(RGB(255, 255, 0), RGB(0, 255, 0))),
    isOwnMessage(false), hasAck(false), hasNack(false), isTimedOut(false),
    timestamp(std::chrono::steady_clock::now()) {
  }
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

  static constexpr UINT_PTR INDICATOR_TIMER_ID = 999;

  std::deque<ChatMessage> m_chatMessages;  // Use deque for pointer stability
  NetworkManager* m_networkManager;
  std::unique_ptr<ConnectDialog> m_connectDialog;
  std::unique_ptr<PeerPanel> m_peerPanel;
  HFONT m_hFont;

  // Cached double-buffering objects
  HDC m_hMemDC = nullptr;
  HBITMAP m_hMemBitmap = nullptr;
  HBITMAP m_hOldBitmap = nullptr;
  int m_memWidth = 0;
  int m_memHeight = 0;

  // Pre-created GDI objects for performance optimization
  HBRUSH m_hAckBgBrush;           // Dark green for ACK messages
  HBRUSH m_hTimeoutBgBrush;       // Dark red for timed out messages
  HBRUSH m_hNackBgBrush;          // Dark red for NACK messages
  HBRUSH m_hOwnMessageBgBrush;    // Dark blue for own messages
  HBRUSH m_hDefaultBgBrush;       // Default dark green
  HBRUSH m_hBackgroundBrush;      // Main window background
  HPEN m_hAckBorderPen;           // Border pen for ACK messages
  HPEN m_hTimeoutBorderPen;       // Border pen for timed out messages
  HPEN m_hNackBorderPen;          // Border pen for NACK messages
  HPEN m_hOwnMessageBorderPen;    // Border pen for own messages
  HPEN m_hDefaultBorderPen;       // Default border pen
  HPEN m_hNeonPen;                // Neon green pen for grid and brackets
  HBRUSH m_hAckIndicatorBrush;    // Green indicator brush
  HBRUSH m_hTimeoutIndicatorBrush;// Red indicator brush
  HBRUSH m_hNackIndicatorBrush;   // Light red indicator brush
  HBRUSH m_hPendingIndicatorBrush;// Yellow indicator brush
  HPEN m_hAckIndicatorBorderPen;  // Green indicator border pen
  HPEN m_hTimeoutIndicatorBorderPen; // Red indicator border pen
  HPEN m_hNackIndicatorBorderPen; // Light red indicator border pen
  HPEN m_hPendingIndicatorBorderPen; // Yellow indicator border pen
  HBRUSH m_hGrayBgBrush;          // Gunmetal gray for ListBox/Input background
  HPEN m_hGrayBorderPen;          // Gunmetal gray border pen

  // Optimized ACK status checking
  std::unordered_set<std::string> m_pendingMessages; // Only own messages that haven't been ACK'd or timed out

  static LRESULT CALLBACK ChatFormProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

  // Refactored message handlers for better code organization
  LRESULT OnCommand(WPARAM wParam, LPARAM lParam);
  LRESULT OnNewMessagesAvailable();
  LRESULT OnSystemEvent(LPARAM lParam);
  LRESULT OnUpdateAckStatus();
  LRESULT OnPeersUpdated();
  LRESULT OnSize(WPARAM wParam, LPARAM lParam);
  LRESULT OnPaint();
  LRESULT OnKeyDown(WPARAM wParam);
  LRESULT OnMeasureItem(LPARAM lParam);
  LRESULT OnDrawItem(LPARAM lParam);

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

  // GDI object management
  void CreateGDIObjects();
  void DestroyGDIObjects();

  // Owner-drawn ListBox handlers
  void OnMeasureItem(MEASUREITEMSTRUCT* pMeasureItem);
  void OnDrawItem(DRAWITEMSTRUCT* pDrawItem);
  void DrawMessageBackground(HDC hdc, const RECT& rect, const ChatMessage& message) const;
  void DrawMessageText(HDC hdc, const RECT& rect, const ChatMessage& message);
  void DrawAckIndicator(HDC hdc, const RECT& rect, const ChatMessage& message) const;

  COLORREF GetSinusoidalColor(ULONGLONG timeMs, float phaseOffset) const;
  void DrawHypercubeIndicator(HDC memDC, int centerX, int centerY, bool connected) const;

public:
  void UpdateConnectionUI();
  void EnableDisconnectControls(bool enable) const;
  HWND GetConnectDialogHandle() const;

public:
  ChatForm(HWND parent, HINSTANCE hInstance = nullptr);
  ~ChatForm();

  bool Show() const;
  void Hide() const;
  bool IsVisible() const;

  void AddChatMessage(const std::wstring& sender, const std::wstring& message, const std::string& messageId = "");
  void ClearChat();
  void FocusMessageInput() const;
  void UpdatePeerDisplay();

  // Pending messages management
  void ClearPendingMessages();
};
