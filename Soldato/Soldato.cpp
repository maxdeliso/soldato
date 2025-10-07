// Soldato.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Soldato.h"
#include "WinsockManager.h"
#include <mutex>
#include <memory>

// Removed MAX_LOADSTRING - no longer needed after removing unused window registration code

// Removed unused forward declarations - these functions are no longer needed

// Chat form instance (no longer global - using smart pointer)
std::unique_ptr<ChatForm> g_pChatForm;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Load Rich Edit library for syntax highlighting
    LoadLibrary(L"riched32.dll");

    // Create the chat form directly, passing the CORRECT hInstance
    // Note: Winsock initialization is handled by NetworkManager's static WinsockManager
    g_pChatForm = std::make_unique<ChatForm>(nullptr, hInstance);

    // Show the chat form immediately
    if (g_pChatForm)
    {
        OutputDebugStringA("Soldato: Showing chat form\n");
        g_pChatForm->Show();
        OutputDebugStringA("Soldato: Chat form shown, entering message loop\n");
    }

    MSG msg;
    OutputDebugStringA("Soldato: About to enter message loop\n");

    // --- START PATCH ---
    HWND hConnectDialog = g_pChatForm->GetConnectDialogHandle();

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        // Check if the message is for the modeless connect dialog
        if (hConnectDialog && IsDialogMessage(hConnectDialog, &msg))
        {
            // If it is, IsDialogMessage already processed it. Continue to the next message.
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // --- END PATCH ---

    OutputDebugStringA("Soldato: Message loop ended\n");

    // Smart pointer handles cleanup automatically
    g_pChatForm.reset();

    return (int) msg.wParam;
}
