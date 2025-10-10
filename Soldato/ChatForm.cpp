#include "framework.h"
#include "ChatForm.h"
#include "Resource.h"
#include "StringUtils.h"
#include "DebugUtils.h"
#include "Message.h"
#include "NetworkManager.h"
#include "JsonUtils.h"
#include "json.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "comctl32.lib")

// Forward declaration
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Control IDs - using enum class for type safety
enum class ControlId {
    ChatHistory = 1001,
    MessageInput = 1002,
    SendButton = 1003
};

// Message status symbols
enum class MessageStatus {
    Pending = 0,
    Acknowledged = 1,
    NegativelyAcknowledged = 2,
    TimedOut = 3
};

// Helper function to get status symbol
std::wstring GetStatusSymbol(MessageStatus status) {
    switch (status) {
        case MessageStatus::TimedOut:
            return L"T";
        case MessageStatus::NegativelyAcknowledged:
            return L"X";
        case MessageStatus::Acknowledged:
            return L"V";
        case MessageStatus::Pending:
        default:
            return L"?";
    }
}

// Custom message for Enter key
#define WM_SEND_MESSAGE    (WM_USER + 1)

// Cyberpunk theme constants
namespace CyberpunkTheme {
    constexpr COLORREF BACKGROUND_COLOR = RGB(0, 20, 0);     // Dark green-black
    constexpr COLORREF GRID_COLOR = RGB(0, 255, 0);          // Bright green
    constexpr COLORREF BRACKET_COLOR = RGB(0, 255, 0);       // Bright green
    constexpr int GRID_SPACING = 20;                         // Grid line spacing
    constexpr int BRACKET_SIZE = 15;                         // Corner bracket size
    constexpr int BRACKET_OFFSET = 10;                       // Distance from window edge

    // Pre-calculated subtraction constants for bracket positioning
    constexpr int BRACKET_END_X = BRACKET_OFFSET + BRACKET_SIZE;     // offset + size
    constexpr int BRACKET_END_Y = BRACKET_OFFSET + BRACKET_SIZE;     // offset + size

    // Pre-calculated bracket corner positions (relative to window edges)
    constexpr int TOP_LEFT_X = BRACKET_OFFSET;                       // Top-left X position
    constexpr int TOP_LEFT_Y = BRACKET_OFFSET;                       // Top-left Y position
    constexpr int TOP_RIGHT_X = BRACKET_OFFSET;                      // Top-right X offset from right edge
    constexpr int TOP_RIGHT_Y = BRACKET_OFFSET;                      // Top-right Y position
    constexpr int BOTTOM_LEFT_X = BRACKET_OFFSET;                    // Bottom-left X position
    constexpr int BOTTOM_LEFT_Y = BRACKET_OFFSET;                    // Bottom-left Y offset from bottom edge
    constexpr int BOTTOM_RIGHT_X = BRACKET_OFFSET;                   // Bottom-right X offset from right edge
    constexpr int BOTTOM_RIGHT_Y = BRACKET_OFFSET;                   // Bottom-right Y offset from bottom edge
}

// Subclass procedure for message input
LRESULT CALLBACK MessageInputProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CHAR && wParam == VK_RETURN)
    {
        // Get the parent window and send custom message
        HWND parent = GetParent(hWnd);
        if (parent)
        {
            DEBUG_LOG("MessageInputProc: Enter key pressed, sending WM_SEND_MESSAGE\n");
            PostMessage(parent, WM_SEND_MESSAGE, 0, 0);
        }
        return 0; // Don't process the Enter key
    }

    // Call the original window procedure
    WNDPROC originalProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (originalProc)
    {
        return CallWindowProc(originalProc, hWnd, message, wParam, lParam);
    }
    else
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

ChatForm::ChatForm(HWND parent, HINSTANCE hInstance) :
m_hParent(parent),
m_hWnd(nullptr),
m_hChatListBox(nullptr),
m_networkManager(nullptr),
m_connectDialog(nullptr),
m_peerPanel(nullptr),
m_hFont(nullptr),
m_hInstance(hInstance),
m_jsonMsg(),
// Initialize GDI objects to nullptr - will be created in constructor body
m_hAckBgBrush(nullptr),
m_hTimeoutBgBrush(nullptr),
m_hNackBgBrush(nullptr),
m_hOwnMessageBgBrush(nullptr),
m_hDefaultBgBrush(nullptr),
m_hBackgroundBrush(nullptr),
m_hAckBorderPen(nullptr),
m_hTimeoutBorderPen(nullptr),
m_hNackBorderPen(nullptr),
m_hOwnMessageBorderPen(nullptr),
m_hDefaultBorderPen(nullptr),
m_hNeonPen(nullptr),
m_hAckIndicatorBrush(nullptr),
m_hTimeoutIndicatorBrush(nullptr),
m_hNackIndicatorBrush(nullptr),
m_hPendingIndicatorBrush(nullptr),
m_hAckIndicatorBorderPen(nullptr),
m_hTimeoutIndicatorBorderPen(nullptr),
m_hNackIndicatorBorderPen(nullptr),
m_hPendingIndicatorBorderPen(nullptr)
{
    // Resolve the proper module handle
    m_hInstance = ResolveModuleHandle(m_hInstance);

    // Register the chat form window class - simplified
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = ChatFormProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(ChatForm*);
        wcex.hInstance = m_hInstance;
        wcex.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SOLDATO));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Standard background
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = L"ChatFormClass";
        wcex.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SMALL));

        RegisterClassExW(&wcex);
        classRegistered = true;
    }

    // Create the chat form window with proper menu
    m_hWnd = CreateWindowW(
        L"ChatFormClass",
        L"Soldato",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        m_hParent,
        LoadMenu(m_hInstance, MAKEINTRESOURCE(IDC_SOLDATO)),
        m_hInstance,
        this
    );

    if (m_hWnd)
    {
        DEBUG_LOG("ChatForm: Window created successfully");
        CenterWindow();

        // Create GDI objects for performance optimization
        CreateGDIObjects();

        // Show window first to ensure proper sizing
        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);

        // Ensure window is brought to foreground and has focus
        SetForegroundWindow(m_hWnd);
        BringWindowToTop(m_hWnd);

        // Initialize controls after window is shown and sized
        InitializeControls();

        // Initialize network manager
        m_networkManager = &NetworkManager::GetInstance();
        DEBUG_LOG("ChatForm: NetworkManager instance obtained");
        m_networkManager->SetNotificationWindow(m_hWnd);
        m_networkManager->SetMessageCallback([this](const std::string& sender, const std::string& message) {
            this->OnNetworkMessage(sender, message);
        });
        m_networkManager->SetSocketEventCallback([this](SocketEventType eventType, const std::string& data) {
            this->OnSocketEvent(eventType, data);
        });

        // Message tracking is now handled by NetworkManager
        DEBUG_LOG("ChatForm: Using NetworkManager's MessageTracker");

        // Set up a timer to periodically update acknowledgment statuses
        SetTimer(m_hWnd, 1, 2000, nullptr); // 2 second timer to reduce flickering

        // Initialize UI state based on connection status
        UpdateConnectionUI();

        // Create connect dialog with callback
        m_connectDialog = std::make_unique<ConnectDialog>(m_hWnd, [this]() {
            // This code runs when the dialog reports success
            this->FocusMessageInput();
            this->UpdateConnectionUI();
        }, m_hInstance);

        // Create peer panel
        DEBUG_LOG("ChatForm: About to create PeerPanel");
        m_peerPanel = std::make_unique<PeerPanel>(m_hWnd, m_hInstance);
        DEBUG_LOG("ChatForm: PeerPanel created successfully");

        // Set initial layout for peer panel
        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        // Calculate split layout: chat on left (70%), peer panel on right (30%)
        int chatWidth = static_cast<int>(width * 0.7);
        int peerWidth = width - chatWidth;

        // Position peer panel on the right
        if (m_peerPanel) {
            SetWindowPos(m_peerPanel->GetHandle(), nullptr, chatWidth, 0, peerWidth, height, SWP_NOZORDER);
        }
    }
}

