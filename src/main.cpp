// WinMain: frameless top-level window with a macOS-style chrome.
//
// The standard frame is removed via the WM_NCCALCSIZE trick (client area =
// window rect, resize borders re-implemented through WM_NCHITTEST), the
// traffic-light buttons live inside the client area, and DWM provides
// rounded corners, the shadow and the optional Mica/Acrylic backdrop.

#include "app/App.h"
#include "core/Settings.h"

#include <windowsx.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <cwchar>

namespace micp {
namespace {
Gdiplus::GdiplusStartupInput g_gdipInput;
ULONG_PTR g_gdipToken = 0;

void TrackMouse(HWND hwnd)
{
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(tme);
    tme.hwndTrack = hwnd;
    tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
    TrackMouseEvent(&tme);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCCALCSIZE:
        if (wParam)
        {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            if (IsZoomed(hwnd))
            {
                // Keep a resize-border margin so maximized content is not
                // clipped off-screen.
                const int bx = GetSystemMetrics(SM_CXSIZEFRAME)
                               + GetSystemMetrics(SM_CXPADDEDBORDER);
                const int by = GetSystemMetrics(SM_CYSIZEFRAME)
                               + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += bx;
                params->rgrc[0].top += by;
                params->rgrc[0].right -= bx;
                params->rgrc[0].bottom -= by;
            }
            return 0; // client = window rect (no caption, no borders)
        }
        break;

    case WM_NCHITTEST:
    {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        const int zone = g_app.HitTestChrome(pt);
        if (zone != 0)
            return zone;
        return HTCLIENT;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(LoadCursorW(nullptr, g_app.WantsHandCursor() ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        g_app.OnPaint(ps.hdc, ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        g_app.OnSize();
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = S(920);
        mmi->ptMinTrackSize.y = S(560); // shorter windows are fine: pages scroll
        return 0;
    }

    case WM_DPICHANGED:
        g_app.OnDpiChanged(HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
        return 0;

    case WM_SETTINGCHANGE:
        if (lParam && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0)
            g_app.OnSettingChangeTheme();
        return 0;

    case WM_ACTIVATE:
        g_app.OnActivate(LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_MOUSEMOVE:
        TrackMouse(hwnd);
        g_app.OnMouseMove(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, false);
        return 0;

    case WM_NCMOUSEMOVE:
        TrackMouse(hwnd);
        {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            g_app.OnMouseMove(pt, true);
        }
        return 0;

    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
        g_app.OnMouseLeave();
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        g_app.OnLButtonDown(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;

    case WM_LBUTTONUP:
        g_app.OnLButtonUp(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;

    case WM_MOUSEWHEEL:
    {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &pt);
        g_app.OnMouseWheel(pt, GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    }

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == ID_EDIT_KEYWORD || id == ID_EDIT_PORT)
        {
            if (code == EN_CHANGE)
                g_app.OnEditChanged(id);
            else if (code == EN_SETFOCUS)
                g_app.OnEditFocus(id, true);
            else if (code == EN_KILLFOCUS)
                g_app.OnEditFocus(id, false);
        }
        return 0;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        // Borderless EDIT controls: match the card background; the field
        // frame is painted by the app. Disabled edits use the muted color.
        const HWND ctrl = reinterpret_cast<HWND>(lParam);
        const HDC hdc = reinterpret_cast<HDC>(wParam);
        const Pal& pal = Palette();
        SetBkColor(hdc, RGB(pal.cardBg.GetR(), pal.cardBg.GetG(), pal.cardBg.GetB()));
        SetTextColor(hdc, IsWindowEnabled(ctrl) ? RGB(pal.text.GetR(), pal.text.GetG(), pal.text.GetB())
                                                : RGB(pal.text3.GetR(), pal.text3.GetG(), pal.text3.GetB()));
        return reinterpret_cast<LRESULT>(g_app.m_editBrush);
    }

    case WM_TIMER:
        g_app.OnTimer(int(wParam));
        return 0;

    case WM_CLOSE:
        g_app.SaveNow(); // captures window placement
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_app.Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        if (msg == InjectionSequence::kMsgProgress)
        {
            g_app.OnSeqUpdate(false);
            return 0;
        }
        if (msg == InjectionSequence::kMsgFinished)
        {
            g_app.OnSeqUpdate(true);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace
} // namespace micp

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    using namespace micp;

    // Per-monitor v2 is in the manifest; this also covers unpackaged runs.
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
    {
        using Fn = BOOL(WINAPI*)(HANDLE);
        if (auto fn = reinterpret_cast<Fn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")))
            fn(reinterpret_cast<HANDLE>(-4)); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    WSADATA wsa = {};
    WSAStartup(MAKEWORD(2, 2), &wsa);
    GdiplusStartup(&g_gdipToken, &g_gdipInput, nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"MicpMainWindow";
    RegisterClassExW(&wc);

    const SettingsData st = SettingsStore::Load();
    const int defW = 1060, defH = 700;
    const int x = st.hasPlacement ? st.winX : CW_USEDEFAULT;
    const int y = st.hasPlacement ? st.winY : CW_USEDEFAULT;
    const int w = st.hasPlacement ? st.winW : defW;
    const int h = st.hasPlacement ? st.winH : defH;

    // macOS chrome only: strip WS_SYSMENU so DWM never draws the native
    // minimize/maximize/close strip in the top-right corner (it occasionally
    // reappeared when left in). Shadow, rounded corners and Aero Snap remain
    // via WS_THICKFRAME; the traffic lights handle close/min/max.
    // WS_CLIPCHILDREN keeps the parent's repaints from painting over the
    // borderless EDIT children on the inject page.
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, kAppName,
                                (WS_OVERLAPPEDWINDOW & ~WS_SYSMENU) | WS_CLIPCHILDREN,
                                x, y, w, h, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd)
        return 1;

    g_app.Init(hwnd);

    // Guard against a saved placement that is fully off-screen (e.g. the
    // monitor was disconnected) - otherwise the window would be invisible.
    if (st.hasPlacement && !MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL))
    {
        RECT wa = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        SetWindowPos(hwnd, nullptr,
                     wa.left + ((wa.right - wa.left) - defW) / 2,
                     wa.top + ((wa.bottom - wa.top) - defH) / 2, defW, defH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    ShowWindow(hwnd, st.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(g_gdipToken);
    WSACleanup();
    CoUninitialize();
    return int(msg.wParam);
}
