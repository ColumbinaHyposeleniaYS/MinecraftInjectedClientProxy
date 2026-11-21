#pragma once
// Color palettes, theme resolution and font management.
//
// Fonts are GDI HFONTs (so they can be handed to the EDIT child controls)
// wrapped once per DPI change into Gdiplus::Font objects. Two faces exist per
// role: a Latin face (Segoe UI Variable / Segoe UI) and a CJK face
// (Microsoft YaHei UI); the text helper picks per string so Chinese labels
// render deterministically.

#include "common.h"
#include <gdiplus.h>

namespace micp {

extern const wchar_t* kAppName;
extern const wchar_t* kRepoUrl;
const wchar_t* AppVersion();

enum class ThemeMode { System, Light, Dark };
enum class Backdrop { None, Mica, Acrylic };

struct Pal {
    // surfaces
    Gdiplus::Color windowBg, sidebarBg, cardBg, cardBorder;
    Gdiplus::Color fieldBg, fieldBorder, hoverFill, separator;
    // text
    Gdiplus::Color text, text2, text3;
    // accent
    Gdiplus::Color accent, accentHover, accentPress;
    // semantic
    Gdiplus::Color success, warning, danger;
    // toast
    Gdiplus::Color toastBg, toastText;
    // log view
    Gdiplus::Color lvInfo, lvDebug, lvWarn, lvError, lvTime, lvText;
    // translucency when a DWM backdrop is active (0-255 base alphas)
    float micaWindowAlpha, micaSidebarAlpha, micaCardAlpha;
};

void SetThemeMode(ThemeMode mode);
ThemeMode ThemeModeNow();
void RefreshSystemDark();          // re-read the registry (AppsUseLightTheme)
bool IsDark();                     // resolved dark flag for the current mode
const Pal& Palette();

// ----- fonts ---------------------------------------------------------------

enum FontId {
    FTitle = 0, // 24 semibold  - page titles, hero status
    FHead,      // 15 semibold  - card headers
    FBody,      // 13 regular   - body copy
    FBodySem,   // 13 semibold  - emphasized rows / buttons on cards
    FSmall,     // 12 regular   - secondary copy
    FSmallSem,  // 12 semibold  - badges
    FMono,      // 12 mono      - log lines, paths, pids
    FMonoSmall, // 11 mono      - log timestamps
    FCount
};

void RebuildFonts(int dpi);
Gdiplus::Font* GFont(FontId id, bool cjk);
HFONT WFont(FontId id, bool cjk);
int LineH(FontId id); // pixel line height (max of both faces)

// DPI helpers. S() scales a 96dpi design unit to the current monitor.
void SetDpi(int dpi);
int Dpi();
inline float SF(float v) { return v * (Dpi() / 96.0f); }
inline int S(int v) { return int(v * (Dpi() / 96.0f) + 0.5f); }

} // namespace micp