ChatForm::~ChatForm()
{
    // Clear network manager callbacks first to prevent race conditions
    if (m_networkManager)
    {
        m_networkManager->ClearMessageCallback();
        m_networkManager->ClearSocketEventCallback();
    }

    // Kill the timer
    if (m_hWnd) {
        KillTimer(m_hWnd, 1);
    }

    // Clean up GDI objects
    DestroyGDIObjects();

    // Clean up the font
    if (m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }

    // Smart pointers handle cleanup automatically
    m_peerPanel.reset();
    m_connectDialog.reset();

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void ChatForm::CreateGDIObjects()
{
    // Create background brushes
    m_hAckBgBrush = CreateSolidBrush(RGB(5, 30, 5));        // Dark green for ACK messages
    m_hTimeoutBgBrush = CreateSolidBrush(RGB(40, 10, 10));  // Dark red for timed out messages
    m_hNackBgBrush = CreateSolidBrush(RGB(30, 10, 10));     // Dark red for NACK messages
    m_hOwnMessageBgBrush = CreateSolidBrush(RGB(5, 15, 25)); // Dark blue for own messages
    m_hDefaultBgBrush = CreateSolidBrush(RGB(5, 25, 5));    // Default dark green
    m_hBackgroundBrush = CreateSolidBrush(CyberpunkTheme::BACKGROUND_COLOR); // Main window background

    // Create border pens
    m_hAckBorderPen = CreatePen(PS_SOLID, 1, RGB(10, 60, 10));
    m_hTimeoutBorderPen = CreatePen(PS_SOLID, 1, RGB(80, 20, 20));
    m_hNackBorderPen = CreatePen(PS_SOLID, 1, RGB(60, 20, 20));
    m_hOwnMessageBorderPen = CreatePen(PS_SOLID, 1, RGB(10, 30, 50));
    m_hDefaultBorderPen = CreatePen(PS_SOLID, 1, RGB(10, 50, 10));
    m_hNeonPen = CreatePen(PS_SOLID, 1, CyberpunkTheme::GRID_COLOR);

    // Create indicator brushes
    m_hAckIndicatorBrush = CreateSolidBrush(RGB(100, 255, 100));      // Green for ACK
    m_hTimeoutIndicatorBrush = CreateSolidBrush(RGB(255, 100, 100));  // Red for timeout
    m_hNackIndicatorBrush = CreateSolidBrush(RGB(255, 150, 150));     // Light red for NACK
    m_hPendingIndicatorBrush = CreateSolidBrush(RGB(255, 255, 100));  // Yellow for pending

    // Create indicator border pens
    m_hAckIndicatorBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 255, 150));
    m_hTimeoutIndicatorBorderPen = CreatePen(PS_SOLID, 1, RGB(255, 150, 150));
    m_hNackIndicatorBorderPen = CreatePen(PS_SOLID, 1, RGB(255, 200, 200));
    m_hPendingIndicatorBorderPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 150));

    DEBUG_LOG("ChatForm: GDI objects created successfully");
}

void ChatForm::DestroyGDIObjects()
{
    // Delete brushes
    if (m_hAckBgBrush) { DeleteObject(m_hAckBgBrush); m_hAckBgBrush = nullptr; }
    if (m_hTimeoutBgBrush) { DeleteObject(m_hTimeoutBgBrush); m_hTimeoutBgBrush = nullptr; }
    if (m_hNackBgBrush) { DeleteObject(m_hNackBgBrush); m_hNackBgBrush = nullptr; }
    if (m_hOwnMessageBgBrush) { DeleteObject(m_hOwnMessageBgBrush); m_hOwnMessageBgBrush = nullptr; }
    if (m_hDefaultBgBrush) { DeleteObject(m_hDefaultBgBrush); m_hDefaultBgBrush = nullptr; }
    if (m_hBackgroundBrush) { DeleteObject(m_hBackgroundBrush); m_hBackgroundBrush = nullptr; }

    // Delete border pens
    if (m_hAckBorderPen) { DeleteObject(m_hAckBorderPen); m_hAckBorderPen = nullptr; }
    if (m_hTimeoutBorderPen) { DeleteObject(m_hTimeoutBorderPen); m_hTimeoutBorderPen = nullptr; }
    if (m_hNackBorderPen) { DeleteObject(m_hNackBorderPen); m_hNackBorderPen = nullptr; }
    if (m_hOwnMessageBorderPen) { DeleteObject(m_hOwnMessageBorderPen); m_hOwnMessageBorderPen = nullptr; }
    if (m_hDefaultBorderPen) { DeleteObject(m_hDefaultBorderPen); m_hDefaultBorderPen = nullptr; }
    if (m_hNeonPen) { DeleteObject(m_hNeonPen); m_hNeonPen = nullptr; }

    // Delete indicator brushes
    if (m_hAckIndicatorBrush) { DeleteObject(m_hAckIndicatorBrush); m_hAckIndicatorBrush = nullptr; }
    if (m_hTimeoutIndicatorBrush) { DeleteObject(m_hTimeoutIndicatorBrush); m_hTimeoutIndicatorBrush = nullptr; }
    if (m_hNackIndicatorBrush) { DeleteObject(m_hNackIndicatorBrush); m_hNackIndicatorBrush = nullptr; }
    if (m_hPendingIndicatorBrush) { DeleteObject(m_hPendingIndicatorBrush); m_hPendingIndicatorBrush = nullptr; }

    // Delete indicator border pens
    if (m_hAckIndicatorBorderPen) { DeleteObject(m_hAckIndicatorBorderPen); m_hAckIndicatorBorderPen = nullptr; }
    if (m_hTimeoutIndicatorBorderPen) { DeleteObject(m_hTimeoutIndicatorBorderPen); m_hTimeoutIndicatorBorderPen = nullptr; }
    if (m_hNackIndicatorBorderPen) { DeleteObject(m_hNackIndicatorBorderPen); m_hNackIndicatorBorderPen = nullptr; }
    if (m_hPendingIndicatorBorderPen) { DeleteObject(m_hPendingIndicatorBorderPen); m_hPendingIndicatorBorderPen = nullptr; }

    DEBUG_LOG("ChatForm: GDI objects destroyed successfully");
}

LRESULT CALLBACK ChatForm::ChatFormProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ChatForm* pThis = nullptr;

    if (message == WM_NCCREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ChatForm*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hWnd = hWnd;
    }
    else
    {
        pThis = (ChatForm*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis)
    {
        return pThis->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT ChatForm::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    // Debug: Log important messages
    if (message == WM_COMMAND || message == WM_CLOSE || message == WM_DESTROY || message == WM_SIZE) {
        std::ostringstream oss;
        oss << "ChatForm: Received message 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << message;
        DEBUG_LOG(oss.str());
    }

    switch (message)
    {
    case WM_CREATE:
        DEBUG_LOG("ChatForm: WM_CREATE received");
        return 0;

    case WM_KEYDOWN:
        return OnKeyDown(wParam);

    case WM_SEND_MESSAGE:
        DEBUG_LOG("ChatForm: WM_SEND_MESSAGE received (Enter key)");
        SendChatMessage();
        break;

    case WM_APP_PEERS_UPDATED:
        return OnPeersUpdated();

    case WM_APP_STATS_UPDATED:
        // Handle stats updates if needed
        return 0;

    case WM_APP_SYSTEM_EVENT:
        return OnSystemEvent(lParam);

    case WM_APP_UPDATE_ACK:
        return OnUpdateAckStatus();

    case WM_APP_NEW_MESSAGES_AVAILABLE:
        return OnNewMessagesAvailable();

    case WM_COMMAND:
        return OnCommand(wParam, lParam);

    case WM_SIZE:
        return OnSize(wParam, lParam);

    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            std::ostringstream oss_mouse;
            oss_mouse << "ChatForm: Left mouse button clicked at (" << x << ", " << y << ")";
            DEBUG_LOG(oss_mouse.str());
        }
        break;

    case WM_ERASEBKGND:
        // Prevent background erasure to eliminate flicker
        return TRUE;

    case WM_PAINT:
        return OnPaint();

    case WM_CLOSE:
        DEBUG_LOG("ChatForm: WM_CLOSE received");
        DestroyWindow(m_hWnd);
        return 0;

    case WM_MEASUREITEM:
        return OnMeasureItem(lParam);

    case WM_DRAWITEM:
        return OnDrawItem(lParam);

    case WM_TIMER:
        return OnTimer(wParam);

    case WM_DESTROY:
        DEBUG_LOG("ChatForm: WM_DESTROY received - posting quit message");
        PostQuitMessage(0);
        return 0;
    }

    // Any message that isn't explicitly handled and returned above
    // should be passed to the default procedure.
    return DefWindowProc(m_hWnd, message, wParam, lParam);
}

