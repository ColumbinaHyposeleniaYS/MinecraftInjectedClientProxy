#pragma once
// Immediate-mode drawing primitives and controls. Everything is rendered with
// GDI+ into the app's 32bpp back buffer; layout rects are plain structs so the
// same numbers can be recomputed for hit-testing without a paint pass.

#include "app/Theme.h"

namespace micp {

struct RectF
{
    float x = 0, y = 0, w = 0, h = 0;
    RectF() = default;
    RectF(float px, float py, float pw, float ph) : x(px), y(py), w(pw), h(ph) {}
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    bool contains(float px, float py) const { return px >= x && px < right() && py >= y && py < bottom(); }
    RectF inset(float d) const { return RectF(x + d, y + d, w - 2 * d, h - 2 * d); }
    RectF centered(float pw, float ph) const { return RectF(x + (w - pw) / 2, y + (h - ph) / 2, pw, ph); }
    Gdiplus::RectF gp() const { return Gdiplus::RectF(x, y, w, h); }
};

enum class HoverState { None, Hover, Press };
enum class IconKind {
    Gauge, Zap, Doc, Gear, Info, Folder, Check, Close, Warn, Link,
    Refresh, Search, Cube, Terminal, Plug
};

namespace ui {

// --- primitives -----------------------------------------------------------

void RoundRect(Gdiplus::Graphics& g, const RectF& r, float radius,
               const Gdiplus::Color* fill, const Gdiplus::Color* border = nullptr,
               float borderW = 1.0f);
void HLine(Gdiplus::Graphics& g, float x, float y, float w, const Gdiplus::Color& c);

enum class Align { Left, Center, Right };

// Single-line text with automatic ellipsis; wraps only when wrap=true.
void Text(Gdiplus::Graphics& g, const std::wstring& s, FontId font, const Gdiplus::Color& c,
          const RectF& rc, Align align = Align::Left, bool wrap = false);
float MeasureText(const std::wstring& s, FontId font); // width in px
RectF TextRect(const std::wstring& s, FontId font, const RectF& anchor,
               Align align = Align::Left);

// --- icons ----------------------------------------------------------------

void Icon(Gdiplus::Graphics& g, IconKind kind, const RectF& box, const Gdiplus::Color& c,
          float strokeW = 1.5f);

// --- controls -------------------------------------------------------------

// style: 0 = primary (accent fill), 1 = secondary, 2 = ghost (text only),
// 3 = small secondary (used inline inside rows).
void Button(Gdiplus::Graphics& g, const RectF& r, const wchar_t* label, int style,
            HoverState hs, bool enabled, bool danger = false);

void Toggle(Gdiplus::Graphics& g, const RectF& r, bool on, HoverState hs);

// Segmented control. Returns the item rects through outRects (size >= items).
void Segmented(Gdiplus::Graphics& g, const RectF& r, const std::vector<std::wstring>& items,
               int sel, int hoverItem, RectF* outRects);
std::vector<RectF> SegmentedLayout(const RectF& r, size_t items); // shared geometry

// Field-like row: icon + ellipsized value + optional trailing hint color.
void PathRow(Gdiplus::Graphics& g, const RectF& r, const std::wstring& value,
             IconKind icon, HoverState hs, bool alert);

// macOS traffic lights. tlHover = any dot hovered (glyphs become visible),
// hoverIdx = the dot currently under the cursor (-1 none).
void TrafficLights(Gdiplus::Graphics& g, const RectF r[3], bool windowActive,
                   bool anyHover, int hoverIdx, int pressIdx);

// Sidebar navigation entry.
void NavItem(Gdiplus::Graphics& g, const RectF& r, IconKind icon, const wchar_t* label,
             bool selected, HoverState hs);

// Injection step bullet: state 0 pending, 1 active (spinner), 2 done, 3 error.
void StepBullet(Gdiplus::Graphics& g, const RectF& r, int state, int spinnerPhase);

// Small colored dot for status lines.
void StatusDot(Gdiplus::Graphics& g, const RectF& r, const Gdiplus::Color& c);

} // namespace ui
} // namespace micp
