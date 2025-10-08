#include "framework.h"
#include "ChatForm.h"
#include "Resource.h"
#include "StringUtils.h"
#include "DebugUtils.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
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

ChatForm::ChatForm(HWND parent, HINSTANCE hInstance) : m_hParent(parent), m_hWnd(nullptr), m_networkManager(nullptr), m_connectDialog(nullptr), m_peerPanel(nullptr), m_hFont(nullptr), m_hInstance(hInstance)
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
        m_networkManager->SetSocketEventCallback([this](int eventType, const std::string& data) {
            this->OnSocketEvent(eventType, data);
        });

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
        char debugMsg[256];
        sprintf_s(debugMsg, "ChatForm: Received message 0x%04X", message);
        DEBUG_LOG(debugMsg);
    }

    switch (message)
    {
    case WM_CREATE:
        DEBUG_LOG("ChatForm: WM_CREATE received");
        return 0;


    case WM_KEYDOWN:
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
        break;

    case WM_SEND_MESSAGE:
        DEBUG_LOG("ChatForm: WM_SEND_MESSAGE received (Enter key)");
        SendChatMessage();
        break;

    case WM_APP_PEERS_UPDATED:
        {
            // Now we're safely on the UI thread
            auto peers = NetworkManager::GetInstance().GetKnownPeers();
            m_peerPanel->updatePeers(peers); // Update the panel
            return 0;
        }

    case WM_APP_STATS_UPDATED:
        {
            // Handle stats updates if needed
            return 0;
        }

    case WM_APP_NEW_MESSAGE:
        {
            // This is EXECUTED on the UI Thread
            MessageData* data = (MessageData*)lParam;

            // Filter out system messages that should be debug logs
            std::wstring sender = data->sender;
            std::wstring message = data->message;

            if (sender == L"System") {
                // Convert system messages to debug logs
                std::string debugMsg = "NetworkManager: " + std::string(message.begin(), message.end());
                DEBUG_LOG(debugMsg);
            } else {
                // Only show actual user messages in chat
                AddChatMessage(sender, message);
            }

            UpdatePeerDisplay();
            delete data; // Clean up the heap memory
            return 0;
        }

    case WM_APP_SYSTEM_EVENT:
        {
            // This is EXECUTED on the UI Thread
            SystemEventData* data = (SystemEventData*)lParam;

            // Only show important system messages in chat, log others as debug
            if (data->eventType == 1 || data->eventType == 3) {
                // Event type 1: Connection established
                // Event type 3: Socket closed by remote
                AddChatMessage(L"System", data->data);
            } else {
                // All other system events are debug information
                std::string debugMsg = "NetworkManager: Event " + std::to_string(data->eventType) + " - " +
                                     std::string(data->data.begin(), data->data.end());
                DEBUG_LOG(debugMsg);
            }

            UpdatePeerDisplay();
            UpdateConnectionUI(); // Update UI state based on connection
            delete data; // Clean up the heap memory
            return 0;
        }

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            char debugMsg[256];
            sprintf_s(debugMsg, "ChatForm: WM_COMMAND received - ID: %d, Event: %d", wmId, wmEvent);
            DEBUG_LOG(debugMsg);

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
                sprintf_s(debugMsg, "ChatForm: Unknown WM_COMMAND ID: %d (0x%04X)", wmId, wmId);
                DEBUG_LOG(debugMsg);
                break;
            }
        }
        return 0; // WM_COMMAND is fully handled, don't pass to DefWindowProc

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Calculate split layout: chat on left (70%), peer panel on right (30%)
            int chatWidth = static_cast<int>(width * 0.7);
            int peerWidth = width - chatWidth;

            // Resize chat history
            SetWindowPos(m_hChatHistory, nullptr, 10, 10, chatWidth - 20, height - 80, SWP_NOZORDER);

            // Resize message input
            SetWindowPos(m_hMessageInput, nullptr, 10, height - 60, chatWidth - 90, 25, SWP_NOZORDER);

            // Move send button
            SetWindowPos(m_hSendButton, nullptr, chatWidth - 80, height - 60, 70, 25, SWP_NOZORDER);

            // Position peer panel on the right
            if (m_peerPanel) {
                SetWindowPos(m_peerPanel->GetHandle(), nullptr, chatWidth, 0, peerWidth, height, SWP_NOZORDER);
            }
        }
        break;

    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            char debugMsg[256];
            sprintf_s(debugMsg, "ChatForm: Left mouse button clicked at (%d, %d)", x, y);
            DEBUG_LOG(debugMsg);
        }
        break;

    case WM_ERASEBKGND:
        // Prevent background erasure to eliminate flicker
        return TRUE;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);

            // Get client rect only when needed
            RECT clientRect;
            GetClientRect(m_hWnd, &clientRect);

            // Create and use background brush
            HBRUSH darkBrush = CreateSolidBrush(CyberpunkTheme::BACKGROUND_COLOR);
            FillRect(hdc, &clientRect, darkBrush);
            DeleteObject(darkBrush);

            // Create and select pen for grid and brackets
            HPEN neonPen = CreatePen(PS_SOLID, 1, CyberpunkTheme::GRID_COLOR);
            HPEN oldPen = (HPEN)SelectObject(hdc, neonPen);

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

            // Restore original pen and cleanup
            SelectObject(hdc, oldPen);
            DeleteObject(neonPen);

            EndPaint(m_hWnd, &ps);
        }
        break;

    case WM_CLOSE:
        DEBUG_LOG("ChatForm: WM_CLOSE received");
        DestroyWindow(m_hWnd);
        return 0;

    case WM_DESTROY:
        DEBUG_LOG("ChatForm: WM_DESTROY received - posting quit message");
        PostQuitMessage(0);
        return 0;
    }

    // Any message that isn't explicitly handled and returned above
    // should be passed to the default procedure.
    return DefWindowProc(m_hWnd, message, wParam, lParam);
}

