#include "app/App.h"

#include "core/AppPaths.h"
#include "core/Injector.h"
#include "core/PortProbe.h"
#include "core/ProcessFinder.h"

#include <commdlg.h>
#include <cstdlib>
#include <cmath>
#include <uxtheme.h> // MARGINS (used by DwmExtendFrameIntoClientArea)
#include <shellapi.h>
#include <windowsx.h>

namespace micp {

App g_app;

static const PageInfo kPages[PgCount] = {
    {L"\u603b\u89c8", IconKind::Gauge},  // 总览
    {L"\u6ce8\u5165", IconKind::Zap},    // 注入
    {L"\u65e5\u5fd7", IconKind::Doc},    // 日志
    {L"\u8bbe\u7f6e", IconKind::Gear},   // 设置
    {L"\u5173\u4e8e", IconKind::Info},   // 关于
};

const PageInfo& PageInfoOf(PageId page) { return kPages[page]; }

// ------------------------------------------------------------- DWM helpers

namespace {

using FnDwmSetWindowAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
using FnDwmExtendFrame = HRESULT(WINAPI*)(HWND, const MARGINS*);

FnDwmSetWindowAttribute DwmSetAttr()
{
    static FnDwmSetWindowAttribute fn = [] {
        HMODULE m = LoadLibraryW(L"dwmapi.dll");
        return m ? reinterpret_cast<FnDwmSetWindowAttribute>(
                       GetProcAddress(m, "DwmSetWindowAttribute"))
                 : nullptr;
    }();
    return fn;
}

FnDwmExtendFrame DwmExtend()
{
    static FnDwmExtendFrame fn = [] {
        HMODULE m = LoadLibraryW(L"dwmapi.dll");
        return m ? reinterpret_cast<FnDwmExtendFrame>(GetProcAddress(m, "DwmExtendFrameIntoClientArea"))
                 : nullptr;
    }();
    return fn;
}

constexpr DWORD kDwmaDarkMode = 20;          // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kDwmaCornerPref = 33;        // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kDwcpRound = 2;              // DWMWCP_ROUND
constexpr DWORD kDwmaBackdrop = 38;          // DWMWA_SYSTEMBACKDROP_TYPE
constexpr DWORD kDwmsbtNone = 1;
constexpr DWORD kDwmsbtMica = 2;             // DWMSBT_MAINWINDOW
constexpr DWORD kDwmsbtAcrylic = 3;          // DWMSBT_TRANSIENTWINDOW

// Undocumented Windows 10 acrylic (kept for pre-22H2 fallback).
struct AccentPolicy
{
    int accent, flags, color, animation;
};
struct WinCompAttrData
{
    int attribute;
    void* data;
    unsigned long size;
};
using FnSetWindowCompositionAttribute = BOOL(WINAPI*)(HWND, WinCompAttrData*);

bool SetAcrylicWin10(HWND hwnd, COLORREF tint)
{
    static FnSetWindowCompositionAttribute fn = [] {
        HMODULE m = GetModuleHandleW(L"user32.dll");
        return m ? reinterpret_cast<FnSetWindowCompositionAttribute>(
                       GetProcAddress(m, "SetWindowCompositionAttribute"))
                 : nullptr;
    }();
    if (!fn)
        return false;
    AccentPolicy policy = {};
    policy.accent = 4; // ACCENT_ENABLE_ACRYLICBLURBEHIND
    policy.flags = 2;
    policy.color = int(tint);
    WinCompAttrData data = {};
    data.attribute = 19; // WCA_ACCENT_POLICY
    data.data = &policy;
    data.size = sizeof(policy);
    return fn(hwnd, &data) != FALSE;
}

DWORD BuildNumber()
{
    HKEY key = nullptr;
    DWORD build = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        wchar_t text[32] = {};
        DWORD size = sizeof(text);
        if (RegQueryValueExW(key, L"CurrentBuildNumber", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(text), &size) == ERROR_SUCCESS)
            build = DWORD(_wtoi(text));
        RegCloseKey(key);
    }
    return build;
}

int QueryDpi(HWND hwnd)
{
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
    {
        using Fn = UINT(WINAPI*)(HWND);
        if (auto fn = reinterpret_cast<Fn>(GetProcAddress(user32, "GetDpiForWindow")))
            return int(fn(hwnd));
    }
    HDC dc = GetDC(hwnd);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(hwnd, dc);
    return dpi ? dpi : 96;
}

// --- EDIT subclass ---------------------------------------------------------
// WM_MOUSEWHEEL is delivered to the focused window; a focused single-line EDIT
// would swallow it. Forward the wheel to the main window so the page scrolls
// even while a field has focus.
WNDPROC g_editOrigProc = nullptr;

LRESULT CALLBACK EditHookProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_MOUSEWHEEL)
    {
        SendMessageW(GetParent(h), msg, wParam, lParam);
        return 0;
    }
    return CallWindowProcW(g_editOrigProc, h, msg, wParam, lParam);
}

} // namespace

// ------------------------------------------------------------------- init

void App::Init(HWND hwnd)
{
    m_hwnd = hwnd;
    m_set = SettingsStore::Load();

    m_dpi = QueryDpi(hwnd);
    SetDpi(m_dpi);
    RefreshSystemDark();
    SetThemeMode(m_set.theme);
    RebuildFonts(m_dpi);

    // EDIT children (visible only on the inject page).
    CreateDirectoryW(AppPaths::SettingsDir().c_str(), nullptr);
    m_editKw = CreateWindowExW(0, L"EDIT", m_set.keyword.c_str(),
                               WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_KEYWORD)),
                               GetModuleHandleW(nullptr), nullptr);
    m_editPort = CreateWindowExW(0, L"EDIT", std::to_wstring(m_set.port).c_str(),
                                 WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER, 0, 0, 10, 10, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_PORT)),
                                 GetModuleHandleW(nullptr), nullptr);
    SendMessageW(m_editKw, WM_SETFONT, reinterpret_cast<WPARAM>(WFont(FBody, true)), TRUE);
    SendMessageW(m_editPort, WM_SETFONT, reinterpret_cast<WPARAM>(WFont(FBody, true)), TRUE);
    // macOS-like inner padding for the borderless fields.
    const LPARAM margins = MAKELPARAM(S(8), S(8));
    SendMessageW(m_editKw, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, margins);
    SendMessageW(m_editPort, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, margins);
    // Both edits share the EDIT class proc; hook them once for wheel forwarding.
    if (!g_editOrigProc)
    {
        g_editOrigProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(m_editKw, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditHookProc)));
        SetWindowLongPtrW(m_editPort, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&EditHookProc));
    }

    m_dllFound = Injector::FileReadable(ResolvedDllPath());
    m_mon.Configure(m_set.keyword, uint16_t(m_set.port));

    SetTimer(hwnd, 1, 15, nullptr);    // fast: animation clock / spinner
    SetTimer(hwnd, 2, 500, nullptr);   // log tail
    SetTimer(hwnd, 3, 1000, nullptr);  // proxy monitor
    SetTimer(hwnd, 4, 900, nullptr);   // settings save debounce

    // The first paint renders straight away; only page switches animate.
    m_pageAnim.v = 1.0f;
    m_pageAnim.target = 1.0f;

    ApplyThemeInternal();
    ApplyBackdrop();
    SyncEditsFromSettings();
    m_now = GetTickCount64();
}

void App::Shutdown()
{
    m_seq.Shutdown();
    SaveNow();
    KillTimer(m_hwnd, 1);
    KillTimer(m_hwnd, 2);
    KillTimer(m_hwnd, 3);
    KillTimer(m_hwnd, 4);
}

