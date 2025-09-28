#include "framework.h"
#include "ChatForm.h"
#include "Resource.h"
#include "StringUtils.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "comctl32.lib")

// Forward declaration
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Control IDs
#define ID_CHAT_HISTORY    1001
#define ID_MESSAGE_INPUT   1002
#define ID_SEND_BUTTON     1003

// Custom message for Enter key
#define WM_SEND_MESSAGE    (WM_USER + 1)

// Subclass procedure for message input
LRESULT CALLBACK MessageInputProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CHAR && wParam == VK_RETURN)
    {
        // Get the parent window and send custom message
        HWND parent = GetParent(hWnd);
        if (parent)
        {
            PostMessage(parent, WM_SEND_MESSAGE, 0, 0);
        }
        return 0; // Don't process the Enter key
    }

    // Call the original window procedure
    WNDPROC originalProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    return CallWindowProc(originalProc, hWnd, message, wParam, lParam);
}

ChatForm::ChatForm(HWND parent) : m_hParent(parent), m_hWnd(nullptr), m_networkManager(nullptr), m_connectDialog(nullptr), m_peerPanel(nullptr), m_hFont(nullptr)
{
    // Register the chat form window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ChatFormProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(ChatForm*);
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_SOLDATO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(0, 20, 0)); // Dark green-black background
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"ChatFormClass";
    wcex.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);

    // Create the chat form window
    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        L"ChatFormClass",
        L"Soldato",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 400,
        m_hParent,
        LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_SOLDATO)),
        GetModuleHandle(nullptr),
        this
    );

    if (m_hWnd)
    {
        InitializeControls();
        CenterWindow();

        // Initialize network manager
        m_networkManager = &NetworkManager::GetInstance();
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
        });

        // Create peer panel
        m_peerPanel = std::make_unique<PeerPanel>(m_hWnd);

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
    switch (message)
    {
    case WM_CREATE:
        return 0;


    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_SEND_MESSAGE:
        AddChatMessage(L"DEBUG", L"WM_SEND_MESSAGE RECEIVED (Enter key)");
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
            AddChatMessage(data->sender, data->message);
            UpdatePeerDisplay();
            delete data; // Clean up the heap memory
            return 0;
        }

    case WM_APP_SYSTEM_EVENT:
        {
            // This is EXECUTED on the UI Thread
            SystemEventData* data = (SystemEventData*)lParam;
            AddChatMessage(L"System", data->data);
            UpdatePeerDisplay();
            UpdateConnectionUI(); // Update UI state based on connection
            delete data; // Clean up the heap memory
            return 0;
        }

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            switch (wmId)
            {
            case ID_SEND_BUTTON:
                AddChatMessage(L"DEBUG", L"SEND BUTTON CLICKED");
                SendChatMessage();
                break;
            case IDM_ABOUT:
                OnAbout();
                break;
            case IDM_CONNECT:
                OnConnect();
                break;
            case IDM_DISCONNECT:
                OnDisconnect();
                break;
            case IDM_REFRESH:
                UpdateConnectionUI();
                break;
            case IDM_EXIT:
                PostQuitMessage(0);
                break;
            }
        }
        break;

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

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);

            // Cyberpunk background
            RECT clientRect;
            GetClientRect(m_hWnd, &clientRect);

            // Create gradient brush for cyberpunk background
            HBRUSH darkBrush = CreateSolidBrush(RGB(0, 20, 0)); // Dark green-black
            FillRect(hdc, &clientRect, darkBrush);
            DeleteObject(darkBrush);

            // Add cyberpunk grid lines
            HPEN neonPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0)); // Bright green
            HPEN oldPen = (HPEN)SelectObject(hdc, neonPen);

            // Draw grid pattern
            for (int x = 0; x < clientRect.right; x += 20)
            {
                MoveToEx(hdc, x, 0, nullptr);
                LineTo(hdc, x, clientRect.bottom);
            }
            for (int y = 0; y < clientRect.bottom; y += 20)
            {
                MoveToEx(hdc, 0, y, nullptr);
                LineTo(hdc, clientRect.right, y);
            }

            // Add corner brackets
            int bracketSize = 15;
            // Top-left
            MoveToEx(hdc, 10, 10, nullptr);
            LineTo(hdc, 10 + bracketSize, 10);
            MoveToEx(hdc, 10, 10, nullptr);
            LineTo(hdc, 10, 10 + bracketSize);

            // Top-right
            MoveToEx(hdc, clientRect.right - 10, 10, nullptr);
            LineTo(hdc, clientRect.right - 10 - bracketSize, 10);
            MoveToEx(hdc, clientRect.right - 10, 10, nullptr);
            LineTo(hdc, clientRect.right - 10, 10 + bracketSize);

            // Bottom-left
            MoveToEx(hdc, 10, clientRect.bottom - 10, nullptr);
            LineTo(hdc, 10 + bracketSize, clientRect.bottom - 10);
            MoveToEx(hdc, 10, clientRect.bottom - 10, nullptr);
            LineTo(hdc, 10, clientRect.bottom - 10 - bracketSize);

            // Bottom-right
            MoveToEx(hdc, clientRect.right - 10, clientRect.bottom - 10, nullptr);
            LineTo(hdc, clientRect.right - 10 - bracketSize, clientRect.bottom - 10);
            MoveToEx(hdc, clientRect.right - 10, clientRect.bottom - 10, nullptr);
            LineTo(hdc, clientRect.right - 10, clientRect.bottom - 10 - bracketSize);

            SelectObject(hdc, oldPen);
            DeleteObject(neonPen);

            EndPaint(m_hWnd, &ps);
        }
        break;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(m_hWnd, message, wParam, lParam);
    }

    return 0;
}