void ChatForm::InitializeControls()
{
    DEBUG_LOG("ChatForm: InitializeControls() called");

    // Get initial window size for proper layout
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    char debugMsg[256];
    sprintf_s(debugMsg, "ChatForm: Window size - Width: %d, Height: %d", width, height);
    DEBUG_LOG(debugMsg);

    // Calculate split layout: chat on left (70%), peer panel on right (30%)
    int chatWidth = static_cast<int>(width * 0.7);

    sprintf_s(debugMsg, "ChatForm: Calculated chat width: %d", chatWidth);
    DEBUG_LOG(debugMsg);

    // Create chat history (read-only Rich Edit Control) with cyberpunk styling
    m_hChatHistory = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RichEdit",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, chatWidth - 20, height - 80,
        m_hWnd,
        (HMENU)static_cast<int>(ControlId::ChatHistory),
        m_hInstance,
        nullptr
    );

    if (m_hChatHistory) {
        DEBUG_LOG("ChatForm: Chat history control created successfully");

        // Debug: Check if control is visible
        if (IsWindowVisible(m_hChatHistory)) {
            DEBUG_LOG("ChatForm: Chat history is visible");
        } else {
            DEBUG_LOG("ChatForm: WARNING - Chat history is not visible");
        }

        // Debug: Check control position and size (client coordinates)
        RECT chatRect;
        GetClientRect(m_hChatHistory, &chatRect);
        char debugMsg[256];
        sprintf_s(debugMsg, "ChatForm: Chat history client rect - Left: %d, Top: %d, Right: %d, Bottom: %d",
                 chatRect.left, chatRect.top, chatRect.right, chatRect.bottom);
        DEBUG_LOG(debugMsg);
    } else {
        DEBUG_LOG("ChatForm: ERROR - Failed to create chat history control");
    }


    // Set cyberpunk monospace font for chat history
    m_hFont = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas"
    );
    SendMessage(m_hChatHistory, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for chat history
    SetWindowLongPtr(m_hChatHistory, GWL_EXSTYLE, GetWindowLongPtr(m_hChatHistory, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);
    SendMessage(m_hChatHistory, EM_SETBKGNDCOLOR, 0, RGB(0, 10, 0)); // Dark green background

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
        char debugMsg[256];
        sprintf_s(debugMsg, "ChatForm: Message input client rect - Left: %d, Top: %d, Right: %d, Bottom: %d",
                 inputRect.left, inputRect.top, inputRect.right, inputRect.bottom);
        DEBUG_LOG(debugMsg);
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

    sprintf_s(debugMsg, "ChatForm: Creating send button at (%d, %d) size %dx%d",
              buttonX, buttonY, buttonWidth, buttonHeight);
    DEBUG_LOG(debugMsg);

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
        char debugMsg[256];
        sprintf_s(debugMsg, "ChatForm: Send button client rect - Left: %d, Top: %d, Right: %d, Bottom: %d",
                 buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom);
        DEBUG_LOG(debugMsg);
    } else {
        DEBUG_LOG("ChatForm: ERROR - Failed to create send button control");
    }

    // Set font for send button
    SendMessage(m_hSendButton, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for send button
    SendMessage(m_hSendButton, BM_SETCHECK, BST_UNCHECKED, 0);


    AddChatMessage(L"SYSTEM", L"READY ...");

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

void ChatForm::AddChatMessage(const std::wstring& sender, const std::wstring& message)
{
    HWND hChat = m_hChatHistory;
    SendMessage(hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1); // Move to the end

    // Set color for sender (yellow)
    CHARFORMAT2W cf_sender{};
    cf_sender.cbSize = sizeof(cf_sender);
    cf_sender.dwMask = CFM_COLOR | CFM_BOLD;
    cf_sender.dwEffects = CFE_BOLD;
    cf_sender.crTextColor = RGB(255, 255, 0);
    SendMessage(hChat, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf_sender);

    std::wstring sender_part = L"[" + sender + L"]: ";
    SendMessage(hChat, EM_REPLACESEL, FALSE, (LPARAM)sender_part.c_str());

    // Set color for message (green)
    CHARFORMAT2W cf_message{};
    cf_message.cbSize = sizeof(cf_message);
    cf_message.dwMask = CFM_COLOR;
    cf_message.dwEffects = 0; // Not bold
    cf_message.crTextColor = RGB(0, 255, 0);
    SendMessage(hChat, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf_message);

    std::wstring message_part = message + L"\r\n";
    SendMessage(hChat, EM_REPLACESEL, FALSE, (LPARAM)message_part.c_str());

    // Scroll to bottom
    SendMessage(hChat, EM_SCROLL, SB_BOTTOM, 0);

    // Store formatted message for compatibility
    std::wstring formattedMessage = sender_part + message_part;
    m_messages.push_back(formattedMessage);
}

void ChatForm::SendChatMessage()
{
    wchar_t buffer[1024];
    GetWindowTextW(m_hMessageInput, buffer, 1024);

    if (wcslen(buffer) > 0)
    {
        // Convert to narrow string using proper UTF-8 conversion
        std::wstring wMessage(buffer);
        std::string message = StringUtils::to_string(wMessage);

        // Debug: Log what we're trying to do
        DEBUG_LOG("ChatForm: SendChatMessage called - checking connection state");

        // Send via network manager (async to avoid blocking UI)
        if (m_networkManager && m_networkManager->IsConnected())
        {
            DEBUG_LOG("ChatForm: Connection confirmed - sending message");
            DEBUG_LOG("ChatForm: Calling SendMessageAsync...");
            m_networkManager->SendMessageAsync(message);
            DEBUG_LOG("ChatForm: SendMessageAsync completed");

            // Add to chat history immediately (optimistic UI update)
            AddChatMessage(L"You", buffer);
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
}

void ChatForm::ClearChat()
{
    SetWindowTextW(m_hChatHistory, L"");
    m_messages.clear();
    AddChatMessage(L"System", L"Chat cleared.");
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
            m_networkManager->SetSocketEventCallback([this](int eventType, const std::string& data) {
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

    // 1. Allocate message data on the heap
    MessageData* data = new MessageData();
    data->sender = StringUtils::to_wstring(sender);
    data->message = StringUtils::to_wstring(message);

    // 2. Post the POINTER to the UI thread
    PostMessage(m_hWnd, WM_APP_NEW_MESSAGE, 0, (LPARAM)data);
}

void ChatForm::OnSocketEvent(int eventType, const std::string& data)
{
    // This is EXECUTED on the Network Thread

    // 1. Allocate event data on the heap
    SystemEventData* eventData = new SystemEventData();
    eventData->eventType = eventType;
    eventData->data = StringUtils::to_wstring(data);

    // 2. Post the POINTER to the UI thread
    PostMessage(m_hWnd, WM_APP_SYSTEM_EVENT, 0, (LPARAM)eventData);
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