void App::ApplyThemeInternal()
{
    RefreshSystemDark();
    SetThemeMode(m_set.theme);

    if (m_editBrush)
        DeleteObject(m_editBrush);
    // The EDIT children sit on cards and get their frame painted by the app,
    // so their background matches the card surface.
    const Gdiplus::Color& field = Palette().cardBg;
    m_editBrush = CreateSolidBrush(RGB(field.GetR(), field.GetG(), field.GetB()));

    if (auto fn = DwmSetAttr())
    {
        const BOOL dark = IsDark();
        fn(m_hwnd, kDwmaDarkMode, &dark, sizeof(dark));
    }
    m_layoutDirty = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::ApplyBackdrop()
{
    const DWORD build = BuildNumber();

    if (auto fn = DwmSetAttr())
    {
        const DWORD round = kDwcpRound;
        fn(m_hwnd, kDwmaCornerPref, &round, sizeof(round));
    }

    Backdrop effective = m_set.backdrop;
    if (effective != Backdrop::None)
    {
        bool supported = false;
        if (build >= 22621 && DwmSetAttr())
        {
            const DWORD value = effective == Backdrop::Mica ? kDwmsbtMica : kDwmsbtAcrylic;
            supported = SUCCEEDED(DwmSetAttr()(m_hwnd, kDwmaBackdrop, &value, sizeof(value)));
        }
        else if (effective == Backdrop::Acrylic && build >= 17134)
        {
            // Windows 10: tinted acrylic behind the window (alpha paints through).
            const Gdiplus::Color& bg = Palette().windowBg;
            const COLORREF tint = RGB(IsDark() ? 0xB8 : 0xE6, bg.GetG(), bg.GetB());
            supported = SetAcrylicWin10(m_hwnd, tint);
        }
        if (!supported)
        {
            effective = Backdrop::None;
            m_set.backdrop = Backdrop::None;
            m_dirty = true;
            ShowToast(L"\u5f53\u524d\u7cfb\u7edf\u4e0d\u652f\u6301\u8be5\u6750\u8d28\uff0c\u5df2\u81ea\u52a8\u5173\u95ed", 2); // 当前系统不支持该材质，已自动关闭
        }
        else if (DwmSetAttr() && build < 22621)
        {
            const DWORD none = kDwmsbtNone;
            DwmSetAttr()(m_hwnd, kDwmaBackdrop, &none, sizeof(none));
        }
    }
    else if (DwmSetAttr() && build >= 22621)
    {
        const DWORD none = kDwmsbtNone;
        DwmSetAttr()(m_hwnd, kDwmaBackdrop, &none, sizeof(none));
    }

    if (auto extend = DwmExtend())
    {
        MARGINS margins = {};
        if (effective != Backdrop::None)
            margins.cxLeftWidth = margins.cxRightWidth = margins.cyTopHeight =
                margins.cyBottomHeight = -1;
        extend(m_hwnd, &margins);
    }

    m_backdropActive = (effective != Backdrop::None);
    m_layoutDirty = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::SyncEditsFromSettings()
{
    SetWindowTextW(m_editKw, m_set.keyword.c_str());
    SetWindowTextW(m_editPort, std::to_wstring(m_set.port).c_str());
    m_portInvalid = false;
}

// ------------------------------------------------------------------ layout

const Layout& App::Lay()
{
    if (m_layoutDirty)
        Relayout();
    return m_lay;
}

void App::Relayout()
{
    Layout& L = m_lay;
    m_layoutDirty = false;
    L = Layout(); // reset

    RECT rc = {};
    if (!m_hwnd || !GetClientRect(m_hwnd, &rc))
    {
        ApplyEditPlacement();
        return;
    }
    const float W = float(rc.right), H = float(rc.bottom);
    const float TITLE_H = float(S(44));
    const float SB_W = float(S(216));
    const float PAD = float(S(26));

    L.root = RectF(0, 0, W, H);
    L.titlebar = RectF(0, 0, W, TITLE_H);
    L.sidebar = RectF(0, 0, SB_W, H);
    L.content = RectF(SB_W, 0, W - SB_W, H);

    for (int i = 0; i < 3; ++i)
    {
        const float cx = float(S(20) + i * S(20));
        const float d = float(S(12));
        L.traffic[i] = RectF(cx - d / 2, TITLE_H / 2 - d / 2, d, d);
    }

    float navY = TITLE_H + float(S(14));
    for (int i = 0; i < PgCount; ++i)
    {
        L.nav[i] = RectF(float(S(12)), navY, SB_W - float(S(24)), float(S(34)));
        navY += float(S(34) + S(4));
    }

    // centered content column
    const float colW = (L.content.w - 2 * PAD) < float(S(680)) ? (L.content.w - 2 * PAD)
                                                               : float(S(680));
    L.pageBox = RectF(L.content.x + (L.content.w - colW) / 2, TITLE_H + float(S(22)),
                      colW, L.content.h - TITLE_H - float(S(22) + S(14)));
    L.pageTop = L.pageBox.y;
    // NOTE: the EDIT children are placed at the END of Relayout, once the
    // page-specific rects (ijEditKw/ijEditPort) are known. Calling here would
    // position them with zeroed rects - the bug that made the fields unusable.

    // ----- page-specific layout -----------------------------------------
    float y = L.pageTop + LineH(FTitle) + float(S(18));
    const float cardPad = float(S(18));

    if (m_page == PgOverview)
    {
        if (!m_dllFound)
        {
            L.ovWarn = RectF(L.pageBox.x, y, L.pageBox.w, float(S(84)));
            y = L.ovWarn.bottom() + float(S(14));
            BtnDef b;
            b.id = ID_BTN_GOTO_INJECT;
            b.label = L"\u53bb\u9009\u62e9"; // 去选择
            b.style = 3;
            b.r = RectF(L.ovWarn.right() - cardPad - float(S(88)),
                        L.ovWarn.y + (L.ovWarn.h - float(S(28))) / 2, float(S(88)), float(S(28)));
            L.btns.push_back(b);
        }

        L.ovStatus = RectF(L.pageBox.x, y, L.pageBox.w, float(S(148)));
        y = L.ovStatus.bottom() + float(S(14));
        {
            BtnDef b;
            b.id = ID_BTN_INJECT;
            b.label = L"\u4e00\u952e\u6ce8\u5165"; // 一键注入
            b.style = 0;
            b.enabled = !m_seq.IsRunning();
            b.r = RectF(L.ovStatus.right() - cardPad - float(S(128)),
                        L.ovStatus.bottom() - cardPad - float(S(38)), float(S(128)), float(S(38)));
            L.ovBtnInject = b.r;
            L.btns.push_back(b);
        }

        const int infoRows = 4;
        L.ovInfo = RectF(L.pageBox.x, y, L.pageBox.w,
                         float(S(24) + LineH(FHead) + float(S(10))
                               + infoRows * float(S(30)) + float(S(10))));
        y = L.ovInfo.bottom() + float(S(14));
        {
            BtnDef b;
            b.id = ID_BTN_GOTO_SETTINGS;
            b.label = L"\u8c03\u6574"; // 调整
            b.style = 3;
            b.r = RectF(L.ovInfo.right() - cardPad - float(S(64)),
                        L.ovInfo.y + cardPad, float(S(64)), float(S(26)));
            L.btns.push_back(b);
        }

        L.ovHint = RectF(L.pageBox.x, y, L.pageBox.w, float(S(144)));
        L.pageContentH = L.ovHint.bottom() + float(S(20));
    }
    else if (m_page == PgInject)
    {
        const float labelW = float(S(96));
        const float fieldH = float(S(32));
        // Each row: field + a hint line underneath (wraps instead of
        // overflowing the card like the old right-side hints did).
        const float rowPitch = float(S(58));

        const float cfgH = float(S(24)) + LineH(FHead) + float(S(14)) + 3 * rowPitch
                            + float(S(6));
        L.ijConfig = RectF(L.pageBox.x, y, L.pageBox.w, cfgH);

        float rowY = L.ijConfig.y + float(S(24)) + LineH(FHead) + float(S(14));
        const float fieldW = L.ijConfig.w - cardPad * 2 - labelW - float(S(88));
        const float hintW = L.ijConfig.w - cardPad - (cardPad + labelW);
        const float hintY = fieldH + float(S(3));

        // DLL row
        L.ijRowDll = RectF(L.ijConfig.x + cardPad + labelW, rowY, fieldW, fieldH);
        L.ijHintDll = RectF(L.ijRowDll.x, rowY + hintY, hintW, float(LineH(FSmall)));
        {
            RowDef row;
            row.id = ID_ROW_DLL;
            row.r = L.ijRowDll;
            row.text = ResolvedDllPath();
            row.icon = IconKind::Doc;
            row.alert = !m_dllFound;
            L.rows.push_back(row);
            L.ijBrowse = RectF(L.ijRowDll.right() + float(S(8)), rowY, float(S(80)), fieldH);
            BtnDef b;
            b.id = ID_BTN_BROWSE_DLL;
            b.label = L"\u6d4f\u89c8\u2026"; // 浏览…
            b.style = 1;
            b.r = L.ijBrowse;
            L.btns.push_back(b);
        }
        rowY += rowPitch;

        // keyword row
        L.ijRowKw = RectF(L.ijConfig.x + cardPad + labelW, rowY, fieldW, fieldH);
        L.ijEditKw = L.ijRowKw;
        L.ijHintKw = RectF(L.ijRowKw.x, rowY + hintY, hintW, float(LineH(FSmall)));
        rowY += rowPitch;

        // port row
        L.ijRowPort = RectF(L.ijConfig.x + cardPad + labelW, rowY, float(S(120)), fieldH);
        L.ijEditPort = L.ijRowPort;
        L.ijHintPort = RectF(L.ijRowPort.x, rowY + hintY, hintW, float(LineH(FSmall)));

        // Steps card: header + 4 steps + banner + buttons (each in its own
        // slot so the banner never sits under the action buttons).
        const int stepCount = 4;
        const float stepsPad = float(S(24)) + LineH(FHead) + float(S(14));
        const float stepsH = stepsPad + stepCount * float(S(64)) + float(S(46))
                             + float(S(12)) + float(S(38)) + float(S(14));
        L.ijSteps = RectF(L.pageBox.x, L.ijConfig.bottom() + float(S(14)), L.pageBox.w, stepsH);

        float stepY = L.ijSteps.y + stepsPad;
        for (int i = 0; i < stepCount; ++i)
        {
            L.ijStepBullets[i] = RectF(L.ijSteps.x + cardPad + float(S(4)),
                                       stepY + (float(S(64)) - float(S(26))) / 2, float(S(26)),
                                       float(S(26)));
            L.ijStepTexts[i] = RectF(L.ijStepBullets[i].right() + float(S(14)), stepY,
                                     L.ijSteps.w - cardPad * 2 - float(S(48)), float(S(64)));
            stepY += float(S(64));
        }
        // WaitPort progress bar sits under the step-4 detail line (the title
        // line is now centered on the bullet, see PaintInject).
        {
            const float yc = L.ijStepBullets[3].y + L.ijStepBullets[3].w / 2;
            L.ijBar = RectF(L.ijStepTexts[3].x,
                            yc + float(LineH(FBodySem)) / 2 + float(LineH(FSmall)) + float(S(8)),
                            float(S(180)), float(S(6)));
        }

        L.ijBanner = RectF(L.ijSteps.x + cardPad, stepY, L.ijSteps.w - cardPad * 2, float(S(46)));

        const float by = L.ijBanner.bottom() + float(S(12));
        const float bh = float(S(38));
        L.ijBtnGo = RectF(L.ijSteps.x + cardPad, by, float(S(124)), bh);
        L.ijBtnRecheck = RectF(L.ijBtnGo.right() + float(S(10)), by, float(S(104)), bh);
        L.ijBtnCancel = RectF(L.ijBtnRecheck.right() + float(S(10)), by, float(S(104)), bh);
        {
            BtnDef go;
            go.id = ID_BTN_INJECT;
            go.label = m_seq.IsRunning()
                ? L"\u6ce8\u5165\u4e2d\u2026" // 注入中…
                : L"\u5f00\u59cb\u6ce8\u5165"; // 开始注入
            go.style = 0;
            go.enabled = !m_seq.IsRunning();
            go.r = L.ijBtnGo;
            L.btns.push_back(go);
            BtnDef re;
            re.id = ID_BTN_RECHECK;
            re.label = L"\u91cd\u65b0\u68c0\u6d4b"; // 重新检测
            re.style = 1;
            re.r = L.ijBtnRecheck;
            L.btns.push_back(re);
            if (m_seq.IsRunning())
            {
                BtnDef cancel;
                cancel.id = ID_BTN_CANCEL_INJ;
                cancel.label = L"\u53d6\u6d88\u6ce8\u5165"; // 取消注入
                cancel.style = 2;
                cancel.danger = true;
                cancel.r = L.ijBtnCancel;
                L.btns.push_back(cancel);
            }
        }
        L.pageContentH = L.ijSteps.bottom() + float(S(20));
    }
    else if (m_page == PgLogs)
    {
        const float toolbarH = float(S(30));
        float ty = L.pageTop;

        L.lgSeg = RectF(L.pageBox.x, ty, float(S(252)), toolbarH);
        {
            SegDef s;
            s.id = ID_SEG_LOGFILTER;
            s.r = L.lgSeg;
            s.sel = m_filter;
            s.items = {L"\u5168\u90e8", L"\u4fe1\u606f", L"\u8b66\u544a", L"\u9519\u8bef"}; // 全部/信息/警告/错误
            L.segs.push_back(s);
        }

        float rx = L.pageBox.right();
        L.lgBtnOpen = RectF(rx - float(S(96)), ty, float(S(96)), toolbarH);
        L.lgBtnClear = RectF(L.lgBtnOpen.x - float(S(10)) - float(S(76)), ty, float(S(76)), toolbarH);
        L.lgTgl = RectF(L.lgBtnClear.x - float(S(10)) - float(S(40)), ty + (toolbarH - float(S(24))) / 2,
                        float(S(40)), float(S(24)));
        L.lgTglLabel = RectF(L.lgTgl.x - float(S(66)), ty, float(S(60)), toolbarH);
        {
            BtnDef clear;
            clear.id = ID_BTN_CLEAR_LOG;
            clear.label = L"\u6e05\u7a7a"; // 清空
            clear.style = 1;
            clear.r = L.lgBtnClear;
            L.btns.push_back(clear);
            BtnDef openF;
            openF.id = ID_BTN_OPEN_LOGFILE;
            openF.label = L"\u6253\u5f00\u6587\u4ef6"; // 打开文件
            openF.style = 1;
            openF.r = L.lgBtnOpen;
            L.btns.push_back(openF);
            TglDef follow;
            follow.id = ID_TGL_FOLLOW;
            follow.r = L.lgTgl;
            follow.on = m_set.followLog;
            L.tgls.push_back(follow);
        }

        L.lgCard = RectF(L.pageBox.x, ty + toolbarH + float(S(12)), L.pageBox.w,
                         L.pageBox.h - toolbarH - float(S(12)));
        L.lgHead = RectF(L.lgCard.x, L.lgCard.y, L.lgCard.w, float(S(36)));
        L.lgBody = RectF(L.lgCard.x + float(S(14)), L.lgHead.bottom(),
                         L.lgCard.w - float(S(28)), L.lgCard.h - float(S(36)) - float(S(10)));
        L.lgScroll = RectF(L.lgCard.right() - float(S(12)), L.lgBody.y, float(S(8)),
                           L.lgBody.h);
        // the logs card always fits the viewport; it scrolls internally
        L.pageContentH = L.pageBox.h;
    }
    else if (m_page == PgSettings)
    {
        const float labelW = float(S(110));
        const float segW = float(S(252));
        const float segH = float(S(30));

        // 外观
        {
            const float h = float(S(24)) + LineH(FHead) + float(S(14)) + 2 * float(S(46))
                            + float(S(30));
            L.stAppearance = RectF(L.pageBox.x, y, L.pageBox.w, h);
            float rowY = L.stAppearance.y + float(S(24)) + LineH(FHead) + float(S(14));
            L.stSegThemeLabel = RectF(L.stAppearance.x + float(S(18)), rowY, labelW, segH);
            L.stSegTheme = RectF(L.stSegThemeLabel.right() + float(S(16)), rowY, segW, segH);
            {
                SegDef s;
                s.id = ID_SEG_THEME;
                s.r = L.stSegTheme;
                s.sel = static_cast<int>(m_set.theme);
                s.items = {L"\u8ddf\u968f\u7cfb\u7edf", L"\u6d45\u8272", L"\u6df1\u8272"}; // 跟随系统/浅色/深色
                L.segs.push_back(s);
            }
            rowY += float(S(46));
            L.stSegBackdropLabel = RectF(L.stAppearance.x + float(S(18)), rowY, labelW, segH);
            L.stSegBackdrop = RectF(L.stSegBackdropLabel.right() + float(S(16)), rowY, segW, segH);
            {
                SegDef s;
                s.id = ID_SEG_BACKDROP;
                s.r = L.stSegBackdrop;
                s.sel = m_set.backdrop == Backdrop::None ? 0
                       : m_set.backdrop == Backdrop::Mica ? 1 : 2;
                s.items = {L"\u5173\u95ed", L"\u4e91\u6bcd", L"\u4e9a\u514b\u529b"}; // 关闭/云母/亚克力
                L.segs.push_back(s);
            }
            y = L.stAppearance.bottom() + float(S(14));
        }

        // 行为
        {
            const int n = 3;
            const float h = float(S(24)) + LineH(FHead) + float(S(14)) + n * float(S(44));
            L.stBehavior = RectF(L.pageBox.x, y, L.pageBox.w, h);
            float rowY = L.stBehavior.y + float(S(24)) + LineH(FHead) + float(S(14));
            const bool on[3] = { m_set.autoSwitchOnDone, m_set.toastOnDone, m_set.followLog };
            const int ids[3] = { ID_TGL_AUTOSWITCH, ID_TGL_TOASTDONE, ID_TGL_FOLLOW };
            for (int i = 0; i < n; ++i)
            {
                L.stTgl[i] = RectF(L.stBehavior.right() - float(S(18)) - float(S(40)),
                                   rowY + (float(S(44)) - float(S(24))) / 2, float(S(40)),
                                   float(S(24)));
                L.stTglLabel[i] = RectF(L.stBehavior.x + float(S(18)), rowY,
                                        L.stBehavior.w - float(S(36)) - float(S(64)), float(S(44)));
                TglDef t;
                t.id = ids[i];
                t.r = L.stTgl[i];
                t.on = on[i];
                L.tgls.push_back(t);
                rowY += float(S(44));
            }
            y = L.stBehavior.bottom() + float(S(14));
        }

        // 日志来源
        {
            const float fieldH = float(S(32));
            const float hintH = float(S(34)); // up to two wrapped lines
            // field + wrapping hint + settings-file line, stacked without overlap
            const float h = float(S(24)) + LineH(FHead) + float(S(14)) + fieldH
                            + float(S(10)) + hintH + float(S(4)) + float(LineH(FSmall))
                            + float(S(12));
            L.stLog = RectF(L.pageBox.x, y, L.pageBox.w, h);
            const float rowY = L.stLog.y + float(S(24)) + LineH(FHead) + float(S(14));
            const float fieldW = L.stLog.w - float(S(36)) - float(S(88));
            L.stRowLog = RectF(L.stLog.x + float(S(18)), rowY, fieldW, fieldH);
            {
                RowDef row;
                row.id = ID_ROW_LOG;
                row.r = L.stRowLog;
                row.text = m_set.logPathOverride.empty() ? m_logSource : m_set.logPathOverride;
                if (TrimW(row.text).empty())
                    row.text = L"\u81ea\u52a8\u63a2\u6d4b\uff08\u5c1a\u672a\u53d1\u73b0\u65e5\u5fd7\uff09"; // 自动探测（尚未发现日志）
                row.icon = IconKind::Terminal;
                L.rows.push_back(row);
                L.stBrowseLog = RectF(L.stRowLog.right() + float(S(8)), rowY, float(S(80)), fieldH);
                BtnDef b;
                b.id = ID_BTN_BROWSE_LOG;
                b.label = L"\u6d4f\u89c8\u2026";
                b.style = 1;
                b.r = L.stBrowseLog;
                L.btns.push_back(b);
            }
            L.stIniLabel = RectF(L.stLog.x + float(S(18)), rowY + fieldH + float(S(10)),
                                 L.stLog.w - float(S(36)), hintH);
            y = L.stLog.bottom() + float(S(14));
        }

        // 重置
        {
            L.stResetCard = RectF(L.pageBox.x, y, L.pageBox.w, float(S(86)));
            L.stBtnReset = RectF(L.stResetCard.x + float(S(18)),
                                 L.stResetCard.y + (L.stResetCard.h - float(S(32))) / 2,
                                 float(S(132)), float(S(32)));
            BtnDef b;
            b.id = ID_BTN_RESET;
            b.label = L"\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e"; // 恢复默认设置
            b.style = 2;
            b.danger = true;
            b.r = L.stBtnReset;
            L.btns.push_back(b);
        }
        L.pageContentH = L.stResetCard.bottom() + float(S(20));
    }
    else if (m_page == PgAbout)
    {
        const float cxr = L.pageBox.x + L.pageBox.w / 2;
        L.abIcon = RectF(cxr - float(S(46)), L.pageTop + float(S(26)), float(S(92)), float(S(92)));
        L.abName = RectF(L.pageBox.x, L.abIcon.bottom() + float(S(18)), L.pageBox.w,
                          float(LineH(FTitle)));
        L.abVersion = RectF(cxr - float(S(120)), L.abName.bottom() + float(S(8)), float(S(240)),
                            float(S(24)));
        L.abTag = RectF(L.pageBox.x, L.abVersion.bottom() + float(S(10)), L.pageBox.w,
                        float(LineH(FBody)));
        // Two equally sized buttons, symmetric around the column center with
        // a gap - the old math placed the GitHub button across the middle.
        const float bw = float(S(132));
        const float gap = float(S(10));
        L.abBtnGh = RectF(cxr - gap - bw, L.abTag.bottom() + float(S(22)), bw, float(S(38)));
        L.abBtnDoc = RectF(cxr + gap, L.abBtnGh.y, bw, float(S(38)));
        L.abFoot = RectF(L.pageBox.x, L.abBtnGh.bottom() + float(S(20)), L.pageBox.w,
                         L.pageBox.h - (L.abBtnGh.bottom() + float(S(20))));
        {
            BtnDef gh;
            gh.id = ID_BTN_GITHUB;
            gh.label = L"GitHub \u4ed3\u5e93"; // GitHub 仓库
            gh.style = 0;
            gh.r = L.abBtnGh;
            L.btns.push_back(gh);
            BtnDef doc;
            doc.id = ID_BTN_ADMIN_RESTART;
            doc.label = L"\u4ee5\u7ba1\u7406\u5458\u8fd0\u884c"; // 以管理员运行
            doc.style = 1;
            doc.r = L.abBtnDoc;
            L.btns.push_back(doc);
        }
        // content bottom mirrors the painted footer lines (PaintAbout)
        float aboutLy = L.abFoot.y + float(S(6)) + float(S(16));
        for (int i = 0; i < 3; ++i)
            aboutLy += float(LineH(FSmall)) + float(S(6));
        L.pageContentH = aboutLy + float(S(8));
    }

    // ----- page scroll geometry ------------------------------------------
    L.pageMaxScroll = L.pageContentH > L.pageBox.h ? L.pageContentH - L.pageBox.h : 0.0f;
    if (L.pageMaxScroll <= 0)
    {
        m_pageScroll = 0;
        m_pageScrollTarget = 0;
        L.pageTrack = RectF();
        L.pageThumb = RectF();
    }
    else
    {
        ClampPageScroll();
        if (m_pageScrollTarget > L.pageMaxScroll)
            m_pageScrollTarget = L.pageMaxScroll;
        // macOS overlay scrollbar: hugs the window's right edge, floats over
        // the content, no visible track.
        L.pageTrack = RectF(L.content.right() - float(S(15)), L.pageBox.y, float(S(8)),
                            L.pageBox.h);
        float thumbH = L.pageBox.h * L.pageBox.h / L.pageContentH;
        if (thumbH < float(S(36)))
            thumbH = float(S(36));
        const float track = L.pageTrack.h - thumbH;
        L.pageThumb = RectF(L.pageTrack.x,
                            L.pageTrack.y + (m_pageScroll / L.pageMaxScroll) * track,
                            L.pageTrack.w, thumbH);
    }

    // Animated controls get their persistent values seeded once, so knobs
    // and segmented thumbs don't slide from zero on every relayout.
    for (const SegDef& s : L.segs)
    {
        auto it = m_segAnims.find(s.id);
        if (it == m_segAnims.end())
            m_segAnims[s.id] = AnimVal{ float(s.sel), float(s.sel) };
        else
            it->second.target = float(s.sel);
    }
    for (const TglDef& t : L.tgls)
    {
        auto it = m_knobAnims.find(t.id);
        if (it == m_knobAnims.end())
            m_knobAnims[t.id] = AnimVal{ t.on ? 1.0f : 0.0f, t.on ? 1.0f : 0.0f };
        else
            it->second.target = t.on ? 1.0f : 0.0f;
    }

    // The EDIT children follow the inject-page fields (with the scroll offset).
    ApplyEditPlacement();
}

void App::ApplyEditPlacement()
{
    if (!m_editKw || !m_editPort)
        return;
    const bool show = (m_page == PgInject);
    const Layout& L = m_lay;
    const DWORD flag = SWP_NOACTIVATE | SWP_NOZORDER;
    auto hide = [&](HWND edit)
    {
        // A hidden edit must not keep the keyboard focus.
        if (GetFocus() == edit)
            SetFocus(m_hwnd);
        SetWindowPos(edit, nullptr, 0, 0, 0, 0, flag | SWP_HIDEWINDOW);
    };
    auto place = [&](HWND edit, const RectF& r)
    {
        if (!show || r.w <= 0 || m_pageAnim.v < 1.0f)
        {
            // During a page transition the fields stay hidden - the HWNDs
            // would otherwise pop in at full opacity over the crossfade.
            hide(edit);
            return;
        }
        // The page scrolls as a whole: the child windows follow the offset.
        RectF v = r;
        v.y -= m_pageScroll;
        // The single-line EDIT draws its text at the top of its client rect,
        // which looks glued to the ceiling inside the 32px field frame. The
        // child is therefore only text-tall and vertically centered in the
        // frame; the painted frame around it stays full height.
        const float textH = float(LineH(FBody)) + 2.0f;
        RectF c(v.x, v.y + (v.h - textH) / 2, v.w, textH);
        // Clip the child to the scroll viewport so partially scrolled fields
        // don't paint over the toolbar above the page box.
        const float kx0 = c.x > L.pageBox.x ? c.x : L.pageBox.x;
        const float ky0 = c.y > L.pageBox.y ? c.y : L.pageBox.y;
        const float kx1 = c.right() < L.pageBox.right() ? c.right() : L.pageBox.right();
        const float ky1 = c.bottom() < L.pageBox.bottom() ? c.bottom() : L.pageBox.bottom();
        if (kx1 <= kx0 || ky1 <= ky0)
        {
            hide(edit);
            return;
        }
        HRGN rgn = CreateRectRgn(int(kx0 - c.x), int(ky0 - c.y),
                                 int(kx1 - c.x + 0.5f), int(ky1 - c.y + 0.5f));
        if (SetWindowRgn(edit, rgn, FALSE) == 0)
        {
            DeleteObject(rgn);
            hide(edit);
            return;
        }
        SetWindowPos(edit, nullptr, int(c.x), int(c.y), int(c.w), int(c.h),
                     flag | SWP_SHOWWINDOW);
        EnableWindow(edit, !m_seq.IsRunning());
    };
    place(m_editKw, L.ijEditKw);
    place(m_editPort, L.ijEditPort);
}

void App::ClampPageScroll()
{
    if (m_pageScroll < 0)
        m_pageScroll = 0;
    if (m_pageScroll > m_lay.pageMaxScroll)
        m_pageScroll = m_lay.pageMaxScroll;
}

POINT App::MapContentPoint(POINT pt) const
{
    // Layout coordinates for page controls: the content column is translated
    // by the scroll offset. The sidebar, titlebar and the scrollbar itself
    // (outside pageBox / before pageTop) stay untouched.
    if (m_page == PgLogs || m_lay.pageMaxScroll <= 0 || m_pageScroll <= 0)
        return pt;
    if (pt.x >= m_lay.content.x && pt.y >= m_lay.pageBox.y)
        return POINT{ pt.x, pt.y + int(m_pageScroll + 0.5f) };
    return pt;
}

// ------------------------------------------------------------------ events

void App::OnSize()
{
    m_layoutDirty = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnDpiChanged(int dpi, const RECT* suggested)
{
    m_dpi = dpi;
    SetDpi(dpi);
    RebuildFonts(dpi);
    SendMessageW(m_editKw, WM_SETFONT, reinterpret_cast<WPARAM>(WFont(FBody, true)), TRUE);
    SendMessageW(m_editPort, WM_SETFONT, reinterpret_cast<WPARAM>(WFont(FBody, true)), TRUE);
    const LPARAM margins = MAKELPARAM(S(8), S(8));
    SendMessageW(m_editKw, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, margins);
    SendMessageW(m_editPort, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, margins);
    m_layoutDirty = true;
    if (suggested)
        SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnSettingChangeTheme()
{
    if (m_set.theme == ThemeMode::System)
        ApplyThemeInternal();
}

void App::OnActivate(bool active)
{
    m_windowActive = active;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnMouseLeave()
{
    m_hoverId = 0;
    m_tlHoverAny = false;
    m_tlHoverIdx = -1;
    for (auto& kv : m_hoverAnims)
        kv.second.target = 0;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnMouseMove(POINT pt, bool fromNc)
{
    const int prevY = m_lastMouse.y;
    m_lastMouse = pt;
    const Layout& L = Lay();

    if (m_pageThumbDrag)
    {
        if (L.pageMaxScroll > 0 && L.pageThumb.w > 0)
        {
            const float track = L.pageTrack.h - L.pageThumb.h;
            if (track > 0)
            {
                const float rel = (float(pt.y) - L.pageTrack.y - m_pageThumbGrab) / track;
                m_pageScroll = rel * L.pageMaxScroll;
                ClampPageScroll();
                m_pageScrollTarget = m_pageScroll;
                ApplyEditPlacement();
                PokePageSb();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return;
    }
    if (m_thumbDragging)
    {
        const float rowH = float(S(24));
        int matched = 0;
        for (const auto& e : m_lines)
            if (PassesFilter(e))
                ++matched;
        const float contentH = matched * rowH;
        const float maxScroll = contentH > L.lgBody.h ? contentH - L.lgBody.h : 0;
        if (maxScroll > 0 && L.lgBody.h > 40)
        {
            const float track = L.lgBody.h - float(S(40));
            const float rel = (pt.y - L.lgBody.y - m_thumbGrab) / track;
            m_logScroll = rel * maxScroll;
            if (m_logScroll < 0)
                m_logScroll = 0;
            if (m_logScroll > maxScroll)
                m_logScroll = maxScroll;
            m_logScrollTarget = m_logScroll;
            m_logPinned = (m_logScroll >= maxScroll - 0.5f);
            PokeLogSb();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }
    if (m_logDragging)
    {
        m_logScroll -= float(pt.y - prevY);
        ClampLogScroll(L);
        m_logScrollTarget = m_logScroll;
        PokeLogSb();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // Hover tracking for page controls uses layout coordinates (scrolled);
    // the overlay scrollbars live outside the scrolled content, so they keep
    // using the raw client point.
    const POINT raw = pt;
    pt = MapContentPoint(pt);

    int hover = 0;
    bool tlAny = false;
    int tlIdx = -1;

    for (int i = 0; i < 3; ++i)
        if (L.traffic[i].contains(float(pt.x), float(pt.y)))
        {
            tlAny = true;
            tlIdx = i;
            hover = ID_TL_CLOSE + i;
        }

    if (!tlAny)
    {
        for (int i = 0; i < PgCount; ++i)
            if (L.nav[i].contains(float(pt.x), float(pt.y)))
                hover = ID_NAV + i;
    }
    // Overlay scrollbars keep their thumb revealed while hovered.
    if (!hover && m_page != PgLogs && L.pageMaxScroll > 0
        && L.pageTrack.contains(float(raw.x), float(raw.y)))
        hover = ID_PAGE_SB;
    if (!hover && m_page == PgLogs && L.lgScroll.w > 0
        && float(raw.x) >= L.lgScroll.x - 2 && float(raw.x) <= L.lgScroll.right() + 2
        && float(raw.y) >= L.lgScroll.y && float(raw.y) <= L.lgScroll.bottom())
        hover = ID_LOG_SB;
    if (!hover)
    {
        for (const RowDef& row : L.rows)
            if (row.r.contains(float(pt.x), float(pt.y)))
                hover = row.id;
    }
    if (!hover)
    {
        for (const SegDef& seg : L.segs)
        {
            const auto items = ui::SegmentedLayout(seg.r, seg.items.size());
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].contains(float(pt.x), float(pt.y)))
                    hover = seg.id * 100 + int(i) + 1;
        }
    }
    if (!hover)
    {
        for (const TglDef& t : L.tgls)
            if (t.r.contains(float(pt.x), float(pt.y)))
                hover = t.id;
    }
    if (!hover)
    {
        for (const BtnDef& b : L.btns)
            if (b.r.contains(float(pt.x), float(pt.y)))
                hover = b.id;
    }

    if (hover != m_hoverId || tlAny != m_tlHoverAny || tlIdx != m_tlHoverIdx)
    {
        // Ease the old control out and the new one in.
        SetHoverTarget(m_hoverId, false);
        SetHoverTarget(hover, true);
        m_hoverId = hover;
        m_tlHoverAny = tlAny;
        m_tlHoverIdx = tlIdx;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void App::OnLButtonDown(POINT pt)
{
    const Layout& L = Lay();
    float px = float(pt.x), py = float(pt.y);

    // traffic lights
    for (int i = 0; i < 3; ++i)
        if (L.traffic[i].contains(px, py))
        {
            m_pressId = ID_TL_CLOSE + i;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }

    // page scrollbar (overlay at the window's right edge; unscrolled coords)
    if (m_page != PgLogs && L.pageMaxScroll > 0 && L.pageTrack.contains(px, py))
    {
        m_pageThumbGrab = (py >= L.pageThumb.y && py <= L.pageThumb.bottom())
            ? py - L.pageThumb.y
            : L.pageThumb.h / 2;
        m_pageThumbDrag = true;
        SetCapture(m_hwnd);
        const float track = L.pageTrack.h - L.pageThumb.h;
        if (track > 0)
        {
            const float rel = (py - L.pageTrack.y - m_pageThumbGrab) / track;
            m_pageScroll = rel * L.pageMaxScroll;
            ClampPageScroll();
            m_pageScrollTarget = m_pageScroll;
            ApplyEditPlacement();
        }
        PokePageSb();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // From here on, hit tests use layout coordinates (page scroll applied).
    pt = MapContentPoint(pt);
    px = float(pt.x);
    py = float(pt.y);

    // log interactions (before generic controls; the log card has priority
    // inside its body area because rows/buttons never overlap it)
    if (m_page == PgLogs)
    {
        if (L.lgScroll.w > 0 && px >= L.lgScroll.x - 2 && px <= L.lgScroll.right() + 2
            && py >= L.lgScroll.y && py <= L.lgScroll.bottom())
        {
            const float rowH = float(S(24));
            int matched = 0;
            for (const auto& e : m_lines)
                if (PassesFilter(e))
                    ++matched;
            const float contentH = matched * rowH;
            const float maxScroll = contentH > L.lgBody.h ? contentH - L.lgBody.h : 0;
            if (maxScroll > 0 && L.lgBody.h > 40)
            {
                const float track = L.lgBody.h - float(S(40));
                const float thumbH = float(S(40));
                const float thumbY = L.lgBody.y + (m_logScroll / maxScroll) * track;
                if (py >= thumbY && py <= thumbY + thumbH)
                    m_thumbGrab = py - thumbY;
                else
                    m_thumbGrab = thumbH / 2;
                m_thumbDragging = true;
                SetCapture(m_hwnd);
                // jump immediately
                const float rel = (py - L.lgBody.y - m_thumbGrab) / track;
                m_logScroll = rel * maxScroll;
                if (m_logScroll < 0) m_logScroll = 0;
                if (m_logScroll > maxScroll) m_logScroll = maxScroll;
                m_logScrollTarget = m_logScroll;
                m_logPinned = (m_logScroll >= maxScroll - 0.5f);
                PokeLogSb();
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }
        if (L.lgBody.contains(px, py) || L.lgHead.contains(px, py))
        {
            m_logDragging = true;
            SetCapture(m_hwnd);
            return;
        }
    }

    // nav switches immediately (macOS behaviour)
    for (int i = 0; i < PgCount; ++i)
        if (L.nav[i].contains(px, py))
        {
            SwitchPage(static_cast<PageId>(i));
            return;
        }

    // segments toggle immediately
    for (const SegDef& seg : L.segs)
    {
        const auto items = ui::SegmentedLayout(seg.r, seg.items.size());
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].contains(px, py))
            {
                SegChanged(seg.id, int(i));
                return;
            }
    }

    // toggles flip immediately
    for (const TglDef& t : L.tgls)
        if (t.r.contains(px, py))
        {
            ToggleSetting(t.id);
            return;
        }

    // buttons & path rows act on release
    for (const BtnDef& b : L.btns)
        if (b.r.contains(px, py))
        {
            m_pressId = b.id;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    for (const RowDef& row : L.rows)
        if (row.r.contains(px, py))
        {
            m_pressId = row.id;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
}

void App::OnLButtonUp(POINT pt)
{
    const bool hadDrag = m_thumbDragging || m_logDragging || m_pageThumbDrag;
    m_thumbDragging = false;
    m_logDragging = false;
    m_pageThumbDrag = false;

    const int press = m_pressId;
    m_pressId = 0;
    if (GetCapture() == m_hwnd)
        ReleaseCapture();
    if (hadDrag)
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    pt = MapContentPoint(pt);
    const Layout& L = Lay();
    const float px = float(pt.x), py = float(pt.y);

    if (press >= ID_TL_CLOSE && press <= ID_TL_MAX)
    {
        const int idx = press - ID_TL_CLOSE;
        if (L.traffic[idx].contains(px, py))
        {
            if (press == ID_TL_CLOSE)
                SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
            else if (press == ID_TL_MIN)
                ShowWindow(m_hwnd, SW_MINIMIZE);
            else
                ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    for (const BtnDef& b : L.btns)
        if (b.id == press && b.enabled && b.r.contains(px, py))
        {
            OnCommand(press);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    for (const RowDef& row : L.rows)
        if (row.id == press && row.r.contains(px, py))
        {
            OnCommand(press);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnMouseWheel(POINT pt, int delta)
{
    const Layout& L = Lay();
    if (m_page == PgLogs)
    {
        if (!(L.lgCard.contains(float(pt.x), float(pt.y))))
            return;
        const float rowH = float(S(24));
        const float step = rowH * 3 * (delta / float(WHEEL_DELTA));
        const float maxScroll = LogMaxScroll(L);
        if (maxScroll <= 0)
            return;
        // Eased scrolling: the wheel moves the target, the animation clock
        // glides the actual offset toward it (macOS trackpad feel).
        m_logScrollTarget -= step;
        if (m_logScrollTarget < 0)
            m_logScrollTarget = 0;
        if (m_logScrollTarget > maxScroll)
            m_logScrollTarget = maxScroll;
        m_logPinned = (m_logScrollTarget >= maxScroll - 0.5f);
        PokeLogSb();
        return;
    }

    // Whole-page scrolling for every other page (content taller than the
    // viewport), including while an EDIT field holds focus (the subclass
    // forwards the wheel here).
    if (L.pageMaxScroll > 0
        && pt.x >= L.content.x && pt.y >= L.pageBox.y - float(S(8)))
    {
        const float step = float(S(38)) * 3 * (delta / float(WHEEL_DELTA));
        m_pageScrollTarget -= step;
        if (m_pageScrollTarget < 0)
            m_pageScrollTarget = 0;
        if (m_pageScrollTarget > L.pageMaxScroll)
            m_pageScrollTarget = L.pageMaxScroll;
        PokePageSb();
    }
}

int App::HitTestChrome(POINT pt)
{
    const Layout& L = Lay();
    const float px = float(pt.x), py = float(pt.y);

    if (!IsZoomed(m_hwnd))
    {
        const float b = float(S(6));
        const float c = float(S(14));
        const bool left = px < b, right = px > L.root.w - b;
        const bool top = py < b, bottom = py > L.root.h - b;
        if (left && top) return HTTOPLEFT;
        if (right && top) return HTTOPRIGHT;
        if (left && bottom) return HTBOTTOMLEFT;
        if (right && bottom) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        (void)c;
    }

    for (int i = 0; i < 3; ++i)
        if (L.traffic[i].contains(px, py))
            return HTCLIENT;

    if (py < L.titlebar.h)
        return HTCAPTION;

    return 0; // caller falls back to HTCLIENT
}

// ----------------------------------------------------------------- timers

void App::OnTimer(int id)
{
    m_now = GetTickCount64();

    if (id == 1) // fast: animation clock
    {
        static ULONGLONG s_lastTick = 0;
        float dt = s_lastTick ? float(m_now - s_lastTick) / 1000.0f : 0.016f;
        s_lastTick = m_now;
        if (dt <= 0)
            dt = 0.001f;
        if (dt > 0.1f)
            dt = 0.1f;

        bool busy = false;
        if (m_seq.IsRunning())
        {
            m_spin = (m_spin + int(dt * 300.0f)) % 360;
            busy = true;
        }
        busy |= TickAnimations(dt);
        if (busy)
            InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (id == 2) // log poll
    {
        LogTail::Result r = m_tail.Poll(m_set.logPathOverride);
        if (r.reset)
        {
            m_lines.clear();
            m_logSource = r.source;
            AppendLogLines(r.lines);
            m_logScroll = 0;
            m_logScrollTarget = 0;
            m_logPinned = true;
            if (m_page == PgLogs || m_page == PgSettings)
                InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        else if (!r.lines.empty())
        {
            AppendLogLines(r.lines);
            if (m_page == PgLogs)
                InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
    else if (id == 3) // proxy monitor
    {
        m_mon.Configure(m_set.keyword, uint16_t(m_set.port));
        m_snap = m_mon.Tick();
        const bool dllFound = Injector::FileReadable(ResolvedDllPath());
        if (dllFound != m_dllFound)
        {
            m_dllFound = dllFound;
            m_layoutDirty = true;
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (id == 4) // settings save debounce
    {
        if (m_dirty)
            SaveNow();
    }
}

// -------------------------------------------------------------- animation

float App::HoverT(int id) const
{
    if (!id)
        return 0;
    const auto it = m_hoverAnims.find(id);
    return it == m_hoverAnims.end() ? 0.0f : it->second.v;
}

float App::KnobT(int id) const
{
    const auto it = m_knobAnims.find(id);
    return it == m_knobAnims.end() ? 0.0f : it->second.v;
}

float App::SegT(int id, int sel) const
{
    const auto it = m_segAnims.find(id);
    return it == m_segAnims.end() ? float(sel) : it->second.v;
}

void App::SetHoverTarget(int id, bool on)
{
    if (!id)
        return;
    AnimVal& a = m_hoverAnims[id];
    a.target = on ? 1.0f : 0.0f;
    // A control that has never been hovered starts settled at 0; if it is
    // first seen already hovered, fade in from 0 for a soft entrance.
}

void App::PokePageSb()
{
    m_sbIdle = m_now;
    m_sbAnim.target = 1.0f;
}

void App::PokeLogSb()
{
    m_lgSbIdle = m_now;
    m_lgSbAnim.target = 1.0f;
}

bool App::TickAnimations(float dt)
{
    bool busy = false;

    auto ease = [&](AnimVal& a, float rate)
    {
        const float diff = a.target - a.v;
        if (diff > 0.001f || diff < -0.001f)
        {
            a.v += diff * (1.0f - std::exp(-dt * rate));
            if (std::abs(a.target - a.v) < 0.002f)
                a.v = a.target;
            busy = true;
        }
    };

    for (auto& kv : m_hoverAnims)
        ease(kv.second, 14.0f);
    for (auto& kv : m_knobAnims)
        ease(kv.second, 16.0f);
    for (auto& kv : m_segAnims)
        ease(kv.second, 15.0f);
    ease(m_toastAnim, 11.0f);
    ease(m_editFocusAnim[0], 14.0f);
    ease(m_editFocusAnim[1], 14.0f);
    {
        const float prev = m_pageAnim.v;
        ease(m_pageAnim, 9.0f);
        if (prev < 1.0f && m_pageAnim.v >= 1.0f)
        {
            // Transition finished: relayout so the EDIT children of the new
            // page come back (they stayed hidden during the crossfade).
            m_layoutDirty = true;
            busy = true;
        }
    }

    // Scrollbar reveal + auto-hide (macOS overlay scrollbars fade out
    // shortly after scrolling stops).
    {
        const bool active = m_pageThumbDrag || m_hoverId == ID_PAGE_SB
                            || m_now < m_sbIdle + 800;
        m_sbAnim.target = (active && m_lay.pageMaxScroll > 0) ? 1.0f : 0.0f;
        ease(m_sbAnim, 10.0f);
    }
    {
        const bool active = m_thumbDragging || m_hoverId == ID_LOG_SB
                            || m_now < m_lgSbIdle + 800;
        m_lgSbAnim.target = (active && LogMaxScroll(m_lay) > 0) ? 1.0f : 0.0f;
        ease(m_lgSbAnim, 10.0f);
    }

    // Toast phases: fade in, hold, fade out, then clear the text.
    if (!m_toastText.empty())
    {
        if (m_now >= m_toastUntil)
            m_toastAnim.target = 0;
        if (m_toastAnim.target == 0 && m_toastAnim.v <= 0.001f)
            m_toastText.clear();
    }

    // Eased wheel scrolling: the page glide uses the same offset the paint
    // pass and hit-testing see, so controls stay in sync frame by frame.
    if (m_lay.pageMaxScroll > 0)
    {
        const float diff = m_pageScrollTarget - m_pageScroll;
        if (diff > 0.5f || diff < -0.5f)
        {
            m_pageScroll += diff * (1.0f - std::exp(-dt * 16.0f));
            if (std::abs(m_pageScrollTarget - m_pageScroll) <= 0.5f)
                m_pageScroll = m_pageScrollTarget;
            ClampPageScroll();
            ApplyEditPlacement();
            busy = true;
        }
    }
    {
        const float maxScroll = LogMaxScroll(m_lay);
        if (maxScroll > 0)
        {
            if (m_logPinned && m_set.followLog)
            {
                m_logScrollTarget = maxScroll;
            }
            const float diff = m_logScrollTarget - m_logScroll;
            if (diff > 0.5f || diff < -0.5f)
            {
                m_logScroll += diff * (1.0f - std::exp(-dt * 16.0f));
                if (std::abs(m_logScrollTarget - m_logScroll) <= 0.5f)
                    m_logScroll = m_logScrollTarget;
                if (m_logScroll < 0)
                    m_logScroll = 0;
                if (m_logScroll > maxScroll)
                    m_logScroll = maxScroll;
                busy = true;
            }
        }
    }

    return busy;
}

// ------------------------------------------------------------------ edits

void App::OnEditChanged(int ctrlId)
{
    if (ctrlId == ID_EDIT_KEYWORD)
    {
        const int len = GetWindowTextLengthW(m_editKw);
        std::wstring buf(size_t(len) + 1, L'\0');
        GetWindowTextW(m_editKw, &buf[0], len + 1);
        // Commit only non-empty input: emptying the field to retype must not
        // snap it back to the default keyword under the cursor.
        const std::wstring t = TrimW(buf);
        if (!t.empty())
        {
            m_set.keyword = t;
            m_dirty = true;
        }
    }
    else if (ctrlId == ID_EDIT_PORT)
    {
        const int len = GetWindowTextLengthW(m_editPort);
        std::wstring buf(size_t(len) + 1, L'\0');
        GetWindowTextW(m_editPort, &buf[0], len + 1);
        const int port = _wtoi(buf.c_str());
        const bool valid = !TrimW(buf).empty() && port >= 1 && port <= 65535;
        if (valid)
        {
            m_set.port = port;
            m_portInvalid = false;
            m_dirty = true;
        }
        else
        {
            m_portInvalid = true;
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void App::OnEditFocus(int ctrlId, bool focused)
{
    const int idx = ctrlId == ID_EDIT_KEYWORD ? 0 : 1;
    m_editFocusAnim[idx].target = focused ? 1.0f : 0.0f;
    const int next = focused ? ctrlId : 0;
    if (m_editFocused != next)
        m_editFocused = next;
    // The focus ring fades via the animation clock; no immediate repaint.
}

// -------------------------------------------------------------- injection

std::wstring App::ResolvedDllPath() const
{
    if (!m_set.dllPath.empty() && Injector::FileReadable(m_set.dllPath))
        return m_set.dllPath;
    for (const auto& c : AppPaths::DefaultDllCandidates())
        if (Injector::FileReadable(c))
            return c;
    return m_set.dllPath.empty() ? AppPaths::DefaultDllCandidates().front() : m_set.dllPath;
}

void App::StartInject()
{
    if (m_seq.IsRunning())
        return;

    const std::wstring dll = ResolvedDllPath();
    for (int i = 0; i < 4; ++i)
    {
        m_stepState[i] = 0;
        m_stepDetail[i].clear();
    }
    m_bannerKind = 0;
    m_bannerText.clear();

    if (!Injector::FileReadable(dll))
    {
        m_stepState[0] = 3;
        m_stepDetail[0] = dll;
        m_bannerKind = 2;
        m_bannerText = L"\u672a\u627e\u5230\u4ee3\u7406 DLL\uff0c\u8bf7\u5c06 Aurora.dll \u653e\u5230\u7a0b\u5e8f\u540c\u76ee\u5f55\uff0c\u6216\u5728\u4e0a\u65b9\u9009\u62e9"; // 未找到代理 DLL，请将 Aurora.dll 放到程序同目录，或在上方选择
        SwitchPage(PgInject);
        ShowToast(L"\u672a\u627e\u5230 Aurora.dll", 3);
        m_layoutDirty = true;
        return;
    }

    m_stepState[0] = 2;
    m_stepDetail[0] = dll;

    InjectionSequence::Config cfg;
    cfg.dllPath = dll;
    cfg.keyword = m_set.keyword;
    cfg.port = uint16_t(m_set.port);
    if (m_seq.Start(m_hwnd, cfg))
    {
        m_lastSeqPhase = InjectionSequence::State::WaitWindow;
        SwitchPage(PgInject);
        ShowToast(L"\u5df2\u5f00\u59cb\u6ce8\u5165\u6d41\u7a0b", 0); // 已开始注入流程
        m_layoutDirty = true;
    }
}

void App::RecheckTargets()
{
    const ProxyTarget t = ProcessFinder::NewestMatch(m_set.keyword);
    m_dllFound = Injector::FileReadable(ResolvedDllPath());

    if (t.valid())
    {
        const bool listening = PortProbe::IsListening(t.pid, uint16_t(m_set.port));
        ShowToast(FormatStr(L"\u76ee\u6807\u7a97\u53e3\uff1a\u300a%s\u300b PID %u%s",
                            t.windowTitle.c_str(), t.pid,
                            listening ? L" \u00b7 \u4ee3\u7406\u5df2\u5728\u76d1\u542c"
                                      : L""), listening ? 1 : 0);
    }
    else
    {
        ShowToast(FormatStr(L"\u672a\u627e\u5230\u6807\u9898\u5305\u542b\u300c%s\u300d\u7684 Java \u7a97\u53e3",
                            m_set.keyword.c_str()), 2);
    }
    m_layoutDirty = true;
}

void App::OnSeqUpdate(bool finished)
{
    const InjectionSequence::Progress p = m_seq.Snapshot();

    switch (p.state)
    {
    case InjectionSequence::State::WaitWindow:
        m_stepState[1] = 1;
        m_stepDetail[1] = FormatStr(L"\u6b63\u5728\u7b49\u5f85\u6807\u9898\u542b\u300c%s\u300d\u7684\u7a97\u53e3\u2026",
                                    m_set.keyword.c_str());
        break;
    case InjectionSequence::State::Injecting:
        m_stepState[1] = 2;
        m_stepDetail[1] = FormatStr(L"\u300a%s\u300b \u00b7 PID %u", p.windowTitle.c_str(), p.pid);
        m_stepState[2] = 1;
        m_stepDetail[2] = p.message;
        break;
    case InjectionSequence::State::WaitPort:
        m_stepState[2] = 2;
        m_stepDetail[2] = L"DLL \u5df2\u52a0\u8f7d\uff0c\u7b49\u5f85\u4ee3\u7406\u542f\u52a8"; // DLL 已加载，等待代理启动
        m_stepState[3] = 1;
        m_stepDetail[3] = FormatStr(L"%.1f / %.0f s", p.waitMs / 1000.0f, p.waitTotal / 1000.0f);
        m_stepProgress = p.waitTotal > 0 ? float(p.waitMs) / float(p.waitTotal) : 0.0f;
        break;
    case InjectionSequence::State::Done:
        m_stepState[1] = 2;
        m_stepState[2] = 2;
        m_stepState[3] = 2;
        m_bannerKind = 1;
        m_bannerText = p.message;
        break;
    case InjectionSequence::State::Failed:
        if (m_lastSeqPhase == InjectionSequence::State::Injecting)
            m_stepState[2] = 3;
        else
            m_stepState[3] = 3;
        m_bannerKind = 2;
        m_bannerText = p.message;
        break;
    case InjectionSequence::State::Cancelled:
        m_bannerKind = 3;
        m_bannerText = L"\u5df2\u53d6\u6d88\u6ce8\u5165";
        break;
    default:
        break;
    }

    if (!finished)
        m_lastSeqPhase = p.state;

    if (finished)
    {
        if (p.state == InjectionSequence::State::Done)
        {
            m_okCount++;
            m_mon.Observe(p.pid);
            if (m_set.toastOnDone)
                ShowToast(p.message, 1);
            if (m_set.autoSwitchOnDone)
                SwitchPage(PgLogs);
        }
        else if (p.state == InjectionSequence::State::Failed && m_set.toastOnDone)
        {
            ShowToast(p.message, 3);
        }
        m_layoutDirty = true;
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------- commands

void App::SwitchPage(PageId page)
{
    if (m_page == page)
        return;
    // Capture the outgoing view for the crossfade transition.
    SnapshotPageContent();
    m_page = page;
    m_hoverId = 0;
    m_pressId = 0;
    for (auto& kv : m_hoverAnims)
        kv.second.target = 0;
    m_pageScroll = 0;      // fresh page starts at the top
    m_pageScrollTarget = 0;
    m_pageThumbDrag = false;
    m_pageAnim.v = 0;
    m_pageAnim.target = 1;
    m_layoutDirty = true;  // Relayout re-places the EDIT children
    ApplyEditPlacement();  // hides the fields while the crossfade runs
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::SegChanged(int segId, int item)
{
    if (segId == ID_SEG_THEME)
    {
        m_set.theme = static_cast<ThemeMode>(item);
        ApplyThemeInternal();
        m_dirty = true;
    }
    else if (segId == ID_SEG_BACKDROP)
    {
        m_set.backdrop = item == 0 ? Backdrop::None
                        : item == 1 ? Backdrop::Mica : Backdrop::Acrylic;
        ApplyBackdrop(); // may revert + toast when unsupported
        m_dirty = true;
    }
    else if (segId == ID_SEG_LOGFILTER)
    {
        m_filter = item;
        const Layout& L = Lay();
        ClampLogScroll(L);
    }
    m_segAnims[segId].target = float(item); // slide the thumb
    m_layoutDirty = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::ToggleSetting(int id)
{
    if (id == ID_TGL_AUTOSWITCH)
        m_set.autoSwitchOnDone = !m_set.autoSwitchOnDone;
    else if (id == ID_TGL_TOASTDONE)
        m_set.toastOnDone = !m_set.toastOnDone;
    else if (id == ID_TGL_FOLLOW)
        m_set.followLog = !m_set.followLog;
    // Animate the knob to its new side.
    m_knobAnims[id].target = (id == ID_TGL_AUTOSWITCH ? m_set.autoSwitchOnDone
                          : id == ID_TGL_TOASTDONE ? m_set.toastOnDone
                                                   : m_set.followLog) ? 1.0f : 0.0f;
    m_dirty = true;
    m_layoutDirty = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::OnCommand(int id)
{
    if (id == ID_BTN_INJECT || id == ID_BTN_GOTO_INJECT)
    {
        if (id == ID_BTN_GOTO_INJECT)
            SwitchPage(PgInject);
        StartInject();
    }
    else if (id == ID_BTN_GOTO_SETTINGS)
    {
        SwitchPage(PgSettings);
    }
    else if (id == ID_BTN_CANCEL_INJ)
    {
        m_seq.Cancel();
        ShowToast(L"\u6b63\u5728\u53d6\u6d88\u2026", 0);
    }
    else if (id == ID_BTN_BROWSE_DLL || id == ID_ROW_DLL)
    {
        BrowseDll();
    }
    else if (id == ID_BTN_RECHECK)
    {
        RecheckTargets();
    }
    else if (id == ID_BTN_CLEAR_LOG)
    {
        m_lines.clear();
        m_logScroll = 0;
        m_logPinned = true;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (id == ID_BTN_OPEN_LOGFILE)
    {
        if (!m_logSource.empty() && FileExistsW(m_logSource))
            ShellExecuteW(m_hwnd, L"open", L"notepad.exe", m_logSource.c_str(), nullptr, SW_SHOWNORMAL);
        else
            ShowToast(L"\u65e5\u5fd7\u6587\u4ef6\u5c1a\u4e0d\u5b58\u5728", 2);
    }
    else if (id == ID_BTN_BROWSE_LOG || id == ID_ROW_LOG)
    {
        BrowseLog();
    }
    else if (id == ID_BTN_OPEN_LOGDIR)
    {
        if (!m_logSource.empty())
        {
            const std::wstring sel = L"/select,\"" + m_logSource + L"\"";
            ShellExecuteW(m_hwnd, L"open", L"explorer.exe", sel.c_str(), nullptr, SW_SHOWNORMAL);
        }
        else
        {
            ShellExecuteW(m_hwnd, L"open", L"explorer.exe", AppPaths::AppDir().c_str(), nullptr,
                          SW_SHOWNORMAL);
        }
    }
    else if (id == ID_BTN_RESET)
    {
        const SettingsData old = m_set;
        SettingsData fresh;
        fresh.hasPlacement = old.hasPlacement;
        fresh.winX = old.winX; fresh.winY = old.winY;
        fresh.winW = old.winW; fresh.winH = old.winH;
        fresh.maximized = old.maximized;
        m_set = fresh;
        m_dllFound = Injector::FileReadable(ResolvedDllPath());
        ApplyThemeInternal();
        ApplyBackdrop();
        SyncEditsFromSettings();
        m_tail.Rescan(m_set.logPathOverride);
        m_lines.clear();
        m_logSource.clear();
        m_logScroll = 0;
        SaveNow();
        m_layoutDirty = true;
        ShowToast(L"\u5df2\u6062\u590d\u9ed8\u8ba4\u8bbe\u7f6e", 1);
    }
    else if (id == ID_BTN_GITHUB)
    {
        ShellExecuteW(m_hwnd, L"open", kRepoUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
    else if (id == ID_BTN_ADMIN_RESTART)
    {
        wchar_t exe[MAX_PATH * 2] = {};
        GetModuleFileNameW(nullptr, exe, MICP_ARRLEN(exe));
        ShellExecuteW(m_hwnd, L"runas", exe, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void App::BrowseDll()
{
    wchar_t file[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"\u4ee3\u7406 DLL (Aurora.dll;*.dll)\0*.dll\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0"; // 代理 DLL / 所有文件
    ofn.lpstrFile = file;
    ofn.nMaxFile = MICP_ARRLEN(file);
    ofn.lpstrTitle = L"\u9009\u62e9\u4ee3\u7406 DLL\uff08Aurora.dll\uff09";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    const std::wstring startDir = AppPaths::AppDir();
    ofn.lpstrInitialDir = startDir.c_str();
    if (GetOpenFileNameW(&ofn))
    {
        m_set.dllPath = file;
        m_dllFound = Injector::FileReadable(m_set.dllPath);
        m_dirty = true;
        m_layoutDirty = true;
        ShowToast(L"\u5df2\u9009\u62e9\u4ee3\u7406 DLL", 1);
    }
}

void App::BrowseLog()
{
    wchar_t file[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"\u65e5\u5fd7\u6587\u4ef6 (*.log;*.txt)\0*.log;*.txt\0\u6240\u6709\u6587\u4ef6 (*.*)\0*.*\0"; // 日志文件 / 所有文件
    ofn.lpstrFile = file;
    ofn.nMaxFile = MICP_ARRLEN(file);
    ofn.lpstrTitle = L"\u9009\u62e9\u65e5\u5fd7\u6587\u4ef6";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn))
    {
        m_set.logPathOverride = file;
        m_tail.Rescan(file);
        m_lines.clear();
        m_logSource.clear();
        m_logScroll = 0;
        m_logPinned = true;
        m_dirty = true;
        m_layoutDirty = true;
        ShowToast(L"\u5df2\u5207\u6362\u65e5\u5fd7\u6765\u6e90", 1);
    }
}

// ------------------------------------------------------------------- logs

bool App::PassesFilter(const LogEntry& e) const
{
    switch (m_filter)
    {
    case LfInfo: return e.level == 0 || e.level == 3;
    case LfWarn: return e.level == 1;
    case LfError: return e.level == 2;
    default: return true;
    }
}

float App::LogMaxScroll(const Layout& L) const
{
    const float rowH = float(S(24));
    int matched = 0;
    for (const auto& e : m_lines)
        if (PassesFilter(e))
            ++matched;
    const float contentH = matched * rowH;
    return contentH > L.lgBody.h ? contentH - L.lgBody.h : 0.0f;
}

void App::ClampLogScroll(const Layout& L)
{
    const float maxScroll = LogMaxScroll(L);
    if (m_logPinned && m_set.followLog)
    {
        m_logScroll = maxScroll;
        m_logScrollTarget = maxScroll;
    }
    if (m_logScroll < 0)
        m_logScroll = 0;
    if (m_logScroll > maxScroll)
        m_logScroll = maxScroll;
    if (m_logScrollTarget < 0)
        m_logScrollTarget = 0;
    if (m_logScrollTarget > maxScroll)
        m_logScrollTarget = maxScroll;
    m_logPinned = (m_logScroll >= maxScroll - 0.5f);
}

void App::AppendLogLines(const std::vector<std::wstring>& lines)
{
    for (const auto& raw : lines)
    {
        LogEntry e;
        e.text = raw;
        e.level = DetectLevel(raw);
        e.time = DetectTime(raw);
        m_lines.push_back(std::move(e));
    }
    while (m_lines.size() > 4000)
    {
        m_lines.erase(m_lines.begin());
        if (!m_logPinned)
            m_logScroll = m_logScroll > float(S(24)) ? m_logScroll - float(S(24)) : 0;
    }
    const Layout& L = Lay();
    ClampLogScroll(L);
}

// ------------------------------------------------------------------ toast

void App::ShowToast(const std::wstring& text, int kind)
{
    m_toastText = text;
    m_toastKind = kind;
    m_toastUntil = GetTickCount64() + 2600;
    m_toastAnim.v = 0;     // replay the slide-in even if one is already up
    m_toastAnim.target = 1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ------------------------------------------------------------------ save

void App::SaveNow()
{
    if (!m_dirty)
        return;
    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    GetWindowPlacement(m_hwnd, &wp);
    if (!IsIconic(m_hwnd) && wp.showCmd != SW_SHOWMAXIMIZED)
    {
        RECT rc = wp.rcNormalPosition;
        m_set.winX = rc.left;
        m_set.winY = rc.top;
        m_set.winW = rc.right - rc.left;
        m_set.winH = rc.bottom - rc.top;
        m_set.hasPlacement = true;
    }
    m_set.maximized = (IsZoomed(m_hwnd) != FALSE);
    SettingsStore::Save(m_set);
    m_dirty = false;
}

// ------------------------------------------------------------ log parsing

int App::DetectLevel(const std::wstring& line)
{
    // Inspect the first 40 characters, case-insensitive.
    std::wstring head = line.size() > 40 ? line.substr(0, 40) : line;
    for (auto& c : head)
        c = towupper(c);
    const size_t p1 = head.find(L'[');
    const size_t p2 = head.find(L']');
    std::wstring tag;
    if (p1 != std::wstring::npos && p2 != std::wstring::npos && p2 > p1 && p1 < 8)
        tag = head.substr(p1 + 1, p2 - p1 - 1);
    else
        tag = head;

    if (tag.find(L"ERROR") != std::wstring::npos || tag.find(L"FATAL") != std::wstring::npos
        || tag.find(L"CRIT") != std::wstring::npos || tag.find(L"ERR") != std::wstring::npos)
        return 2;
    if (tag.find(L"WARN") != std::wstring::npos)
        return 1;
    if (tag.find(L"DEBUG") != std::wstring::npos || tag.find(L"TRACE") != std::wstring::npos)
        return 3;
    return 0;
}

std::wstring App::DetectTime(const std::wstring& line)
{
    // [HH:MM:SS] or HH:MM:SS prefix.
    size_t i = 0;
    std::wstring head = line.size() > 24 ? line.substr(0, 24) : line;
    if (!head.empty() && head[0] == L'[')
        i = 1;
    if (head.size() >= i + 8
        && iswdigit(head[i]) && iswdigit(head[i + 1]) && head[i + 2] == L':'
        && iswdigit(head[i + 3]) && iswdigit(head[i + 4]) && head[i + 5] == L':'
        && iswdigit(head[i + 6]) && iswdigit(head[i + 7]))
        return head.substr(i, 8);
    return L"";
}

} // namespace micp