// Refactored message handlers for better code organization
LRESULT ChatForm::OnCommand(WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);

    std::ostringstream oss_cmd;
    oss_cmd << "ChatForm: WM_COMMAND received - ID: " << wmId << ", Event: " << wmEvent;
    DEBUG_LOG(oss_cmd.str());

    switch (wmId)
    {
    case static_cast<int>(ControlId::SendButton):
        DEBUG_LOG("ChatForm: Send button clicked");
        SendChatMessage();
        break;
    case static_cast<int>(ControlId::ChatHistory):
        DEBUG_LOG("ChatForm: Chat history notification received");
        // Handle chat history control notifications if needed
        break;
    case static_cast<int>(ControlId::MessageInput):
        DEBUG_LOG("ChatForm: Message input notification received");
        // Handle message input control notifications if needed
        if (wmEvent == EN_CHANGE) {
            DEBUG_LOG("ChatForm: Message input text changed");
        }
        break;
    case IDM_ABOUT:
        DEBUG_LOG("ChatForm: About menu clicked");
        OnAbout();
        break;
    case IDM_CONNECT:
        DEBUG_LOG("ChatForm: Connect menu clicked");
        OnConnect();
        break;
    case IDM_DISCONNECT:
        DEBUG_LOG("ChatForm: Disconnect menu clicked");
        OnDisconnect();
        break;
    case IDM_REFRESH:
        DEBUG_LOG("ChatForm: Refresh menu clicked");
        UpdateConnectionUI();
        break;
    case IDM_EXIT:
        DEBUG_LOG("ChatForm: Exit menu clicked");
        PostQuitMessage(0);
        break;
    default:
        std::ostringstream oss_unknown;
        oss_unknown << "ChatForm: Unknown WM_COMMAND ID: " << wmId << " (0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << wmId << ")";
        DEBUG_LOG(oss_unknown.str());
        break;
    }
    return 0; // WM_COMMAND is fully handled, don't pass to DefWindowProc
}

LRESULT ChatForm::OnNewMessagesAvailable()
{
    DEBUG_LOG("ChatForm: WM_APP_NEW_MESSAGES_AVAILABLE received");
    // Safely pull all messages from the NetworkManager queue
    if (m_networkManager) {
        auto messages = m_networkManager->PopAllMessages();
        for (const auto& queuedMsg : messages) {
            // Filter out system messages that should be debug logs
            std::wstring sender = StringUtils::to_wstring(queuedMsg.sender);
            std::wstring message = StringUtils::to_wstring(queuedMsg.message);

            if (sender == L"System") {
                // Convert system messages to debug logs
                std::string debugMsg = "NetworkManager: " + queuedMsg.message;
                DEBUG_LOG(debugMsg);
            } else {
                // Only show actual user messages in chat
                AddChatMessage(sender, message);
            }
        }
        UpdatePeerDisplay();
    }
    return 0;
}

LRESULT ChatForm::OnSystemEvent(LPARAM lParam)
{
    // This is EXECUTED on the UI Thread
    SystemEventData* data = (SystemEventData*)lParam;

    // All events reaching this handler are UI-relevant (Connection established or Socket closed)
    AddChatMessage(L"System", data->data);

    UpdatePeerDisplay();
    UpdateConnectionUI(); // Update UI state based on connection
    delete data; // Clean up the heap memory
    return 0;
}

LRESULT ChatForm::OnUpdateAckStatus()
{
    DEBUG_LOG("ChatForm: WM_APP_UPDATE_ACK received");
    // Update only pending message acknowledgment statuses for performance
    bool needsRedraw = false;
    if (m_networkManager && !m_pendingMessages.empty()) {
        // Get the MessageTracker from NetworkManager once at the beginning
        auto networkTracker = m_networkManager->GetMessageTracker();
        if (networkTracker) {
            // Create a copy of pending messages to iterate over (in case we modify the set)
            std::unordered_set<std::string> pendingCopy = m_pendingMessages;

            for (const std::string& messageId : pendingCopy) {
                // Find the message in our deque
                auto msgIt = std::find_if(m_chatMessages.begin(), m_chatMessages.end(),
                    [&messageId](const ChatMessage& msg) { return msg.messageId == messageId; });

                if (msgIt != m_chatMessages.end()) {
                    auto& msg = *msgIt;
                    auto ackParties = networkTracker->getAcknowledgingParties(msg.messageId);
                    bool hadAck = msg.hasAck;
                    bool wasTimedOut = msg.isTimedOut;

                    msg.acknowledgingParties = ackParties;
                    msg.hasAck = networkTracker->hasAcknowledgment(msg.messageId);

                    // Check for timeout with protection against extreme time jumps
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - msg.timestamp).count();

                    // Protect against extreme time jumps (e.g., from sleep/hibernate)
                    // If elapsed time is unreasonably large, treat as timeout but don't spam
                    constexpr long long MAX_REASONABLE_ELAPSED = 60 * 60; // 1 hour
                    bool isTimedOut = false;

                    if (elapsed > MAX_REASONABLE_ELAPSED) {
                        // Extreme time jump - likely from sleep/hibernate
                        // Mark as timed out but don't trigger excessive redraws
                        isTimedOut = true;
                        DEBUG_LOG("ChatForm: Extreme time jump detected (" + std::to_string(elapsed) +
                                 "s) for message " + msg.messageId + " - treating as timeout");
                    } else if (elapsed > MessageTracker::MESSAGE_TIMEOUT_SECONDS) {
                        // Normal timeout
                        isTimedOut = true;
                    }

                    msg.isTimedOut = (isTimedOut && !msg.hasAck);

                    if (hadAck != msg.hasAck || wasTimedOut != msg.isTimedOut) {
                        DEBUG_LOG("ChatForm: Message " + msg.messageId + " status changed - hasAck: " +
                                 (msg.hasAck ? "true" : "false") + ", isTimedOut: " +
                                 (msg.isTimedOut ? "true" : "false"));
                        needsRedraw = true;

                        // Remove from pending set if message is acknowledged or timed out
                        if (msg.hasAck || msg.isTimedOut) {
                            m_pendingMessages.erase(msg.messageId);
                            DEBUG_LOG("ChatForm: Removed message " + msg.messageId + " from pending set");
                        }
                    }
                }
            }

            // Only invalidate if something actually changed
            if (needsRedraw && m_hChatListBox) {
                DEBUG_LOG("ChatForm: Invalidating ListBox for redraw");
                InvalidateRect(m_hChatListBox, nullptr, TRUE);
            } else {
                DEBUG_LOG("ChatForm: No changes detected, skipping redraw");
            }
        }
    }
    return 0;
}

LRESULT ChatForm::OnPeersUpdated()
{
    // Now we're safely on the UI thread
    auto peers = NetworkManager::GetInstance().GetKnownPeers();
    m_peerPanel->updatePeers(peers); // Update the panel
    return 0;
}

LRESULT ChatForm::OnSize(WPARAM wParam, LPARAM lParam)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);

    // Calculate split layout: chat on left (70%), peer panel on right (30%)
    int chatWidth = static_cast<int>(width * 0.7);
    int peerWidth = width - chatWidth;

    // Resize chat ListBox
    SetWindowPos(m_hChatListBox, nullptr, 10, 10, chatWidth - 20, height - 80, SWP_NOZORDER);

    // Resize message input
    SetWindowPos(m_hMessageInput, nullptr, 10, height - 60, chatWidth - 90, 25, SWP_NOZORDER);

    // Move send button
    SetWindowPos(m_hSendButton, nullptr, chatWidth - 80, height - 60, 70, 25, SWP_NOZORDER);

    // Position peer panel on the right
    if (m_peerPanel) {
        SetWindowPos(m_peerPanel->GetHandle(), nullptr, chatWidth, 0, peerWidth, height, SWP_NOZORDER);
    }
    return 0;
}

