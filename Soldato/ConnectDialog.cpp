#include "framework.h"
#include "ConnectDialog.h"
#include "Resource.h"
#include "NetworkManager.h"
#include "StringUtils.h"
#include <sstream>

// Control IDs
#define ID_MULTICAST_IP    2001
#define ID_PORT            2002
#define ID_USERNAME        2003
#define ID_CONNECT_BTN     2004
#define ID_CANCEL_BTN      2005

ConnectDialog::ConnectDialog(HWND parent, ConnectSuccessCallback on_success)
    : m_hParent(parent), m_hWnd(nullptr), m_on_success(on_success)
{
    // Create the dialog window
    m_hWnd = CreateDialogParam(
        GetModuleHandle(nullptr),
        MAKEINTRESOURCE(IDD_CONNECT_DIALOG),
        m_hParent,
        DialogProc,
        (LPARAM)this
    );
}

ConnectDialog::~ConnectDialog()
{
    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
    }
}

LRESULT CALLBACK ConnectDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ConnectDialog* pThis = nullptr;

    if (message == WM_INITDIALOG)
    {
        pThis = (ConnectDialog*)lParam;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hWnd = hWnd;
        pThis->InitializeControls();
        return TRUE;
    }
    else
    {
        pThis = (ConnectDialog*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis)
    {
        return pThis->HandleMessage(message, wParam, lParam);
    }

    return FALSE;
}

LRESULT ConnectDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case ID_CONNECT_BTN:
                OnConnect();
                break;
            case ID_CANCEL_BTN:
            case IDCANCEL:
                Hide();
                break;
            }
        }
        break;

    case WM_CLOSE:
        Hide();
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

void ConnectDialog::InitializeControls()
{
    // Get control handles
    m_hMulticastIP = GetDlgItem(m_hWnd, ID_MULTICAST_IP);
    m_hPort = GetDlgItem(m_hWnd, ID_PORT);
    m_hUsername = GetDlgItem(m_hWnd, ID_USERNAME);
    m_hConnectButton = GetDlgItem(m_hWnd, ID_CONNECT_BTN);
    m_hCancelButton = GetDlgItem(m_hWnd, ID_CANCEL_BTN);

    // Set default values
    SetWindowTextW(m_hMulticastIP, L"224.0.0.122");
    SetWindowTextW(m_hPort, L"1337");
    SetWindowTextW(m_hUsername, L"User");

    // Set focus to username field
    SetFocus(m_hUsername);
}

void ConnectDialog::OnConnect()
{
    // Get values from controls
    wchar_t multicastIP[256];
    wchar_t port[32];
    wchar_t username[256];

    GetWindowTextW(m_hMulticastIP, multicastIP, 256);
    GetWindowTextW(m_hPort, port, 32);
    GetWindowTextW(m_hUsername, username, 256);

    // Validate inputs
    if (wcslen(multicastIP) == 0 || wcslen(port) == 0 || wcslen(username) == 0)
    {
        MessageBoxW(m_hWnd, L"Please fill in all fields.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Convert to strings
    std::wstring wMulticastIP(multicastIP);
    std::wstring wPort(port);
    std::wstring wUsername(username);

    // Convert to narrow strings using proper UTF-8 conversion
    std::string multicastIPStr = StringUtils::to_string(wMulticastIP);
    std::string usernameStr = StringUtils::to_string(wUsername);

    // Convert port to integer
    int portNum = _wtoi(port);
    if (portNum <= 0 || portNum > 65535)
    {
        MessageBoxW(m_hWnd, L"Port must be between 1 and 65535.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Call the network manager to connect
    NetworkManager& networkManager = NetworkManager::GetInstance();
    {
        bool success = networkManager.Connect(multicastIPStr, portNum, usernameStr);
        if (success)
        {
            Hide();
            // Call the success callback if provided
            if (m_on_success)
            {
                m_on_success();
            }
        }
        else
        {
            MessageBoxW(m_hWnd, L"Failed to connect to network. Please check your settings.", L"Connection Error", MB_OK | MB_ICONERROR);
        }
    }
}

bool ConnectDialog::Show() const
{
    if (m_hWnd)
    {
        CenterWindow();
        ShowWindow(m_hWnd, SW_SHOW);
        SetForegroundWindow(m_hWnd);
        return true;
    }
    return false;
}

void ConnectDialog::CenterWindow() const
{
    if (!m_hWnd || !m_hParent)
        return;

    RECT dialogRect, parentRect;
    GetWindowRect(m_hWnd, &dialogRect);
    GetWindowRect(m_hParent, &parentRect);

    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    int parentWidth = parentRect.right - parentRect.left;
    int parentHeight = parentRect.bottom - parentRect.top;

    int x = parentRect.left + (parentWidth - dialogWidth) / 2;
    int y = parentRect.top + (parentHeight - dialogHeight) / 2;

    // Ensure the dialog stays on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + dialogWidth > screenWidth) x = screenWidth - dialogWidth;
    if (y + dialogHeight > screenHeight) y = screenHeight - dialogHeight;

    SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void ConnectDialog::Hide() const
{
    if (m_hWnd)
    {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

std::string ConnectDialog::GetMulticastIP() const
{
    wchar_t buffer[256];
    GetWindowTextW(m_hMulticastIP, buffer, 256);
    std::wstring wstr(buffer);
    return StringUtils::to_string(wstr);
}

int ConnectDialog::GetPort() const
{
    wchar_t buffer[32];
    GetWindowTextW(m_hPort, buffer, 32);
    return _wtoi(buffer);
}

std::string ConnectDialog::GetUsername() const
{
    wchar_t buffer[256];
    GetWindowTextW(m_hUsername, buffer, 256);
    std::wstring wstr(buffer);
    return StringUtils::to_string(wstr);
}
