#include "framework.h"
#include "PeerPanel.h"
#include "Resource.h"
#include "StringUtils.h"
#include "DebugUtils.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// Ensure ListBox constants are available
#ifndef LB_SETBKCOLOR
#define LB_SETBKCOLOR 0x0194
#endif
#ifndef LB_SETTEXTCOLOR
#define LB_SETTEXTCOLOR 0x0193
#endif

// Control IDs - using enum class for type safety
enum class PeerControlId {
    PeerList = 2001,
    PeerCount = 2002
};

PeerPanel::PeerPanel(HWND parent, HINSTANCE hInstance)
    : m_hParent(parent), m_hWnd(nullptr), m_hPeerList(nullptr), m_hPeerCountLabel(nullptr), m_hFont(nullptr), m_hBkgBrush(nullptr), m_hNeonPen(nullptr), m_hInstance(hInstance) {

    DEBUG_LOG("PeerPanel: Constructor started\n");

    // Register the peer panel window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = PeerPanelProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(PeerPanel*);
    wcex.hInstance = m_hInstance;
    wcex.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SOLDATO));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    m_hBkgBrush = CreateSolidBrush(RGB(0, 15, 0)); // Dark green background
    m_hNeonPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0)); // Bright green neon pen
    wcex.hbrBackground = m_hBkgBrush;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"PeerPanelClass";
    wcex.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);
    DEBUG_LOG("PeerPanel: Window class registered\n");

    // Create the peer panel window
    m_hWnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"PeerPanelClass",
        L"Known Peers",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 300, 400,
        m_hParent,
        nullptr,
        m_hInstance,
        this
    );

    if (m_hWnd) {
        DEBUG_LOG("PeerPanel: Window created, initializing controls\n");
        InitializeControls();
        DEBUG_LOG("PeerPanel: Controls initialized\n");
    } else {
        DEBUG_LOG("PeerPanel: ERROR - Window creation failed\n");
    }

    DEBUG_LOG("PeerPanel: Constructor completed\n");
}

PeerPanel::~PeerPanel() {
    if (m_hFont) {
        DeleteObject(m_hFont); // Clean up the GDI resource
    }
    if (m_hBkgBrush) {
        DeleteObject(m_hBkgBrush); // Clean up the GDI resource
    }
    if (m_hNeonPen) {
        DeleteObject(m_hNeonPen); // Clean up the GDI resource
    }
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

LRESULT CALLBACK PeerPanel::PeerPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PeerPanel* pThis = nullptr;

    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (PeerPanel*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hWnd = hWnd;
    } else {
        pThis = (PeerPanel*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT PeerPanel::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        // WM_CREATE is fully handled, we can return 0
        return 0;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Resize peer count label
            SetWindowPos(m_hPeerCountLabel, nullptr, 10, 10, width - 20, 25, SWP_NOZORDER);

            // Resize peer list
            SetWindowPos(m_hPeerList, nullptr, 10, 40, width - 20, height - 50, SWP_NOZORDER);
        }
        // Let this fall through to DefWindowProc for proper child control management

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);

            // Cyberpunk background
            RECT clientRect;
            GetClientRect(m_hWnd, &clientRect);

            // Use pre-created background brush
            FillRect(hdc, &clientRect, m_hBkgBrush);

            // Use pre-created neon pen for border
            HPEN oldPen = (HPEN)SelectObject(hdc, m_hNeonPen);

            // Draw border
            MoveToEx(hdc, 0, 0, nullptr);
            LineTo(hdc, clientRect.right - 1, 0);
            LineTo(hdc, clientRect.right - 1, clientRect.bottom - 1);
            LineTo(hdc, 0, clientRect.bottom - 1);
            LineTo(hdc, 0, 0);

            SelectObject(hdc, oldPen);

            EndPaint(m_hWnd, &ps);
        }
        // For WM_PAINT, returning 0 is acceptable after BeginPaint/EndPaint,
        // but allowing it to fall through is also safe and simpler
        return 0;
    }

    // Any message not handled above (including WM_SIZE now)
    // MUST be passed to the default window procedure
    return DefWindowProc(m_hWnd, message, wParam, lParam);
}

