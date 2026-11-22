#pragma once
// Immediate-mode drawing primitives and controls. Everything is rendered with
// GDI+ into the app's 32bpp back buffer; layout rects are plain structs so the
// same numbers can be recomputed for hit-testing without a paint pass.
//
// Hover feedback is animated by the caller: instead of a discrete HoverState,
// controls take a 0..1 fade value (plus a pressed flag) so the App animation
// clock can ease colors in and out like macOS does.

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

// Single-line text. The string is converted to a GraphicsPath so the *ink*
// (the pixels actually covered by the glyphs) can be centered in the rect -
// GDI+'s line-box centering leaves CJK faces like Microsoft YaHei visibly
// off-center. Strings wider than the rect fall back to DrawString with
// ellipsis trimming. wrap=true uses the classic multi-line path.
void Text(Gdiplus::Graphics& g, const std::wstring& s, FontId font, const Gdiplus::Color& c,
          const RectF& rc, Align align = Align::Left, bool wrap = false);
float MeasureText(const std::wstring& s, FontId font); // tight ink width in px
RectF TextRect(const std::wstring& s, FontId font, const RectF& anchor,
               Align align = Align::Left);

// --- icons ----------------------------------------------------------------

void Icon(Gdiplus::Graphics& g, IconKind kind, const RectF& box, const Gdiplus::Color& c,
          float strokeW = 1.5f);

// --- controls -------------------------------------------------------------

// style: 0 = primary (accent fill), 1 = secondary, 2 = ghost (text only),
// 3 = small secondary (used inline inside rows).
// hover: 0..1 fade, pressed: mouse is down on the button.
void Button(Gdiplus::Graphics& g, const RectF& r, const wchar_t* label, int style,
            float hover, bool pressed, bool enabled, bool danger = false);

// knob: 0..1 animated switch position.
void Toggle(Gdiplus::Graphics& g, const RectF& r, float knob, float hover);

// Segmented control. selPos is a float so the selected thumb can slide
// between items; hoverItem/hoverFade ease the item highlight.
// Returns the item rects through outRects (size >= items).
void Segmented(Gdiplus::Graphics& g, const RectF& r, const std::vector<std::wstring>& items,
               float selPos, int hoverItem, float hoverFade, RectF* outRects);
std::vector<RectF> SegmentedLayout(const RectF& r, size_t items); // shared geometry

// Field-like row: icon + ellipsized value + trailing chevron.
void PathRow(Gdiplus::Graphics& g, const RectF& r, const std::wstring& value,
             IconKind icon, float hover, bool alert);

// macOS traffic lights. groupFade eases the glyph reveal when the cursor
// enters the button group; hoverIdx is the dot under the cursor (-1 none).
void TrafficLights(Gdiplus::Graphics& g, const RectF r[3], bool windowActive,
                   float groupFade, const float hoverFade[3], int pressIdx);

// Sidebar navigation entry.
void NavItem(Gdiplus::Graphics& g, const RectF& r, IconKind icon, const wchar_t* label,
             bool selected, float hover);

// Injection step bullet: state 0 pending, 1 active (spinner), 2 done, 3 error.
void StepBullet(Gdiplus::Graphics& g, const RectF& r, int state, int index, int spinnerPhase);

// Small colored dot for status lines.
void StatusDot(Gdiplus::Graphics& g, const RectF& r, const Gdiplus::Color& c);

// --- color helpers ----------------------------------------------------------

Gdiplus::Color WithAlpha(const Gdiplus::Color& c, float alpha01);
Gdiplus::Color Mix(const Gdiplus::Color& a, const Gdiplus::Color& b, float t);

} // namespace ui
} // namespace micp
