#pragma once

#include <windows.h>
#include <string>
#include <functional>

class ConnectDialog
{
public:
    using ConnectSuccessCallback = std::function<void()>;

private:
    HWND m_hWnd;
    HWND m_hParent;
    HWND m_hMulticastIP;
    HWND m_hPort;
    HWND m_hUsername;
    HWND m_hConnectButton;
    HWND m_hCancelButton;
    ConnectSuccessCallback m_on_success;
    HINSTANCE m_hInstance;

    static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void OnConnect();
    void CenterWindow() const;

public:
    ConnectDialog(HWND parent, ConnectSuccessCallback on_success = nullptr, HINSTANCE hInstance = nullptr);
    ~ConnectDialog();

    bool Show() const;
    void Hide() const;
    HWND GetHandle() const { return m_hWnd; }

    std::string GetMulticastIP() const;
    int GetPort() const;
    std::string GetUsername() const;
};

