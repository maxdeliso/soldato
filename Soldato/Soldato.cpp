#include "framework.h"
#include "Soldato.h"
#include "WinsockManager.h"
#include "DebugUtils.h"
#include <mutex>
#include <memory>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPWSTR    lpCmdLine,
  _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);

  // Note: Winsock initialization is handled by NetworkManager's static WinsockManager
  std::unique_ptr<ChatForm> g_pChatForm = std::make_unique<ChatForm>(nullptr, hInstance);

  if (g_pChatForm)
  {
    DEBUG_LOG("Soldato: Showing chat form");
    g_pChatForm->Show();
    DEBUG_LOG("Soldato: Chat form shown, entering message loop");
  }

  MSG msg;
  DEBUG_LOG("Soldato: About to enter message loop");
  HWND hConnectDialog = g_pChatForm->GetConnectDialogHandle();

  while (GetMessage(&msg, nullptr, 0, 0))
  {
    if (hConnectDialog && IsDialogMessage(hConnectDialog, &msg))
    {
      continue;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DEBUG_LOG("Soldato: Message loop ended");
  g_pChatForm.reset();

  return (int)msg.wParam;
}