LRESULT ChatForm::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);

    // Get client rect only when needed
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);

    // Use pre-created background brush
    FillRect(hdc, &clientRect, m_hBackgroundBrush);

    // Use pre-created neon pen for grid and brackets
    HPEN oldPen = (HPEN)SelectObject(hdc, m_hNeonPen);

    // Draw grid pattern
    for (int x = 0; x < clientRect.right; x += CyberpunkTheme::GRID_SPACING)
    {
        MoveToEx(hdc, x, 0, nullptr);
        LineTo(hdc, x, clientRect.bottom);
    }
    for (int y = 0; y < clientRect.bottom; y += CyberpunkTheme::GRID_SPACING)
    {
        MoveToEx(hdc, 0, y, nullptr);
        LineTo(hdc, clientRect.right, y);
    }

    // Draw corner brackets using pre-calculated constants
    const int endX = CyberpunkTheme::BRACKET_END_X;
    const int endY = CyberpunkTheme::BRACKET_END_Y;

    // Top-left
    MoveToEx(hdc, CyberpunkTheme::TOP_LEFT_X, CyberpunkTheme::TOP_LEFT_Y, nullptr);
    LineTo(hdc, endX, CyberpunkTheme::TOP_LEFT_Y);
    MoveToEx(hdc, CyberpunkTheme::TOP_LEFT_X, CyberpunkTheme::TOP_LEFT_Y, nullptr);
    LineTo(hdc, CyberpunkTheme::TOP_LEFT_X, endY);

    // Top-right
    MoveToEx(hdc, clientRect.right - CyberpunkTheme::TOP_RIGHT_X, CyberpunkTheme::TOP_RIGHT_Y, nullptr);
    LineTo(hdc, clientRect.right - endX, CyberpunkTheme::TOP_RIGHT_Y);
    MoveToEx(hdc, clientRect.right - CyberpunkTheme::TOP_RIGHT_X, CyberpunkTheme::TOP_RIGHT_Y, nullptr);
    LineTo(hdc, clientRect.right - CyberpunkTheme::TOP_RIGHT_X, endY);

    // Bottom-left
    MoveToEx(hdc, CyberpunkTheme::BOTTOM_LEFT_X, clientRect.bottom - CyberpunkTheme::BOTTOM_LEFT_Y, nullptr);
    LineTo(hdc, endX, clientRect.bottom - CyberpunkTheme::BOTTOM_LEFT_Y);
    MoveToEx(hdc, CyberpunkTheme::BOTTOM_LEFT_X, clientRect.bottom - CyberpunkTheme::BOTTOM_LEFT_Y, nullptr);
    LineTo(hdc, CyberpunkTheme::BOTTOM_LEFT_X, clientRect.bottom - endY);

    // Bottom-right
    MoveToEx(hdc, clientRect.right - CyberpunkTheme::BOTTOM_RIGHT_X, clientRect.bottom - CyberpunkTheme::BOTTOM_RIGHT_Y, nullptr);
    LineTo(hdc, clientRect.right - endX, clientRect.bottom - CyberpunkTheme::BOTTOM_RIGHT_Y);
    MoveToEx(hdc, clientRect.right - CyberpunkTheme::BOTTOM_RIGHT_X, clientRect.bottom - CyberpunkTheme::BOTTOM_RIGHT_Y, nullptr);
    LineTo(hdc, clientRect.right - CyberpunkTheme::BOTTOM_RIGHT_X, clientRect.bottom - endY);

    // Restore original pen
    SelectObject(hdc, oldPen);

    EndPaint(m_hWnd, &ps);
    return 0;
}

LRESULT ChatForm::OnTimer(WPARAM wParam)
{
    // Update acknowledgment statuses periodically
    if (wParam == 1) { // Our timer ID
        PostMessage(m_hWnd, WM_APP_UPDATE_ACK, 0, 0);
    }
    return 0;
}

LRESULT ChatForm::OnKeyDown(WPARAM wParam)
{
    if (wParam == VK_ESCAPE)
    {
        PostQuitMessage(0);
        return 0;
    }
    else if (wParam == VK_F1)
    {
        // Test: Manually trigger send message
        DEBUG_LOG("ChatForm: F1 pressed - manually triggering send");
        AddChatMessage(L"System", L"Test message sent");
        return 0;
    }
    return 0;
}

LRESULT ChatForm::OnMeasureItem(LPARAM lParam)
{
    MEASUREITEMSTRUCT* pMeasureItem = (MEASUREITEMSTRUCT*)lParam;
    if (pMeasureItem->CtlID == static_cast<int>(ControlId::ChatHistory)) {
        OnMeasureItem(pMeasureItem);
        return TRUE;
    }
    return 0;
}

LRESULT ChatForm::OnDrawItem(LPARAM lParam)
{
    DRAWITEMSTRUCT* pDrawItem = (DRAWITEMSTRUCT*)lParam;
    if (pDrawItem->CtlID == static_cast<int>(ControlId::ChatHistory)) {
        OnDrawItem(pDrawItem);
        return TRUE;
    }
    return 0;
}

