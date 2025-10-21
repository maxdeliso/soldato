#include "framework.h"
#include "ConnectDialog.h"
#include "Resource.h"
#include "NetworkManager.h"
#include "StringUtils.h"
#include <sstream>

enum class ConnectControlId {
    MulticastIP = 2001,
    Port = 2002,
    ConnectBtn = 2004,
    CancelBtn = 2005
};

ConnectDialog::ConnectDialog(HWND parent, ConnectSuccessCallback on_success, HINSTANCE hInstance)
    : m_hParent(parent), m_hWnd(nullptr), m_hMulticastIP(nullptr), m_hPort(nullptr),
      m_hConnectButton(nullptr), m_hCancelButton(nullptr), m_on_success(on_success), m_hInstance(hInstance)
{
    // Create the dialog window
    m_hWnd = CreateDialogParam(
        m_hInstance,
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

INT_PTR CALLBACK ConnectDialog::DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

LRESULT ConnectDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case static_cast<int>(ConnectControlId::ConnectBtn):
                OnConnect();
                break;
            case static_cast<int>(ConnectControlId::CancelBtn):
            case IDCANCEL:
                Hide();
                break;
            }
        }
        // WM_COMMAND is fully handled for dialog, return TRUE
        return TRUE;

    case WM_CLOSE:
        Hide();
        // WM_CLOSE is fully handled for dialog, return TRUE
        return TRUE;

    default:
        // For dialogs, return FALSE to let the system handle the message
        return FALSE;
    }
}

void ConnectDialog::InitializeControls()
{
    // Get control handles
    m_hMulticastIP = GetDlgItem(m_hWnd, static_cast<int>(ConnectControlId::MulticastIP));
    m_hPort = GetDlgItem(m_hWnd, static_cast<int>(ConnectControlId::Port));
    m_hConnectButton = GetDlgItem(m_hWnd, static_cast<int>(ConnectControlId::ConnectBtn));
    m_hCancelButton = GetDlgItem(m_hWnd, static_cast<int>(ConnectControlId::CancelBtn));

    // Populate the multicast IP dropdown with preset values
    SendMessageW(m_hMulticastIP, CB_ADDSTRING, 0, (LPARAM)L"FF02::77");      // IPv6 link-local multicast
    SendMessageW(m_hMulticastIP, CB_ADDSTRING, 0, (LPARAM)L"224.0.0.122");  // IPv4 multicast

    // Set default to IPv6 address (first item)
    SendMessageW(m_hMulticastIP, CB_SETCURSEL, 0, 0);

    // Set default port
    SetWindowTextW(m_hPort, L"1337");

    // Set focus to multicast IP field
    SetFocus(m_hMulticastIP);
}

void ConnectDialog::OnConnect()
{
    // Get values from controls
    wchar_t multicastIP[256];
    wchar_t port[32];

    GetWindowTextW(m_hMulticastIP, multicastIP, 256);
    GetWindowTextW(m_hPort, port, 32);

    // Validate inputs
    if (wcslen(multicastIP) == 0 || wcslen(port) == 0)
    {
        MessageBoxW(m_hWnd, L"Please fill in all fields.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Convert to strings
    std::wstring wMulticastIP(multicastIP);
    std::wstring wPort(port);

    // Convert to narrow strings using proper UTF-8 conversion
    std::string multicastIPStr = StringUtils::to_string(wMulticastIP);

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
        bool success = networkManager.Connect(multicastIPStr, portNum);
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