void PeerPanel::InitializeControls() {
    DEBUG_LOG("PeerPanel: InitializeControls started\n");

    // Create peer count label
    m_hPeerCountLabel = CreateWindowExW(
        0,
        L"STATIC",
        L"0 peers",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 10, 280, 25,
        m_hWnd,
        (HMENU)static_cast<int>(PeerControlId::PeerCount),
        m_hInstance,
        nullptr
    );

    // Create and store the font
    m_hFont = CreateFontW(
        12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas"
    );
    SendMessage(m_hPeerCountLabel, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Create peer list (ListBox)
    m_hPeerList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY,
        10, 40, 280, 350,
        m_hWnd,
        (HMENU)static_cast<int>(PeerControlId::PeerList),
        m_hInstance,
        nullptr
    );

    // Set font for peer list
    SendMessage(m_hPeerList, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set cyberpunk colors for peer list (using symbolic constants)
    SendMessage(m_hPeerList, LB_SETBKCOLOR, 0, RGB(0, 10, 0)); // Dark green background
    SendMessage(m_hPeerList, LB_SETTEXTCOLOR, 0, RGB(0, 255, 0)); // Bright green text

    // Initial state
    DEBUG_LOG("PeerPanel: About to call UpdatePeerCount\n");
    UpdatePeerCount();
    DEBUG_LOG("PeerPanel: UpdatePeerCount completed\n");
}

bool PeerPanel::Show() const {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_SHOW);
        return true;
    }
    return false;
}

void PeerPanel::Hide() const {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

bool PeerPanel::IsVisible() const {
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void PeerPanel::updatePeers(const std::unordered_map<std::string, PeerInfo>& peers) {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_currentPeers = peers;

    // Update UI without holding the mutex to avoid deadlock
    UpdatePeerListInternal();
    UpdatePeerCountInternal();
}

void PeerPanel::UpdatePeerList() {
    if (!m_hPeerList) return;

    std::lock_guard<std::mutex> lock(m_peersMutex);
    UpdatePeerListInternal();
}

void PeerPanel::UpdatePeerCount() {
    if (!m_hPeerCountLabel) return;

    std::lock_guard<std::mutex> lock(m_peersMutex);
    UpdatePeerCountInternal();
}

void PeerPanel::UpdatePeerListInternal() {
    if (!m_hPeerList) return;

    // Clear existing entries
    SendMessage(m_hPeerList, LB_RESETCONTENT, 0, 0);
    m_peerEntries.clear();

    // Add peer entries
    for (const auto& pair : m_currentPeers) {
        std::wstring entry = FormatPeerEntry(pair.second);
        m_peerEntries.push_back(entry);
        SendMessage(m_hPeerList, LB_ADDSTRING, 0, (LPARAM)entry.c_str());
    }
}

void PeerPanel::UpdatePeerCountInternal() {
    if (!m_hPeerCountLabel) return;

    int count = static_cast<int>(m_currentPeers.size());
    std::wstring countText = (count == 1) ? L"1 peer" : std::to_wstring(count) + L" peers";

    SetWindowTextW(m_hPeerCountLabel, countText.c_str());
}

std::wstring PeerPanel::FormatPeerEntry(const PeerInfo& peer) const {
    // Truncate UUID to 8 characters
    std::string truncatedUuid = peer.uuid.length() > 8 ?
        peer.uuid.substr(0, 8) + "..." : peer.uuid;

    // Format timestamp
    std::wstring timeStr = FormatTimestamp(peer.lastSeen);

    // Create formatted entry: "uuid... ip (time)"
    std::wstring entry = StringUtils::to_wstring(truncatedUuid) +
                        L" " + StringUtils::to_wstring(peer.ipAddress) +
                        L" (" + timeStr + L")";

    return entry;
}

std::wstring PeerPanel::FormatTimestamp(const std::chrono::steady_clock::time_point& timestamp) const {
    // Convert to time_t for formatting
    auto now = std::chrono::steady_clock::now();
    auto duration = now - timestamp;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    if (seconds < 60) {
        return L"<1m";
    } else if (seconds < 3600) {
        return std::to_wstring(seconds / 60) + L"m";
    } else {
        return std::to_wstring(seconds / 3600) + L"h";
    }
}
