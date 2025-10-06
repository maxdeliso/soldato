// Soldato.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Soldato.h"
#include "WinsockManager.h"
#include <mutex>
#include <memory>

constexpr auto MAX_LOADSTRING = 100;

ATOM MyRegisterClass(HINSTANCE, LPCWSTR);
BOOL InitInstance(HINSTANCE, int, LPCWSTR, LPCWSTR);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Chat form instance (no longer global - using smart pointer)
std::unique_ptr<ChatForm> g_pChatForm;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize Winsock using RAII wrapper
    WinsockManager winsockManager;
    if (!winsockManager.IsInitialized()) {
        MessageBoxW(nullptr, L"Failed to initialize Winsock", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    HINSTANCE hInst = nullptr;
    WCHAR szTitle[MAX_LOADSTRING];
    WCHAR szWindowClass[MAX_LOADSTRING];

    LoadStringW(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInst, IDC_SOLDATO, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInst, szWindowClass);

    // Create the chat form directly (no main window needed)
    g_pChatForm = std::make_unique<ChatForm>(nullptr, hInstance);

    // Show the chat form immediately
    if (g_pChatForm)
    {
        g_pChatForm->Show();
    }

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Smart pointer handles cleanup automatically
    g_pChatForm.reset();

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance, LPCWSTR szWindowClass)
{
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SOLDATO));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SOLDATO);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, LPCWSTR szWindowClass, LPCWSTR szTitle)
{
   HWND hWnd = CreateWindowW(
     szWindowClass,
     szTitle,
     WS_OVERLAPPEDWINDOW,
     CW_USEDEFAULT,
     0,
     CW_USEDEFAULT,
     0,
     nullptr,
     nullptr,
     hInstance,
     nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(
  HWND hWnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(NULL, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_OPEN_CHAT:
                if (g_pChatForm)
                {
                    g_pChatForm->Show();
                }
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
