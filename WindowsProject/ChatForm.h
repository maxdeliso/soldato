#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "NetworkManager.h"
#include "ConnectDialog.h"

class ChatForm
{
private:
    HWND m_hWnd;
    HWND m_hChatHistory;
    HWND m_hMessageInput;
    HWND m_hSendButton;
    HWND m_hParent;

    std::vector<std::wstring> m_messages;
    NetworkManager* m_networkManager;
    ConnectDialog* m_connectDialog;

    static LRESULT CALLBACK ChatFormProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeControls();
    void AddMessage(const std::wstring& message);
    void SendChatMessage();
    void CenterWindow() const;
    void OnAbout() const;
    void OnConnect();
    static void OnNetworkMessage(const std::string& sender, const std::string& message);
    static void OnSocketEvent(int eventType, const std::string& data);

public:
    ChatForm(HWND parent);
    ~ChatForm();

    bool Show() const;
    void Hide() const;
    bool IsVisible() const;

    void AddChatMessage(const std::wstring& sender, const std::wstring& message);
    void ClearChat();
};