void ChatForm::InitializeControls()
{
    // Get initial window size for proper layout
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    // Calculate split layout: chat on left (70%), peer panel on right (30%)
    int chatWidth = static_cast<int>(width * 0.7);

    // Create chat history (read-only RichTextBox) with cyberpunk styling
    m_hChatHistory = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, chatWidth - 20, height - 80,
        m_hWnd,
        (HMENU)ID_CHAT_HISTORY,
        GetModuleHandle(nullptr),
        nullptr
    );

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
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, height - 60, chatWidth - 90, 25,
        m_hWnd,
        (HMENU)ID_MESSAGE_INPUT,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Set font for message input
    SendMessage(m_hMessageInput, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for message input
    SendMessage(m_hMessageInput, EM_SETBKGNDCOLOR, 0, RGB(0, 10, 0)); // Dark green background

    // Subclass the message input to handle Enter key
    SetWindowLongPtr(m_hMessageInput, GWLP_USERDATA, (LONG_PTR)GetWindowLongPtr(m_hMessageInput, GWLP_WNDPROC));
    SetWindowLongPtr(m_hMessageInput, GWLP_WNDPROC, (LONG_PTR)MessageInputProc);

    // Create send button with cyberpunk styling
    m_hSendButton = CreateWindowExW(
        0,
        L"BUTTON",
        L"[SEND]",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        chatWidth - 80, height - 60, 70, 25,
        m_hWnd,
        (HMENU)ID_SEND_BUTTON,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Set font for send button
    SendMessage(m_hSendButton, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for send button
    SendMessage(m_hSendButton, BM_SETCHECK, BST_UNCHECKED, 0);


    AddChatMessage(L"SYSTEM", L"READY ...");

    // Set focus to message input
    SetFocus(m_hMessageInput);
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

    SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

bool ChatForm::Show() const
{
    if (m_hWnd)
    {
        ShowWindow(m_hWnd, SW_SHOW);
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hMessageInput);
        return true;
    }
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
    std::wstring formattedMessage = L"[" + sender + L"]: " + message + L"\r\n";

    // Get current text length
    int textLength = GetWindowTextLength(m_hChatHistory);

    // Set selection to end
    SendMessage(m_hChatHistory, EM_SETSEL, textLength, textLength);

    // Insert new message
    SendMessage(m_hChatHistory, EM_REPLACESEL, FALSE, (LPARAM)formattedMessage.c_str());

    // Scroll to bottom
    SendMessage(m_hChatHistory, EM_SCROLL, SB_BOTTOM, 0);

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
        AddChatMessage(L"DEBUG", L"SendChatMessage called - checking connection state");

        // Send via network manager (async to avoid blocking UI)
        if (m_networkManager && m_networkManager->IsConnected())
        {
            AddChatMessage(L"DEBUG", L"Connection confirmed - sending message");
            AddChatMessage(L"DEBUG", L"Calling SendMessageAsync...");
            m_networkManager->SendMessageAsync(message);
            AddChatMessage(L"DEBUG", L"SendMessageAsync completed");

            // Add to chat history immediately (optimistic UI update)
            AddChatMessage(L"You", buffer);
            SetWindowTextW(m_hMessageInput, L"");
            SetFocus(m_hMessageInput);

            // Ensure send button remains the default button
            SendMessage(m_hSendButton, BM_SETSTYLE, BS_PUSHBUTTON | BS_DEFPUSHBUTTON, TRUE);
            AddChatMessage(L"DEBUG", L"Send button set as default after message sent");
        }
        else
        {
            AddChatMessage(L"DEBUG", L"Not connected - showing not connected message");
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
    DialogBox(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_ABOUTBOX), m_hWnd, About);
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
    AddChatMessage(L"DEBUG", L"OnDisconnect called - checking connection state");

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
        AddChatMessage(L"DEBUG", L"UpdateConnectionUI called - connected: true");

        // Restore socket callback after reconnecting
        if (connected) {
            m_networkManager->SetSocketEventCallback([this](int eventType, const std::string& data) {
                this->OnSocketEvent(eventType, data);
            });
            AddChatMessage(L"DEBUG", L"Socket callback restored after connection");
        }

        EnableDisconnectControls(connected);

        // Ensure proper button focus when connection state changes
        if (connected) {
            SendMessage(m_hSendButton, BM_SETSTYLE, BS_PUSHBUTTON | BS_DEFPUSHBUTTON, TRUE);
            AddChatMessage(L"DEBUG", L"Send button set as default after connection");
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