void ChatForm::InitializeControls()
{
    DEBUG_LOG("ChatForm: InitializeControls() called");

    // Get initial window size for proper layout
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    std::ostringstream oss_size;
    oss_size << "ChatForm: Window size - Width: " << width << ", Height: " << height;
    DEBUG_LOG(oss_size.str());

    // Calculate split layout: chat on left (70%), peer panel on right (30%)
    int chatWidth = static_cast<int>(width * 0.7);

    std::ostringstream oss_width;
    oss_width << "ChatForm: Calculated chat width: " << chatWidth;
    DEBUG_LOG(oss_width.str());

    // Create owner-drawn ListBox for chat messages with cyberpunk styling
    m_hChatListBox = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWVARIABLE | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,
        10, 10, chatWidth - 20, height - 80,
        m_hWnd,
        (HMENU)static_cast<int>(ControlId::ChatHistory),
        m_hInstance,
        nullptr
    );

    if (m_hChatListBox) {
        DEBUG_LOG("ChatForm: Chat ListBox control created successfully");

        // Debug: Check if control is visible
        if (IsWindowVisible(m_hChatListBox)) {
            DEBUG_LOG("ChatForm: Chat ListBox is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Chat ListBox is not visible");
        }

        // Debug: Check control position and size (client coordinates)
        RECT chatRect;
        GetClientRect(m_hChatListBox, &chatRect);
        std::ostringstream oss_chat_rect;
        oss_chat_rect << "ChatForm: Chat ListBox client rect - Left: " << chatRect.left
            << ", Top: " << chatRect.top << ", Right: " << chatRect.right
            << ", Bottom: " << chatRect.bottom;
        DEBUG_LOG(oss_chat_rect.str());
    } else {
        DEBUG_LOG("ChatForm: ERROR - Failed to create chat ListBox control");
    }

    // Set cyberpunk monospace font for chat ListBox
    m_hFont = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas"
    );
    SendMessage(m_hChatListBox, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Create message input field with cyberpunk styling
    m_hMessageInput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        10, height - 60, chatWidth - 90, 25,
        m_hWnd,
        (HMENU)static_cast<int>(ControlId::MessageInput),
        m_hInstance,
        nullptr
    );

    if (m_hMessageInput) {
        DEBUG_LOG("ChatForm: Message input control created successfully");

        // Debug: Check if control is visible
        if (IsWindowVisible(m_hMessageInput)) {
            DEBUG_LOG("ChatForm: Message input is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Message input is not visible");
        }

        // Debug: Check control position and size (client coordinates)
        RECT inputRect;
        GetClientRect(m_hMessageInput, &inputRect);
        std::ostringstream oss_input_rect;
        oss_input_rect << "ChatForm: Message input client rect - Left: " << inputRect.left
            << ", Top: " << inputRect.top << ", Right: " << inputRect.right
            << ", Bottom: " << inputRect.bottom;
        DEBUG_LOG(oss_input_rect.str());
    } else {
        DEBUG_LOG("ChatForm: ERROR - Failed to create message input control");
    }

    // Set font for message input
    SendMessage(m_hMessageInput, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for message input
    SendMessage(m_hMessageInput, EM_SETBKGNDCOLOR, 0, RGB(0, 10, 0)); // Dark green background

    // Subclass the message input to handle Enter key
    SetWindowLongPtr(m_hMessageInput, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(m_hMessageInput, GWLP_WNDPROC));
    SetWindowLongPtr(m_hMessageInput, GWLP_WNDPROC, (LONG_PTR)MessageInputProc);

    // Create send button with cyberpunk styling
    int buttonX = chatWidth - 80;
    int buttonY = height - 60;
    int buttonWidth = 70;
    int buttonHeight = 25;

    std::ostringstream oss3;
    oss3 << "ChatForm: Creating send button at (" << buttonX << ", " << buttonY
        << ") size " << buttonWidth << "x" << buttonHeight;
    DEBUG_LOG(oss3.str());

    m_hSendButton = CreateWindowExW(
        0,
        L"BUTTON",
        L"[SEND]",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        buttonX, buttonY, buttonWidth, buttonHeight,
        m_hWnd,
        (HMENU)static_cast<int>(ControlId::SendButton),
        m_hInstance,
        nullptr
    );

    if (m_hSendButton) {
        DEBUG_LOG("ChatForm: Send button control created successfully");

        // Debug: Check if control is visible
        if (IsWindowVisible(m_hSendButton)) {
            DEBUG_LOG("ChatForm: Send button is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Send button is not visible");
        }

        // Debug: Check control position and size (client coordinates)
        RECT buttonRect;
        GetClientRect(m_hSendButton, &buttonRect);
        std::ostringstream oss_button_rect;
        oss_button_rect << "ChatForm: Send button client rect - Left: " << buttonRect.left
            << ", Top: " << buttonRect.top << ", Right: " << buttonRect.right
            << ", Bottom: " << buttonRect.bottom;
        DEBUG_LOG(oss_button_rect.str());
    } else {
        DEBUG_LOG("ChatForm: ERROR - Failed to create send button control");
    }

    // Set font for send button
    SendMessage(m_hSendButton, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for send button
    SendMessage(m_hSendButton, BM_SETCHECK, BST_UNCHECKED, 0);

    // Force window update to ensure controls are displayed
    UpdateWindow(m_hWnd);
    InvalidateRect(m_hWnd, nullptr, TRUE);

    // Set focus to message input
    if (m_hMessageInput) {
        SetFocus(m_hMessageInput);
        DEBUG_LOG("ChatForm: Initial focus set to message input");

        // Verify the control is enabled and can receive focus
        if (IsWindowEnabled(m_hMessageInput)) {
            DEBUG_LOG("ChatForm: Message input is enabled");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Message input is disabled");
        }

        if (IsWindowVisible(m_hMessageInput)) {
            DEBUG_LOG("ChatForm: Message input is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Message input is not visible");
        }
    } else {
        DEBUG_LOG("ChatForm: ERROR - Cannot set focus, m_hMessageInput is null");
    }

    // Add initial system message after all controls are properly initialized
    AddChatMessage(L"SYSTEM", L"READY ...");
}

void ChatForm::CenterWindow() const
{
    RECT windowRect;
    GetWindowRect(m_hWnd, &windowRect);

    int x, y;

    if (m_hParent)
    {
        RECT parentRect;
        GetWindowRect(m_hParent, &parentRect);
        x = parentRect.left + (parentRect.right - parentRect.left - (windowRect.right - windowRect.left)) / 2;
        y = parentRect.top + (parentRect.bottom - parentRect.top - (windowRect.bottom - windowRect.top)) / 2;
    }
    else
    {
        // Center on screen
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        x = (screenWidth - (windowRect.right - windowRect.left)) / 2;
        y = (screenHeight - (windowRect.bottom - windowRect.top)) / 2;
    }

    SetWindowPos(m_hWnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

bool ChatForm::Show() const
{
    if (m_hWnd)
    {
        DEBUG_LOG("ChatForm: Show() called - showing window");

        // Show the window first
        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);

        // Use the proper three-step foreground window pattern
        BringWindowToForeground(m_hWnd);

        // Set focus to message input for immediate user interaction
        if (m_hMessageInput) {
            SetFocus(m_hMessageInput);
            DEBUG_LOG("ChatForm: Focus set to message input");
        } else {
            DEBUG_LOG("ChatForm: WARNING - m_hMessageInput is null");
        }

        // Debug: Check if window is actually visible and active
        if (IsWindowVisible(m_hWnd)) {
            DEBUG_LOG("ChatForm: Window is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Window is not visible");
        }

        if (GetForegroundWindow() == m_hWnd) {
            DEBUG_LOG("ChatForm: Window is in foreground");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Window is not in foreground");
        }

        return true;
    }
    DEBUG_LOG("ChatForm: Show() called but m_hWnd is null");
    return false;
}

void ChatForm::Hide() const
{
    if (m_hWnd)
    {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

bool ChatForm::IsVisible() const
{
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void ChatForm::AddChatMessage(const std::wstring& sender, const std::wstring& message, const std::string& messageId)
{
    // Check if ListBox is available
    if (!m_hChatListBox) {
        DEBUG_LOG("ChatForm: AddChatMessage called but ListBox not available yet");
        return;
    }

    // Create ChatMessage object
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = message;
    chatMsg.messageId = messageId.empty() ? Message::generateUUID() : messageId;
    chatMsg.isOwnMessage = (sender == L"You");

    // Set colors based on sender using packed format for efficiency
    if (sender == L"System" || sender == L"SYSTEM") {
        chatMsg.packedColors = PACK_COLORS(RGB(255, 100, 100), RGB(255, 150, 150));  // Red for system messages
    } else if (sender == L"You") {
        chatMsg.packedColors = PACK_COLORS(RGB(100, 150, 255), RGB(150, 200, 255));  // Blue for own messages
    } else {
        chatMsg.packedColors = PACK_COLORS(RGB(255, 255, 0), RGB(0, 255, 0));        // Yellow/Green for other users
    }

    // Add to our message deque
    m_chatMessages.push_back(chatMsg);

    // If this is our own message, add it to the pending set for optimized ACK checking
    if (chatMsg.isOwnMessage && !chatMsg.messageId.empty()) {
        m_pendingMessages.insert(chatMsg.messageId);
    }

    // Add item to ListBox (for owner-drawn, we pass empty string)
    LRESULT index = SendMessage(m_hChatListBox, LB_ADDSTRING, 0, (LPARAM)L"");

    // Check if the item was added successfully
    if (index == LB_ERR || index == LB_ERRSPACE) {
        DEBUG_LOG("ChatForm: Failed to add item to ListBox");
        return;
    }

    // Associate the ChatMessage data with the ListBox item
    LRESULT result = SendMessage(m_hChatListBox, LB_SETITEMDATA, index, (LPARAM)&m_chatMessages.back());
    if (result == LB_ERR) {
        DEBUG_LOG("ChatForm: Failed to set item data in ListBox");
    }

    // Auto-scroll to the bottom
    SendMessage(m_hChatListBox, LB_SETTOPINDEX, index, 0);

    // Note: Message tracking is now handled by NetworkManager to ensure
    // the same message ID is used for both sending and ACK tracking
}

void ChatForm::ClearChat()
{
    // Clear the ListBox first
    if (m_hChatListBox) {
        SendMessage(m_hChatListBox, LB_RESETCONTENT, 0, 0);
    }

    // Clear the message deque
    m_chatMessages.clear();

    // Clear the pending messages set
    ClearPendingMessages();

    // Add system message to indicate chat was cleared
    AddChatMessage(L"System", L"Chat cleared.");

    DEBUG_LOG("ChatForm: Chat cleared");
}

void ChatForm::ClearPendingMessages()
{
    m_pendingMessages.clear();
    DEBUG_LOG("ChatForm: Pending messages cleared");
}

void ChatForm::SendChatMessage()
{
    // Get the length of the text first
    int textLength = GetWindowTextLengthW(m_hMessageInput);
    if (textLength == 0)
    {
        return; // No text to send
    }

    // Allocate buffer dynamically based on actual text length
    std::vector<wchar_t> buffer(textLength + 1);
    GetWindowTextW(m_hMessageInput, buffer.data(), textLength + 1);

    // Convert to narrow string using proper UTF-8 conversion
    std::wstring wMessage(buffer.data());
    std::string message = StringUtils::to_string(wMessage);

    // Debug: Log what we're trying to do
    DEBUG_LOG("ChatForm: SendChatMessage called - checking connection state");

    // Send via network manager (async to avoid blocking UI)
    if (m_networkManager && m_networkManager->IsConnected())
    {
        DEBUG_LOG("ChatForm: Connection confirmed - sending message");

        // Create a proper Message object for tracking
        Message msg("You", message);

        // Add to chat history immediately (optimistic UI update) with message ID
        AddChatMessage(L"You", wMessage, msg.messageId);

        // Send the message as JSON (reuse the member JSON object)
        to_json(m_jsonMsg, msg);
        std::string jsonString = m_jsonMsg.dump();

        DEBUG_LOG("ChatForm: Calling SendMessageAsync...");
        m_networkManager->SendMessageAsync(jsonString);
        DEBUG_LOG("ChatForm: SendMessageAsync completed");

        // Note: The NetworkManager will create a wrapper message with a new ID
        // We need to track the actual message ID that gets sent over the network
        // This will be handled by the NetworkManager's callback

        SetWindowTextW(m_hMessageInput, L"");
        SetFocus(m_hMessageInput);

        // Ensure send button remains the default button
        SendMessage(m_hSendButton, BM_SETSTYLE, BS_PUSHBUTTON | BS_DEFPUSHBUTTON, TRUE);
        DEBUG_LOG("ChatForm: Send button set as default after message sent");
    }
    else
    {
        DEBUG_LOG("ChatForm: Not connected - showing not connected message");
        AddChatMessage(L"System", L"Not connected to network");
    }
}


void ChatForm::FocusMessageInput()
{
    if (m_hMessageInput)
    {
        SetFocus(m_hMessageInput);
    }
}

void ChatForm::UpdatePeerDisplay()
{
    if (m_peerPanel && m_networkManager)
    {
        auto peers = m_networkManager->GetKnownPeers();
        m_peerPanel->updatePeers(peers);
    }
}

void ChatForm::OnAbout() const
{
    DialogBox(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_hWnd, About);
}

void ChatForm::OnConnect()
{
    if (m_connectDialog)
    {
        m_connectDialog->Show();
    }
}

void ChatForm::OnDisconnect()
{
    DEBUG_LOG("ChatForm: OnDisconnect called - checking connection state");

    if (m_networkManager && m_networkManager->IsConnected())
    {
        try
        {
            // Log disconnect attempt
            AddChatMessage(L"SYSTEM", L"DISCONNECTING FROM NEURAL NETWORK...");

            // Disconnect from network
            m_networkManager->Disconnect();

            // Update UI to reflect disconnected state
            UpdateConnectionUI();

            // Log successful disconnect
            AddChatMessage(L"SYSTEM", L"NEURAL LINK TERMINATED SUCCESSFULLY");
            AddChatMessage(L"SYSTEM", L"READY FOR NEW CONNECTION...");
        }
        catch (const std::exception& e)
        {
            // Log error to chat
            std::string errorMsg = "DISCONNECT ERROR: " + std::string(e.what());
            std::wstring wErrorMsg = StringUtils::to_wstring(errorMsg);
            AddChatMessage(L"ERROR", wErrorMsg);

            // Show error dialog
            MessageBoxW(m_hWnd,
                       L"An error occurred during disconnect. Check the chat log for details.",
                       L"Disconnect Error",
                       MB_OK | MB_ICONERROR);
        }
        catch (...)
        {
            // Log unknown error to chat
            AddChatMessage(L"ERROR", L"DISCONNECT ERROR: Unknown error occurred");

            // Show error dialog
            MessageBoxW(m_hWnd,
                       L"An unknown error occurred during disconnect. Check the chat log for details.",
                       L"Disconnect Error",
                       MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        AddChatMessage(L"SYSTEM", L"NOT CURRENTLY CONNECTED TO NEURAL NETWORK");
    }
}

void ChatForm::UpdateConnectionUI()
{
    if (m_networkManager)
    {
        bool connected = m_networkManager->IsConnected();
        DEBUG_LOG("ChatForm: UpdateConnectionUI called - connected: " + std::string(connected ? "true" : "false"));

        // Restore socket callback after reconnecting
        if (connected) {
            m_networkManager->SetSocketEventCallback([this](SocketEventType eventType, const std::string& data) {
                this->OnSocketEvent(eventType, data);
            });
            DEBUG_LOG("ChatForm: Socket callback restored after connection");
        }

        EnableDisconnectControls(connected);

        // Ensure proper button focus when connection state changes
        if (connected) {
            SendMessage(m_hSendButton, BM_SETSTYLE, BS_PUSHBUTTON | BS_DEFPUSHBUTTON, TRUE);
            DEBUG_LOG("ChatForm: Send button set as default after connection");
        }
    }
}

void ChatForm::EnableDisconnectControls(bool enable)
{
    // Enable/disable disconnect menu item
    HMENU hMenu = GetMenu(m_hWnd);
    if (hMenu)
    {
        EnableMenuItem(hMenu, IDM_DISCONNECT,
                      enable ? MF_ENABLED : MF_GRAYED);
        DrawMenuBar(m_hWnd);
    }
}

HWND ChatForm::GetConnectDialogHandle() const
{
    if (m_connectDialog) {
        return m_connectDialog->GetHandle();
    }
    return nullptr;
}

void ChatForm::OnNetworkMessage(const std::string& sender, const std::string& message)
{
    // This is EXECUTED on the Network Thread

    DEBUG_LOG("ChatForm: OnNetworkMessage received from: " + sender + ", message: " + message);

    // Check if this is an acknowledgment message or regular JSON message
    if (m_networkManager) {
        // Try to parse as JSON
        try {
            nlohmann::json jsonMsg = nlohmann::json::parse(message);
            // Use optimized JSON utility to avoid redundant key hashing
            std::string msgType = JsonUtils::getString(jsonMsg, "type");
            if (!msgType.empty()) {
                DEBUG_LOG("ChatForm: Parsed JSON message type: " + msgType);

                if (msgType == "ACK" || msgType == "NACK") {
                    // This is an acknowledgment, process it
                    Message ackMsg;
                    from_json(jsonMsg, ackMsg);
                    DEBUG_LOG("ChatForm: Processing " + msgType + " for message: " + ackMsg.originalMessageId.value_or("unknown"));

                    // Use NetworkManager's MessageTracker
                    if (m_networkManager) {
                        auto networkTracker = m_networkManager->GetMessageTracker();
                        if (networkTracker) {
                            networkTracker->processAcknowledgment(ackMsg);
                        }
                    }

                    // Update UI on main thread
                    PostMessage(m_hWnd, WM_APP_UPDATE_ACK, 0, 0);
                    DEBUG_LOG("ChatForm: Posted WM_APP_UPDATE_ACK to UI thread");
                    return; // Don't display ACK messages in chat and don't process further
                } else if (msgType == "CHAT") {
                    // This is a regular chat message, extract the content
                    Message chatMsg;
                    from_json(jsonMsg, chatMsg);

                    // Don't process our own messages here - they're already in the UI from SendChatMessage()
                    if (chatMsg.senderId == m_networkManager->GetSenderId()) {
                        DEBUG_LOG("ChatForm: OnNetworkMessage ignoring own message to prevent duplication: " + chatMsg.messageId);
                        return;
                    }

                    // Queue the message for UI thread processing (notify-and-pull pattern)
                    if (m_networkManager) {
                        // Use the NetworkManager's queue instead of direct PostMessage
                        // This will be handled by the new WM_APP_NEW_MESSAGES_AVAILABLE handler
                        return;
                    }
                    return;
                }
            }
        } catch (const std::exception& e) {
            // IF PARSING FAILS, it's a plain string message (e.g., "Network connection established").
            // Handle it gracefully instead of just logging an error.
            DEBUG_LOG("ChatForm: JSON parsing failed: " + std::string(e.what()) + ". Treating as plain text.");
            (void)e; // Suppress unused variable warning

            // Fall through to the plain text handling logic below.
        } catch (...) {
            DEBUG_LOG("ChatForm: JSON parsing failed with unknown exception");
        }
    }

    // Fallback for non-JSON messages - queue for UI thread processing
    if (m_networkManager) {
        // Use the NetworkManager's queue instead of direct PostMessage
        // This will be handled by the new WM_APP_NEW_MESSAGES_AVAILABLE handler
        return;
    }
}

void ChatForm::OnSocketEvent(SocketEventType eventType, const std::string& data)
{
    // This is EXECUTED on the Network Thread

    DEBUG_LOG("ChatForm: OnSocketEvent received - eventType: " + std::to_string(static_cast<int>(eventType)) + ", data: " + data);

    // Handle "Raw JSON" event to capture message ID
    if (eventType == SocketEventType::RawJsonReceived && m_networkManager) {
        ProcessRawJsonEvent(data);
    }

    // Only send UI events for important system messages (Connection established and Socket closed)
    if (eventType == SocketEventType::Connected || eventType == SocketEventType::SocketClosed) {
        // 1. Allocate event data on the heap
        SystemEventData* eventData = new SystemEventData();
        eventData->eventType = static_cast<int>(eventType);
        eventData->data = StringUtils::to_wstring(data);

        // 2. Post the POINTER to the UI thread
        PostMessage(m_hWnd, WM_APP_SYSTEM_EVENT, 0, (LPARAM)eventData);
    } else {
        // All other system events are debug information only
        std::string debugMsg = "NetworkManager: Event " + std::to_string(static_cast<int>(eventType)) + " - " + data;
        DEBUG_LOG(debugMsg);
    }
}

void ChatForm::ProcessRawJsonEvent(const std::string& data)
{
    DEBUG_LOG("ChatForm: Processing RawJsonReceived event");
    try {
        // Parse the JSON message directly - no prefix extraction needed
        DEBUG_LOG("ChatForm: Received JSON data: " + data);

        nlohmann::json message = nlohmann::json::parse(data);
        DEBUG_LOG("ChatForm: Successfully parsed message JSON");

        // Handle different message types
        if (message.contains("type")) {
            std::string messageType = message["type"];
            if (messageType == "CHAT") {
                ProcessChatMessage(message);
            } else if (messageType == "ACK") {
                ProcessAckMessage(message);
            } else {
                DEBUG_LOG("ChatForm: Unknown message type: " + messageType);
            }
        } else {
            DEBUG_LOG("ChatForm: Message doesn't contain required fields");
        }
    } catch (const std::exception& e) {
        DEBUG_LOG("ChatForm: Failed to parse message: " + std::string(e.what()));
        (void)e; // Suppress unused variable warning
    }
}

void ChatForm::ProcessChatMessage(const nlohmann::json& message)
{
    using namespace nlohmann::literals;

    if (!message.contains("messageId") || !message.contains("body") || !message.contains("senderId")) {
        DEBUG_LOG("ChatForm: CHAT message missing required fields");
        return;
    }

    // Use JSON pointer literals for efficient property access
    std::string messageId = message["/messageId"_json_pointer];
    std::string body = message["/body"_json_pointer];
    std::string senderId = message["/senderId"_json_pointer];

    DEBUG_LOG("ChatForm: CHAT message - ID: " + messageId + ", senderId: " + senderId);

    // Don't process our own messages here - they're already in the UI from SendChatMessage()
    if (senderId == m_networkManager->GetSenderId()) {
        DEBUG_LOG("ChatForm: Ignoring own message to prevent duplication: " + messageId);
        return; // Exit early to prevent duplicate processing
    }

    DEBUG_LOG("ChatForm: Processing message from other user: " + senderId);
}

void ChatForm::ProcessAckMessage(const nlohmann::json& message)
{
    using namespace nlohmann::literals;

    // This is an ACK message - process it directly using JSON pointer literals
    std::string ackMessageId = message["/messageId"_json_pointer];
    std::string originalMessageId = message["/originalMessageId"_json_pointer];
    std::string ackSenderId = message["/senderId"_json_pointer];

    DEBUG_LOG("ChatForm: Processing ACK - Message ID: " + ackMessageId + ", Original ID: " + originalMessageId + ", From: " + ackSenderId);

    // Create ACK message for MessageTracker
    Message ackMsg;
    ackMsg.senderId = ackSenderId;
    ackMsg.messageId = ackMessageId;
    ackMsg.type = MessageType::ACK;
    ackMsg.originalMessageId = originalMessageId;
    ackMsg.body = message["/body"_json_pointer];

    // Use NetworkManager's MessageTracker
    if (m_networkManager) {
        auto networkTracker = m_networkManager->GetMessageTracker();
        if (networkTracker) {
            networkTracker->processAcknowledgment(ackMsg);
        }
    }
    DEBUG_LOG("ChatForm: Processed ACK for message: " + originalMessageId);

    // Update UI on main thread
    PostMessage(m_hWnd, WM_APP_UPDATE_ACK, 0, 0);
    DEBUG_LOG("ChatForm: Posted WM_APP_UPDATE_ACK to UI thread");
}

// About dialog procedure
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }

    return (INT_PTR)FALSE;
}

// Helper function to bring window to foreground using proper Windows pattern
void ChatForm::BringWindowToForeground(HWND hWnd)
{
    if (!hWnd) return;

    // Step 1: Try the simple approach first
    if (SetForegroundWindow(hWnd)) {
        DEBUG_LOG("ChatForm: SetForegroundWindow succeeded");
        return;
    }

    // Step 2: If simple approach fails, use FlashWindow to get user's attention
    DEBUG_LOG("ChatForm: SetForegroundWindow failed, using FlashWindow");
    FlashWindow(hWnd, TRUE);

    // Step 3: Use the forceful method with AttachThreadInput
    HWND foregroundWnd = GetForegroundWindow();
    if (!foregroundWnd) {
        DEBUG_LOG("ChatForm: No foreground window, using basic approach");
        BringWindowToTop(hWnd);
        ShowWindow(hWnd, SW_SHOW);
        SetForegroundWindow(hWnd);
        return;
    }

    // Get thread IDs
    DWORD foregroundThreadId = GetWindowThreadProcessId(foregroundWnd, nullptr);
    DWORD currentThreadId = GetCurrentThreadId();

    // If we're not the foreground thread, attach to it
    if (currentThreadId != foregroundThreadId) {
        DEBUG_LOG("ChatForm: Attaching to foreground thread");
        AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);
    }

    // Bring the window to the top and set it as foreground
    BringWindowToTop(hWnd);
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);

    // Detach from the foreground thread's input queue (CRITICAL!)
    if (currentThreadId != foregroundThreadId) {
        DEBUG_LOG("ChatForm: Detaching from foreground thread");
        AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);
    }

    // Additional attempt: Try to make the window topmost temporarily
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    DEBUG_LOG("ChatForm: BringWindowToForeground completed");
}

// Helper function to resolve proper module handle
HINSTANCE ChatForm::ResolveModuleHandle(HINSTANCE hInstance)
{
    // Get the proper module handle - use provided instance or fall back to main executable
    HINSTANCE hModule = hInstance;
    if (!hModule) {
        // Try to get the main executable handle
        hModule = GetModuleHandle(L"Soldato.exe");
        if (!hModule) {
            hModule = GetModuleHandle(nullptr); // Last resort fallback
        }
    }
    return hModule;
}

// Owner-drawn ListBox implementation
void ChatForm::OnMeasureItem(MEASUREITEMSTRUCT* pMeasureItem)
{
    if (pMeasureItem->itemID >= 0 && pMeasureItem->itemID < static_cast<int>(m_chatMessages.size())) {
        const ChatMessage& message = m_chatMessages[pMeasureItem->itemID];

        // Get device context for text measurement
        HDC hdc = GetDC(m_hChatListBox);
        RECT itemRect = {0, 0, static_cast<LONG>(pMeasureItem->itemWidth), 0};

        // Combine sender and message for calculation
        std::wstring fullText = L"[" + message.sender + L"]: " + message.message;

        // Use DrawText with DT_CALCRECT to get the required height
        DrawTextW(hdc, fullText.c_str(), static_cast<int>(fullText.length()), &itemRect, DT_WORDBREAK | DT_CALCRECT);

        ReleaseDC(m_hChatListBox, hdc);

        // Set the height with padding for acknowledgment indicator
        pMeasureItem->itemHeight = itemRect.bottom - itemRect.top + 20;
    }
}

void ChatForm::OnDrawItem(DRAWITEMSTRUCT* pDrawItem)
{
    if (pDrawItem->itemID == -1 || pDrawItem->itemID >= static_cast<int>(m_chatMessages.size())) {
        return;
    }

    const ChatMessage& message = m_chatMessages[pDrawItem->itemID];

    // Cache the HDC to avoid multiple memory reads
    HDC hdc = pDrawItem->hDC;

    // Save the original device context state
    int savedDC = SaveDC(hdc);

    // Set up clipping to prevent drawing outside the item bounds
    IntersectClipRect(hdc, pDrawItem->rcItem.left, pDrawItem->rcItem.top,
                     pDrawItem->rcItem.right, pDrawItem->rcItem.bottom);

    // Create a memory DC for double buffering to reduce flickering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc,
        pDrawItem->rcItem.right - pDrawItem->rcItem.left,
        pDrawItem->rcItem.bottom - pDrawItem->rcItem.top);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Offset the rectangle for the memory DC
    RECT memRect = pDrawItem->rcItem;
    OffsetRect(&memRect, -pDrawItem->rcItem.left, -pDrawItem->rcItem.top);

    // Draw to memory DC
    DrawMessageBackground(memDC, memRect, message);
    DrawAckIndicator(memDC, memRect, message);
    DrawMessageText(memDC, memRect, message);

    // Copy from memory DC to screen
    BitBlt(hdc, pDrawItem->rcItem.left, pDrawItem->rcItem.top,
           pDrawItem->rcItem.right - pDrawItem->rcItem.left,
           pDrawItem->rcItem.bottom - pDrawItem->rcItem.top,
           memDC, 0, 0, SRCCOPY);

    // Clean up
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    // Restore the device context state
    RestoreDC(hdc, savedDC);
}

void ChatForm::DrawMessageBackground(HDC hdc, const RECT& rect, const ChatMessage& message)
{
    // Choose pre-created brush and pen based on acknowledgment status
    HBRUSH hBrushToUse;
    HPEN hPenToUse;

    if (message.isTimedOut) {
        hBrushToUse = m_hTimeoutBgBrush;
        hPenToUse = m_hTimeoutBorderPen;
    } else if (message.hasNack) {
        hBrushToUse = m_hNackBgBrush;
        hPenToUse = m_hNackBorderPen;
    } else if (message.hasAck) {
        hBrushToUse = m_hAckBgBrush;
        hPenToUse = m_hAckBorderPen;
    } else if (message.isOwnMessage) {
        hBrushToUse = m_hOwnMessageBgBrush;
        hPenToUse = m_hOwnMessageBorderPen;
    } else {
        hBrushToUse = m_hDefaultBgBrush;
        hPenToUse = m_hDefaultBorderPen;
    }

    // Draw background using pre-created brush
    FillRect(hdc, &rect, hBrushToUse);

    // Draw subtle border using pre-created pen
    HPEN oldPen = (HPEN)SelectObject(hdc, hPenToUse);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldPen);
}

