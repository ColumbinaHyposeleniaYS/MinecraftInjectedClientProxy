#include "app/Theme.h"

#include "app/Lang.h"

#include <cstdarg>
#include <cwctype>
#include <cstring>

namespace micp {

const wchar_t* kAppName = L"MinecraftProxy";
const wchar_t* kRepoUrl = L"https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy";

#ifndef MICP_APP_VERSION
#define MICP_APP_VERSION "1.0.0"
#endif

const wchar_t* AppVersion()
{
    static std::wstring cached;
    if (cached.empty())
    {
        const char* v = MICP_APP_VERSION;
        cached.assign(v, v + strlen(v));
    }
    return cached.c_str();
}

// ---------------------------------------------------------------- utilities

std::wstring FormatStr(const wchar_t* fmt, ...)
{
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, MICP_ARRLEN(buf), fmt, args);
    va_end(args);
    return buf;
}

bool HasCJK(const std::wstring& s)
{
    for (wchar_t c : s)
    {
        if (c >= 0x2E80 && c <= 0x9FFF)
            return true;
        if (c >= 0xF900 && c <= 0xFAFF)
            return true;
        if (c >= 0xFF00 && c <= 0xFFEF)
            return true; // fullwidth forms
    }
    return false;
}

std::wstring TrimW(const std::wstring& s)
{
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a]))
        ++a;
    while (b > a && iswspace(s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

bool FileExistsW(const std::wstring& path)
{
    if (path.empty())
        return false;
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// ------------------------------------------------------------------- theme

namespace {

bool g_systemDark = false;
ThemeMode g_mode = ThemeMode::System;
bool g_dark = false;

const Pal kLight = {
    Gdiplus::Color(0xFFF5F5F7),  // windowBg
    Gdiplus::Color(0xFFEDEDF0),  // sidebarBg
    Gdiplus::Color(0xFFFFFFFF),  // cardBg
    Gdiplus::Color(0x1A000000),  // cardBorder
    Gdiplus::Color(0xFFFFFFFF),  // fieldBg
    Gdiplus::Color(0x2E3C3C43),  // fieldBorder
    Gdiplus::Color(0x0D000000),  // hoverFill
    Gdiplus::Color(0x14000000),  // separator
    Gdiplus::Color(0xFF1D1D1F),  // text
    Gdiplus::Color(0xFF6E6E73),  // text2
    Gdiplus::Color(0xFFAEAEB2),  // text3
    Gdiplus::Color(0xFF0A84FF),  // accent
    Gdiplus::Color(0xFF3D9BFF),  // accentHover
    Gdiplus::Color(0xFF0872D9),  // accentPress
    Gdiplus::Color(0xFF34C759),  // success
    Gdiplus::Color(0xFFFF9500),  // warning
    Gdiplus::Color(0xFFFF3B30),  // danger
    Gdiplus::Color(0xF0212125),  // toastBg
    Gdiplus::Color(0xFFF5F5F7),  // toastText
    Gdiplus::Color(0xFF6E6E73),  // lvInfo
    Gdiplus::Color(0xFF9B51E0),  // lvDebug
    Gdiplus::Color(0xFFCC7A00),  // lvWarn (readable on white)
    Gdiplus::Color(0xFFD70015),  // lvError
    Gdiplus::Color(0xFFAEAEB2),  // lvTime
    Gdiplus::Color(0xFF1D1D1F),  // lvText
    0.92f, 0.82f, 0.99f,         // mica alphas
};

const Pal kDark = {
    Gdiplus::Color(0xFF1B1B1F),  // windowBg
    Gdiplus::Color(0xFF232327),  // sidebarBg
    Gdiplus::Color(0xFF2A2A2E),  // cardBg
    Gdiplus::Color(0x17FFFFFF),  // cardBorder
    Gdiplus::Color(0xFF38383D),  // fieldBg
    Gdiplus::Color(0x24FFFFFF),  // fieldBorder
    Gdiplus::Color(0x0FFFFFFF),  // hoverFill
    Gdiplus::Color(0x1AFFFFFF),  // separator
    Gdiplus::Color(0xFFF5F5F7),  // text
    Gdiplus::Color(0xFF98989F),  // text2
    Gdiplus::Color(0xFF6E6E73),  // text3
    Gdiplus::Color(0xFF0A84FF),  // accent
    Gdiplus::Color(0xFF3D9BFF),  // accentHover
    Gdiplus::Color(0xFF0872D9),  // accentPress
    Gdiplus::Color(0xFF30D158),  // success
    Gdiplus::Color(0xFFFF9F0A),  // warning
    Gdiplus::Color(0xFFFF453A),  // danger
    Gdiplus::Color(0xF2505055),  // toastBg
    Gdiplus::Color(0xFFF5F5F7),  // toastText
    Gdiplus::Color(0xFF98989F),  // lvInfo
    Gdiplus::Color(0xFFBF5AF2),  // lvDebug
    Gdiplus::Color(0xFFFF9F0A),  // lvWarn
    Gdiplus::Color(0xFFFF6961),  // lvError
    Gdiplus::Color(0xFF6E6E73),  // lvTime
    Gdiplus::Color(0xFFEBEBF0),  // lvText
    0.94f, 0.86f, 0.985f,        // mica alphas
};

void ReadSystemDark()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        DWORD value = 1, size = sizeof(value);
        if (RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS)
            g_systemDark = (value == 0);
        RegCloseKey(key);
    }
}

} // namespace

void SetThemeMode(ThemeMode mode)
{
    g_mode = mode;
    g_dark = (mode == ThemeMode::Dark) || (mode == ThemeMode::System && g_systemDark);
}

ThemeMode ThemeModeNow() { return g_mode; }

void RefreshSystemDark()
{
    ReadSystemDark();
    SetThemeMode(g_mode);
}

bool IsDark() { return g_dark; }

const Pal& Palette() { return g_dark ? kDark : kLight; }

// ------------------------------------------------------------------- fonts

namespace {

struct FontSlot
{
    HFONT latin = nullptr;
    HFONT cjk = nullptr;
    Gdiplus::Font* gLatin = nullptr;
    Gdiplus::Font* gCjk = nullptr;
    int height = 0;
};

struct FontSpec
{
    int px96;      // design pixel height at 96 dpi
    int weight;    // GDI weight
    bool mono;     // use the mono faces
};

const FontSpec kSpecs[FCount] = {
    /* FTitle     */ {24, 600, false},
    /* FHead      */ {15, 600, false},
    /* FBody      */ {13, 400, false},
    /* FBodySem   */ {13, 600, false},
    /* FSmall     */ {12, 400, false},
    /* FSmallSem  */ {12, 600, false},
    /* FMono      */ {12, 400, true},
    /* FMonoSmall */ {11, 400, true},
};

FontSlot g_slots[FCount];
int g_dpi = 96;

void DestroyFonts()
{
    for (FontSlot& slot : g_slots)
    {
        delete slot.gLatin;
        delete slot.gCjk;
        slot.gLatin = slot.gCjk = nullptr;
        if (slot.latin)
        {
            DeleteObject(slot.latin);
            slot.latin = nullptr;
        }
        if (slot.cjk)
        {
            DeleteObject(slot.cjk);
            slot.cjk = nullptr;
        }
    }
}

int MeasureFontHeight(HFONT font)
{
    HDC dc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm = {};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return tm.tmHeight;
}

} // namespace

void SetDpi(int dpi)
{
    if (dpi > 0)
        g_dpi = dpi;
}

int Dpi() { return g_dpi; }

void RebuildFonts(int dpi)
{
    SetDpi(dpi);
    DestroyFonts();

    const double scale = dpi / 96.0;
    HDC screen = GetDC(nullptr);

    for (int i = 0; i < FCount; ++i)
    {
        const FontSpec& spec = kSpecs[i];
        const int px = int(spec.px96 * scale + 0.5);

        // The CJK companion face depends on the active language (YaHei /
        // JhengHei / Yu Gothic); the latin face is always Segoe UI Variable.
        const wchar_t* latin = spec.mono ? L"Consolas" : L"Segoe UI Variable Text";
        const wchar_t* cjk = spec.mono ? L"Microsoft YaHei UI" : lang::CjkFontFace();
        const int cjkWeight = spec.mono ? 400 : spec.weight;

        g_slots[i].latin = CreateFontW(-px, 0, 0, 0, spec.weight, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH, latin);
        g_slots[i].cjk = CreateFontW(-px, 0, 0, 0, cjkWeight, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, cjk);
        if (g_slots[i].latin)
            g_slots[i].gLatin = new Gdiplus::Font(screen, g_slots[i].latin);
        if (g_slots[i].cjk)
            g_slots[i].gCjk = new Gdiplus::Font(screen, g_slots[i].cjk);
        const int hLatin = MeasureFontHeight(g_slots[i].latin ? g_slots[i].latin : g_slots[i].cjk);
        const int hCjk = MeasureFontHeight(g_slots[i].cjk);
        g_slots[i].height = hLatin > hCjk ? hLatin : hCjk;
    }

    ReleaseDC(nullptr, screen);
}

Gdiplus::Font* GFont(FontId id, bool cjk)
{
    FontSlot& slot = g_slots[id];
    Gdiplus::Font* f = cjk ? slot.gCjk : slot.gLatin;
    return f ? f : slot.gLatin;
}

HFONT WFont(FontId id, bool cjk)
{
    FontSlot& slot = g_slots[id];
    return (cjk && slot.cjk) ? slot.cjk : slot.latin;
}

int LineH(FontId id) { return g_slots[id].height; }

} // namespace micp
