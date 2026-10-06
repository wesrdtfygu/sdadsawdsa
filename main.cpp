#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <chrono>
#include <cstdint>

struct State {
    bool running = false;
    std::uint64_t events = 0;
    bool left = true;
    std::chrono::steady_clock::time_point last{};
};

static State g;

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_KEYDOWN:
        if (w == VK_F8 && !(l & 0x40000000)) {
            g.running = !g.running;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }
        if (w == VK_ESCAPE) { DestroyWindow(h); return 0; }
        break;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (x >= 35 && x <= 230 && y >= 115 && y <= 160) {
            g.running = !g.running;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }
        if (x >= 35 && x <= 230 && y >= 175 && y <= 220) {
            g.events = 0;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }
        break;
    }
    case WM_TIMER:
        if (g.running) {
            ++g.events;
            g.left = !g.left;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h, &ps);
        SetBkMode(dc, TRANSPARENT);
        TextOutW(dc, 35, 30, L"StrafeHelper Remake", 19);
        TextOutW(dc, 35, 58, L"Standalone movement training utility", 36);
        const wchar_t* status = g.running ? L"Status: RUNNING" : L"Status: STOPPED";
        TextOutW(dc, 35, 88, status, lstrlenW(status));
        Rectangle(dc, 35, 115, 230, 160);
        const wchar_t* toggle = g.running ? L"Stop Training" : L"Start Training";
        TextOutW(dc, 55, 130, toggle, lstrlenW(toggle));
        Rectangle(dc, 35, 175, 230, 220);
        TextOutW(dc, 55, 190, L"Reset Statistics", 16);
        std::wostringstream os;
        os << L"Training events: " << g.events;
        auto s = os.str();
        TextOutW(dc, 285, 125, s.c_str(), (int)s.size());
        const wchar_t* dir = g.left ? L"Direction cue: LEFT" : L"Direction cue: RIGHT";
        TextOutW(dc, 285, 160, dir, lstrlenW(dir));
        TextOutW(dc, 35, 270, L"F8 toggles training. ESC exits.", 30);
        TextOutW(dc, 35, 300, L"No game input injection or anti-cheat bypass.", 44);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    const wchar_t cls[] = L"StrafeHelperRemake";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND h = CreateWindowExW(0, cls, L"StrafeHelper Remake", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 420, nullptr, nullptr, hi, nullptr);
    if (!h) return 2;
    ShowWindow(h, show);
    SetTimer(h, 1, 25, nullptr);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
