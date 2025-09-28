#pragma once

#include <windows.h>
#include <string>

class ConnectDialog
{
private:
    HWND m_hWnd;
    HWND m_hParent;
    HWND m_hMulticastIP;
    HWND m_hPort;
    HWND m_hUsername;
    HWND m_hConnectButton;
    HWND m_hCancelButton;

    static LRESULT CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void OnConnect();

public:
    ConnectDialog(HWND parent);
    ~ConnectDialog();

    bool Show();
    void Hide();

    std::string GetMulticastIP() const;
    int GetPort() const;
    std::string GetUsername() const;
};

