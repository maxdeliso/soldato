#include "framework.h"
#include "ChatForm.h"
#include "Resource.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

// Forward declaration
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Global chat form instance (declared in WindowsProject.cpp)
extern ChatForm* g_pChatForm;

// Control IDs
#define ID_CHAT_HISTORY    1001
#define ID_MESSAGE_INPUT   1002
#define ID_SEND_BUTTON     1003

ChatForm::ChatForm(HWND parent) : m_hParent(parent), m_hWnd(nullptr), m_networkManager(nullptr), m_connectDialog(nullptr)
{
    // Register the chat form window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ChatFormProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(ChatForm*);
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(0, 20, 0)); // Dark green-black background
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"ChatFormClass";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExW(&wcex);

    // Create the chat form window
    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        L"ChatFormClass",
        L"CYBERPUNK CHAT v2.0.77 - NEURAL INTERFACE",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 400,
        m_hParent,
        LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_WINDOWSPROJECT)),
        GetModuleHandle(nullptr),
        this
    );

    if (m_hWnd)
    {
        InitializeControls();
        CenterWindow();

        // Initialize network manager
        m_networkManager = NetworkManager::GetInstance();
        m_networkManager->SetMessageCallback(OnNetworkMessage);
        m_networkManager->SetSocketEventCallback(OnSocketEvent);

        // Create connect dialog
        m_connectDialog = new ConnectDialog(m_hWnd);
    }
}

ChatForm::~ChatForm()
{
    if (m_connectDialog)
    {
        delete m_connectDialog;
    }

    if (m_networkManager)
    {
        NetworkManager::DestroyInstance();
    }

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
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

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case ID_SEND_BUTTON:
                SendChatMessage();
                break;
            case IDM_ABOUT:
                OnAbout();
                break;
            case IDM_CONNECT:
                OnConnect();
                break;
            case IDM_EXIT:
                PostQuitMessage(0);
                break;
            }
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        else if (wParam == VK_RETURN && GetFocus() == m_hMessageInput)
        {
            SendChatMessage();
            return 0;
        }
        break;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Resize chat history
            SetWindowPos(m_hChatHistory, nullptr, 10, 10, width - 20, height - 80, SWP_NOZORDER);

            // Resize message input
            SetWindowPos(m_hMessageInput, nullptr, 10, height - 60, width - 90, 25, SWP_NOZORDER);

            // Move send button
            SetWindowPos(m_hSendButton, nullptr, width - 80, height - 60, 70, 25, SWP_NOZORDER);
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
    // Create chat history (read-only RichTextBox) with cyberpunk styling
    m_hChatHistory = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, 460, 280,
        m_hWnd,
        (HMENU)ID_CHAT_HISTORY,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Set cyberpunk monospace font for chat history
    HFONT hFont = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas"
    );
    SendMessage(m_hChatHistory, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set cyberpunk colors for chat history
    SetWindowLongPtr(m_hChatHistory, GWL_EXSTYLE, GetWindowLongPtr(m_hChatHistory, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);
    SendMessage(m_hChatHistory, EM_SETBKGNDCOLOR, 0, RGB(0, 10, 0)); // Dark green background

    // Create message input field with cyberpunk styling
    m_hMessageInput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 300, 380, 25,
        m_hWnd,
        (HMENU)ID_MESSAGE_INPUT,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Set font for message input
    SendMessage(m_hMessageInput, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set cyberpunk colors for message input
    SendMessage(m_hMessageInput, EM_SETBKGNDCOLOR, 0, RGB(0, 10, 0)); // Dark green background

    // Create send button with cyberpunk styling
    m_hSendButton = CreateWindowExW(
        0,
        L"BUTTON",
        L"[SEND]",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        400, 300, 70, 25,
        m_hWnd,
        (HMENU)ID_SEND_BUTTON,
        GetModuleHandle(nullptr),
        nullptr
    );

    // Set font for send button
    SendMessage(m_hSendButton, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set cyberpunk colors for send button
    SendMessage(m_hSendButton, BM_SETCHECK, BST_UNCHECKED, 0);

    // Add cyberpunk welcome message
    AddChatMessage(L"SYSTEM", L"NEURAL LINK ESTABLISHED");
    AddChatMessage(L"SYSTEM", L"CYBERPUNK CHAT v2.0.77 ONLINE");
    AddChatMessage(L"SYSTEM", L"READY FOR NEURAL INTERFACE...");

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
        // Convert to narrow string
        std::wstring wMessage(buffer);
        std::string message(wMessage.begin(), wMessage.end());

        // Send via network manager
        if (m_networkManager && m_networkManager->IsConnected())
        {
            bool success = m_networkManager->SendMessage(message);
            if (success)
            {
                // Add to chat history
                AddChatMessage(L"You", buffer);
                SetWindowTextW(m_hMessageInput, L"");
                SetFocus(m_hMessageInput);
            }
            else
            {
                AddChatMessage(L"System", L"Failed to send message");
            }
        }
        else
        {
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

void ChatForm::OnNetworkMessage(const std::string& sender, const std::string& message)
{
    // Convert to wide strings
    std::wstring wSender(sender.begin(), sender.end());
    std::wstring wMessage(message.begin(), message.end());

    // Add to chat history
    // Note: This is called from a network thread, so we need to post a message to the UI thread
    // For now, we'll add it directly (in a real app, you'd use PostMessage)
    // This is a simplified implementation

    // Get the global chat form instance and add the message
    // This is a workaround since the callback is static
    if (g_pChatForm)
    {
        g_pChatForm->AddChatMessage(wSender, wMessage);
    }
}

void ChatForm::OnSocketEvent(int eventType, const std::string& data)
{
    // Convert to wide string
    std::wstring wData(data.begin(), data.end());

    // Add system message to chat
    std::wstring systemMessage = L"[System]: " + wData;

    // Note: This is called from a network thread, so we need to post a message to the UI thread
    // For now, we'll add it directly (in a real app, you'd use PostMessage)
    // This is a simplified implementation

    // Get the global chat form instance and add the system message
    if (g_pChatForm)
    {
        g_pChatForm->AddChatMessage(L"System", wData);
    }
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