void ChatForm::DrawAckIndicator(HDC hdc, const RECT& rect, const ChatMessage& message)
{
    if (!message.isOwnMessage) return; // Only show indicators for own messages

    RECT indicatorRect = rect;
    indicatorRect.left = rect.right - 20;
    indicatorRect.top += 2;
    indicatorRect.bottom -= 2;
    indicatorRect.right -= 2;

    // Determine message status once
    MessageStatus status;
    if (message.isTimedOut) {
        status = MessageStatus::TimedOut;
    } else if (message.hasNack) {
        status = MessageStatus::NegativelyAcknowledged;
    } else if (message.hasAck) {
        status = MessageStatus::Acknowledged;
    } else {
        status = MessageStatus::Pending;
    }

    // Choose pre-created brush, pen, and text color based on status
    HBRUSH hBrushToUse;
    HPEN hPenToUse;
    COLORREF textColor;

    switch (status) {
        case MessageStatus::TimedOut:
            hBrushToUse = m_hTimeoutIndicatorBrush;
            hPenToUse = m_hTimeoutIndicatorBorderPen;
            textColor = RGB(255, 255, 255);  // White text for better contrast on red
            break;
        case MessageStatus::NegativelyAcknowledged:
            hBrushToUse = m_hNackIndicatorBrush;
            hPenToUse = m_hNackIndicatorBorderPen;
            textColor = RGB(0, 0, 0);  // Black text for contrast on light red
            break;
        case MessageStatus::Acknowledged:
            hBrushToUse = m_hAckIndicatorBrush;
            hPenToUse = m_hAckIndicatorBorderPen;
            textColor = RGB(0, 0, 0);  // Black text for contrast on green
            break;
        case MessageStatus::Pending:
        default:
            hBrushToUse = m_hPendingIndicatorBrush;
            hPenToUse = m_hPendingIndicatorBorderPen;
            textColor = RGB(0, 0, 0);  // Black text for contrast on yellow
            break;
    }

    // Draw indicator background using pre-created brush
    FillRect(hdc, &indicatorRect, hBrushToUse);

    // Draw border using pre-created pen
    HPEN oldPen = (HPEN)SelectObject(hdc, hPenToUse);
    Rectangle(hdc, indicatorRect.left, indicatorRect.top, indicatorRect.right, indicatorRect.bottom);
    SelectObject(hdc, oldPen);

    // Draw status symbol with appropriate text color
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);

    std::wstring statusSymbol = GetStatusSymbol(status);

    // Center the symbol in the indicator
    RECT textRect = indicatorRect;
    DrawTextW(hdc, statusSymbol.c_str(), static_cast<int>(statusSymbol.length()), &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void ChatForm::DrawMessageText(HDC hdc, const RECT& rect, const ChatMessage& message)
{
    // Set up for drawing text
    SetBkMode(hdc, TRANSPARENT);

    // Draw the sender's name in its color
    SetTextColor(hdc, UNPACK_SENDER_COLOR(message.packedColors));
    RECT senderRect = rect;
    senderRect.left += 5;
    senderRect.right -= 25; // Leave space for ACK indicator

    // Use DT_CALCRECT to find where the sender text ends
    std::wstring senderText = L"[" + message.sender + L"]: ";
    DrawTextW(hdc, senderText.c_str(), -1, &senderRect, DT_SINGLELINE | DT_CALCRECT);
    // Reuse senderRect for actual drawing
    DrawTextW(hdc, senderText.c_str(), -1, &senderRect, DT_SINGLELINE);

    // Draw the message text - reuse senderRect as messageRect
    senderRect.left += (senderRect.right - senderRect.left);
    senderRect.right = rect.right - 25; // Leave space for ACK indicator

    SetTextColor(hdc, UNPACK_MESSAGE_COLOR(message.packedColors));
    DrawTextW(hdc, message.message.c_str(), -1, &senderRect, DT_WORDBREAK);
}

void ChatForm::UpdateMessageAckStatus(const std::string& messageId)
{
    // Find the message in our deque and update its acknowledgment status
    for (auto& msg : m_chatMessages) {
        if (msg.messageId == messageId) {
            if (m_networkManager) {
                auto networkTracker = m_networkManager->GetMessageTracker();
                if (networkTracker) {
                    auto ackParties = networkTracker->getAcknowledgingParties(messageId);
                    msg.acknowledgingParties = ackParties;
                    msg.hasAck = networkTracker->hasAcknowledgment(messageId);
                }

                // Check for NACK (this would need to be implemented in MessageTracker)
                // For now, we'll assume no NACK unless explicitly set
            }
            break;
        }
    }

    // Invalidate the ListBox to trigger redraw
    if (m_hChatListBox) {
        InvalidateRect(m_hChatListBox, nullptr, TRUE);
    }
}
