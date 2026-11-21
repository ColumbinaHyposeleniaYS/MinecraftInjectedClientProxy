#include "app/Ui.h"

#include <cmath>
#include <initializer_list>

namespace micp {
namespace ui {

// ------------------------------------------------------------ measure gfx

namespace {

struct MeasureCtx
{
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    Gdiplus::Graphics* g = nullptr;
    ~MeasureCtx()
    {
        delete g;
        if (dc)
        {
            DeleteDC(dc);
            dc = nullptr;
        }
        if (dib)
            DeleteObject(dib);
    }
};

Gdiplus::Graphics& MeasureGfx()
{
    static MeasureCtx ctx;
    if (!ctx.g)
    {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = 8;
        bi.bmiHeader.biHeight = -8;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        ctx.dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        ctx.dc = CreateCompatibleDC(nullptr);
        SelectObject(ctx.dc, ctx.dib);
        ctx.g = new Gdiplus::Graphics(ctx.dc);
    }
    return *ctx.g;
}

Gdiplus::Color WithAlpha(const Gdiplus::Color& c, float alpha01)
{
    return Gdiplus::Color(BYTE(255 * alpha01), c.GetR(), c.GetG(), c.GetB());
}

} // namespace

// ------------------------------------------------------------- primitives

void RoundRect(Gdiplus::Graphics& g, const RectF& r, float radius,
               const Gdiplus::Color* fill, const Gdiplus::Color* border, float borderW)
{
    if (r.w <= 0 || r.h <= 0)
        return;
    if (radius < 0.5f)
        radius = 0.5f;
    const float d = radius * 2;
    Gdiplus::GraphicsPath path;
    path.AddArc(r.x, r.y, d, d, 180, 90);
    path.AddArc(r.right() - d, r.y, d, d, 270, 90);
    path.AddArc(r.right() - d, r.bottom() - d, d, d, 0, 90);
    path.AddArc(r.x, r.bottom() - d, d, d, 90, 90);
    path.CloseFigure();
    if (fill)
    {
        Gdiplus::SolidBrush brush(*fill);
        g.FillPath(&brush, &path);
    }
    if (border)
    {
        Gdiplus::Pen pen(*border, borderW);
        pen.SetAlignment(Gdiplus::PenAlignmentInset);
        g.DrawPath(&pen, &path);
    }
}

void HLine(Gdiplus::Graphics& g, float x, float y, float w, const Gdiplus::Color& c)
{
    Gdiplus::SolidBrush brush(c);
    g.FillRectangle(&brush, Gdiplus::RectF(x, y, w, 1.0f));
}

void Text(Gdiplus::Graphics& g, const std::wstring& s, FontId font, const Gdiplus::Color& c,
          const RectF& rc, Align align, bool wrap)
{
    if (s.empty() || rc.w <= 0 || rc.h <= 0)
        return;
    Gdiplus::Font* f = GFont(font, HasCJK(s));
    if (!f)
        return;

    Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericDefault());
    sf.SetAlignment(align == Align::Left ? Gdiplus::StringAlignmentNear
                    : align == Align::Center ? Gdiplus::StringAlignmentCenter
                                             : Gdiplus::StringAlignmentFar);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    if (!wrap)
    {
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap
                          | Gdiplus::StringFormatFlagsNoClip);
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    }

    Gdiplus::SolidBrush brush(c);
    g.DrawString(s.c_str(), -1, f, rc.gp(), &sf, &brush);
}

float MeasureText(const std::wstring& s, FontId font)
{
    if (s.empty())
        return 0;
    Gdiplus::Graphics& g = MeasureGfx();
    Gdiplus::Font* f = GFont(font, HasCJK(s));
    if (!f)
        return 0;
    Gdiplus::StringFormat sf(Gdiplus::StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
    Gdiplus::RectF box(0, 0, 100000, 100000);
    Gdiplus::RectF out;
    g.MeasureString(s.c_str(), -1, f, box, &sf, &out);
    return out.Width;
}

RectF TextRect(const std::wstring& s, FontId font, const RectF& anchor, Align align)
{
    const float w = MeasureText(s, font);
    const float h = float(LineH(font));
    if (align == Align::Left)
        return RectF(anchor.x, anchor.y, w, h);
    if (align == Align::Center)
        return RectF(anchor.x + (anchor.w - w) / 2, anchor.y, w, h);
    return RectF(anchor.right() - w, anchor.y, w, h);
}

// ------------------------------------------------------------------ icons

namespace {

struct Pt
{
    float x, y;
};

void PathPolyPts(Gdiplus::Graphics& g, const Pt* pts, int count, Gdiplus::Pen& pen, bool close)
{
    Gdiplus::GraphicsPath path;
    Gdiplus::PointF first(pts[0].x, pts[0].y);
    path.AddLine(first, first);
    for (int i = 1; i < count; ++i)
        path.AddLine(Gdiplus::PointF(pts[i - 1].x, pts[i - 1].y), Gdiplus::PointF(pts[i].x, pts[i].y));
    if (close)
        path.CloseFigure();
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawPath(&pen, &path);
}

void PathLines(Gdiplus::Graphics& g, std::initializer_list<Pt> pts, Gdiplus::Pen& pen)
{
    PathPolyPts(g, pts.begin(), int(pts.size()), pen, false);
}

} // namespace

void Icon(Gdiplus::Graphics& g, IconKind kind, const RectF& box, const Gdiplus::Color& c,
          float strokeW)
{
    if (box.w <= 0 || box.h <= 0)
        return;
    Gdiplus::GraphicsState state = g.Save();
    // Map the 24x24 design space onto the box.
    Gdiplus::Matrix m(box.w / 24.0f, 0, 0, box.h / 24.0f, box.x, box.y);
    g.SetTransform(&m);
    g.SetPageUnit(Gdiplus::UnitPixel);

    // Pen widths are expressed in the 24x24 design space; the world
    // transform below scales them to the box automatically.
    Gdiplus::Pen pen(c, strokeW);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);

    switch (kind)
    {
    case IconKind::Gauge:
    {
        Gdiplus::RectF arc(2.6f, 2.6f, 18.8f, 18.8f);
        g.DrawArc(&pen, arc, 135, 270);
        PathLines(g, {{12, 13}, {16.8f, 8.2f}}, pen);
        Gdiplus::SolidBrush dot(c);
        g.FillEllipse(&dot, Gdiplus::RectF(10.9f, 11.9f, 2.2f, 2.2f));
        break;
    }
    case IconKind::Zap:
    {
        const Pt bolt[] = {{13.2f, 2.6f}, {5.4f, 13.4f}, {10.8f, 13.4f}, {10.2f, 21.4f},
                           {18.6f, 10.2f}, {13.2f, 10.2f}};
        PathPolyPts(g, bolt, MICP_ARRLEN(bolt), pen, true);
        break;
    }
    case IconKind::Doc:
    {
        RoundRect(g, RectF(5, 2.8f, 14, 18.4f), 2.2f, nullptr, &c, strokeW);
        for (float y = 8.2f; y <= 16.2f; y += 4)
            PathLines(g, {{8.4f, y}, {15.6f, y}}, pen);
        break;
    }
    case IconKind::Gear:
    {
        Gdiplus::RectF ring(6.2f, 6.2f, 11.6f, 11.6f);
        g.DrawEllipse(&pen, ring);
        for (int i = 0; i < 8; ++i)
        {
            const double a = i * 3.14159265358979 / 4.0;
            const float c1 = 12 + float(9.8f * cos(a));
            const float s1 = 12 + float(9.8f * sin(a));
            const float c0 = 12 + float(6.8f * cos(a));
            const float s0 = 12 + float(6.8f * sin(a));
            PathLines(g, {{c0, s0}, {c1, s1}}, pen);
        }
        break;
    }
    case IconKind::Info:
    {
        Gdiplus::RectF ring(3.2f, 3.2f, 17.6f, 17.6f);
        g.DrawEllipse(&pen, ring);
        Gdiplus::SolidBrush dot(c);
        g.FillEllipse(&dot, Gdiplus::RectF(10.8f, 7.2f, 2.4f, 2.4f));
        PathLines(g, {{12, 11.4f}, {12, 16.6f}}, pen);
        break;
    }
    case IconKind::Folder:
    {
        const Pt fold[] = {{3.4f, 5.6f}, {9.8f, 5.6f}, {12.2f, 8.2f}, {20.6f, 8.2f},
                           {20.6f, 18.6f}, {3.4f, 18.6f}};
        PathPolyPts(g, fold, MICP_ARRLEN(fold), pen, true);
        break;
    }
    case IconKind::Check:
        PathLines(g, {{5, 12.6f}, {9.8f, 17.2f}, {19, 6.8f}}, pen);
        break;
    case IconKind::Close:
        PathLines(g, {{6.4f, 6.4f}, {17.6f, 17.6f}}, pen);
        PathLines(g, {{17.6f, 6.4f}, {6.4f, 17.6f}}, pen);
        break;
    case IconKind::Warn:
    {
        const Pt tri[] = {{12, 3.8f}, {22, 20.2f}, {2, 20.2f}};
        PathPolyPts(g, tri, MICP_ARRLEN(tri), pen, true);
        PathLines(g, {{12, 9.4f}, {12, 13.4f}}, pen);
        Gdiplus::SolidBrush dot(c);
        g.FillEllipse(&dot, Gdiplus::RectF(11, 16.2f, 2, 2));
        break;
    }
    case IconKind::Link:
    {
        RoundRect(g, RectF(3.8f, 8.6f, 11.6f, 11.6f), 2.2f, nullptr, &c, strokeW);
        PathLines(g, {{13.2f, 10.8f}, {20.4f, 3.6f}}, pen);
        PathLines(g, {{14.8f, 3.6f}, {20.4f, 3.6f}, {20.4f, 9.2f}}, pen);
        break;
    }
    case IconKind::Refresh:
    {
        Gdiplus::RectF arc(3.8f, 3.8f, 16.4f, 16.4f);
        g.DrawArc(&pen, arc, 30, 280);
        const double a = 30.0 * 3.14159265358979 / 180.0;
        const float ex = 12 + float(8.2f * cos(a));
        const float ey = 12 + float(8.2f * sin(a));
        const Pt head[] = {{ex + 2.6f, ey - 0.6f}, {ex - 0.4f, ey - 0.2f}, {ex + 0.6f, ey + 2.8f}};
        PathPolyPts(g, head, MICP_ARRLEN(head), pen, false);
        break;
    }
    case IconKind::Search:
    {
        Gdiplus::RectF ring(4.2f, 4.2f, 11.2f, 11.2f);
        g.DrawEllipse(&pen, ring);
        PathLines(g, {{13.8f, 13.8f}, {19.6f, 19.6f}}, pen);
        break;
    }
    case IconKind::Cube:
    {
        const Pt hex[] = {{12, 2.8f}, {20.2f, 7.4f}, {20.2f, 16.6f}, {12, 21.2f},
                          {3.8f, 16.6f}, {3.8f, 7.4f}};
        PathPolyPts(g, hex, MICP_ARRLEN(hex), pen, true);
        PathLines(g, {{12, 12}, {20.2f, 7.4f}}, pen);
        PathLines(g, {{12, 12}, {3.8f, 7.4f}}, pen);
        PathLines(g, {{12, 12}, {12, 21.2f}}, pen);
        break;
    }
    case IconKind::Terminal:
    {
        RoundRect(g, RectF(3, 4.6f, 18, 14.8f), 2.2f, nullptr, &c, strokeW);
        PathLines(g, {{6.8f, 9.4f}, {9.2f, 12}, {6.8f, 14.6f}}, pen);
        PathLines(g, {{11.8f, 14.6f}, {16.4f, 14.6f}}, pen);
        break;
    }
    case IconKind::Plug:
    {
        PathLines(g, {{9.4f, 5.6f}, {9.4f, 2.4f}}, pen);
        PathLines(g, {{14.6f, 5.6f}, {14.6f, 2.4f}}, pen);
        RoundRect(g, RectF(7, 5.6f, 10, 8.2f), 2.0f, nullptr, &c, strokeW);
        PathLines(g, {{12, 13.8f}, {12, 17.6f}}, pen);
        PathLines(g, {{8.4f, 17.6f}, {15.6f, 17.6f}}, pen);
        PathLines(g, {{8.4f, 21}, {15.6f, 21}}, pen);
        break;
    }
    }

    g.Restore(state);
}

// ---------------------------------------------------------------- controls

namespace {

Gdiplus::Color MixFill(float normal, float hover, float press, HoverState hs)
{
    float a = normal;
    if (hs == HoverState::Hover)
        a = hover;
    else if (hs == HoverState::Press)
        a = press;
    return Gdiplus::Color(BYTE(255 * a), 120, 120, 128);
}

} // namespace

void Button(Gdiplus::Graphics& g, const RectF& r, const wchar_t* label, int style,
            HoverState hs, bool enabled, bool danger)
{
    const Pal& pal = Palette();
    const float radius = r.h >= S(34) ? 9.0f : 7.0f;

    Gdiplus::Color fill, textColor;
    float borderAlpha = 0;

    if (style == 0) // primary
    {
        fill = pal.accent;
        if (hs == HoverState::Hover)
            fill = pal.accentHover;
        else if (hs == HoverState::Press)
            fill = pal.accentPress;
        textColor = Gdiplus::Color(255, 255, 255);
    }
    else if (style == 2) // ghost
    {
        fill = (hs == HoverState::None) ? Gdiplus::Color(0, 0, 0, 0)
                                        : WithAlpha(danger ? pal.danger : pal.text, 0.12f);
        textColor = danger ? pal.danger : pal.text;
    }
    else // secondary / small
    {
        fill = MixFill(IsDark() ? 0.20f : 0.10f, IsDark() ? 0.28f : 0.16f,
                       IsDark() ? 0.14f : 0.07f, hs);
        borderAlpha = 0.05f;
        textColor = danger ? pal.danger : pal.text;
    }

    Gdiplus::Color borderColor(0, 0, 0, 0);
    const Gdiplus::Color* border = nullptr;
    if (borderAlpha > 0)
    {
        borderColor = IsDark() ? Gdiplus::Color(BYTE(255 * borderAlpha), 255, 255, 255)
                               : Gdiplus::Color(BYTE(255 * borderAlpha), 0, 0, 0);
        border = &borderColor;
    }

    if (!enabled)
        fill = WithAlpha(fill, 0.45f);

    RoundRect(g, r, radius, &fill, border, 1.0f);

    const FontId font = (style == 3) ? FSmallSem : FBodySem;
    Gdiplus::Color labelColor = enabled ? textColor : WithAlpha(textColor, 0.5f);
    if (style == 0 && !enabled)
        labelColor = Gdiplus::Color(140, 255, 255, 255);
    Text(g, label, font, labelColor, r, Align::Center);
}

void Toggle(Gdiplus::Graphics& g, const RectF& r, bool on, HoverState hs)
{
    const Pal& pal = Palette();
    const float radius = r.h / 2;

    Gdiplus::Color track;
    if (on)
        track = (hs == HoverState::None) ? pal.success : WithAlpha(pal.success, 0.85f);
    else
        track = IsDark() ? Gdiplus::Color(82, 120, 120, 128)
                         : Gdiplus::Color(46, 120, 120, 128);
    if (!on && hs == HoverState::Hover)
        track = IsDark() ? Gdiplus::Color(100, 120, 120, 128)
                          : Gdiplus::Color(64, 120, 120, 128);

    RoundRect(g, r, radius, &track, nullptr, 0);

    const float pad = r.h * 0.09f;
    const float thumbD = r.h - pad * 2;
    const float tx = on ? r.right() - thumbD - pad : r.x + pad;
    const Gdiplus::Color thumb(255, 255, 255);
    const Gdiplus::Color thumbBorder(28, 0, 0, 0);
    RoundRect(g, RectF(tx, r.y + pad, thumbD, thumbD), thumbD / 2, &thumb,
              &thumbBorder, 1.0f);
}

std::vector<RectF> SegmentedLayout(const RectF& r, size_t items)
{
    std::vector<RectF> out;
    if (items == 0)
        return out;
    const float pad = 2.5f;
    const float inner = r.w - pad * 2;
    const float itemW = inner / float(items);
    for (size_t i = 0; i < items; ++i)
        out.push_back(RectF(r.x + pad + itemW * float(i), r.y + pad, itemW, r.h - pad * 2));
    return out;
}

void Segmented(Gdiplus::Graphics& g, const RectF& r, const std::vector<std::wstring>& items,
               int sel, int hoverItem, RectF* outRects)
{
    const std::vector<RectF> layout = SegmentedLayout(r, items.size());
    if (outRects)
        for (size_t i = 0; i < layout.size(); ++i)
            outRects[i] = layout[i];

    const Gdiplus::Color track = IsDark() ? Gdiplus::Color(0xFF2E2E33) : Gdiplus::Color(0xFFE9E9EC);
    RoundRect(g, r, r.h / 2 - 1, &track, nullptr, 0);

    for (size_t i = 0; i < items.size(); ++i)
    {
        const bool selected = int(i) == sel;
        if (selected)
        {
            const Gdiplus::Color thumb = IsDark() ? Gdiplus::Color(0xFF5B5B60) : Gdiplus::Color(0xFFFFFFFF);
            const Gdiplus::Color tb = IsDark() ? Gdiplus::Color(16, 255, 255, 255)
                                               : Gdiplus::Color(16, 0, 0, 0);
            RoundRect(g, layout[i], layout[i].h / 2 - 1, &thumb, &tb, 1.0f);
        }
        else if (int(i) == hoverItem)
        {
            const Gdiplus::Color hoverFill(10, 0, 0, 0);
            RoundRect(g, layout[i], layout[i].h / 2 - 1, &hoverFill, nullptr, 0);
        }
        const FontId font = r.h <= S(30) ? FSmall : FBodySem;
        Text(g, items[i], font, selected ? Palette().text : Palette().text2,
             layout[i].inset(3), Align::Center);
    }
}

void PathRow(Gdiplus::Graphics& g, const RectF& r, const std::wstring& value,
             IconKind icon, HoverState hs, bool alert)
{
    const Pal& pal = Palette();
    Gdiplus::Color border = pal.fieldBorder;
    if (hs != HoverState::None)
        border = WithAlpha(pal.accent, 0.65f);
    RoundRect(g, r, 8, &pal.fieldBg, &border, 1.0f);

    const float pad = S(10);
    Icon(g, icon, RectF(r.x + pad, r.y + (r.h - S(16)) / 2, S(16), S(16)),
         alert ? pal.danger : pal.text3, 1.4f);
    const float textX = r.x + pad + S(16) + S(8);
    Text(g, value, FMono, alert ? pal.danger : pal.text,
         RectF(textX, r.y, r.right() - S(26) - textX, r.h), Align::Left);
    // trailing chevron
    const float cy = r.y + r.h / 2;
    Gdiplus::Pen pen(pal.text3, 1.6f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&pen, Gdiplus::PointF(r.right() - S(16), cy - S(3.5f)),
               Gdiplus::PointF(r.right() - S(11), cy));
    g.DrawLine(&pen, Gdiplus::PointF(r.right() - S(11), cy),
               Gdiplus::PointF(r.right() - S(16), cy + S(3.5f)));
}

void TrafficLights(Gdiplus::Graphics& g, const RectF r[3], bool windowActive,
                   bool anyHover, int hoverIdx, int pressIdx)
{
    const Gdiplus::Color kColors[3] = {
        Gdiplus::Color(255, 255, 95, 86),   // close  #FF5F57
        Gdiplus::Color(255, 254, 188, 46),  // min    #FEBC2E
        Gdiplus::Color(255, 40, 200, 64),   // max    #28C840
    };
    const Gdiplus::Color inactive = IsDark() ? Gdiplus::Color(255, 82, 82, 88)
                                             : Gdiplus::Color(255, 200, 200, 205);

    for (int i = 0; i < 3; ++i)
    {
        Gdiplus::Color fill = windowActive ? kColors[i] : inactive;
        if (pressIdx == i && windowActive)
            fill = Gdiplus::Color(225, (fill.GetR() * 3) / 4, (fill.GetG() * 3) / 4, (fill.GetB() * 3) / 4);
        Gdiplus::SolidBrush brush(fill);
        g.FillEllipse(&brush, r[i].gp());

        if (!anyHover)
            continue;

        const float cx = r[i].x + r[i].w / 2;
        const float cy = r[i].y + r[i].h / 2;
        const float g4 = r[i].w * 0.27f;
        Gdiplus::Pen pen(Gdiplus::Color(130, 0, 0, 0), 1.4f * (r[i].w / 12.0f));
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);

        if (i == 0) // close: x
        {
            g.DrawLine(&pen, Gdiplus::PointF(cx - g4, cy - g4), Gdiplus::PointF(cx + g4, cy + g4));
            g.DrawLine(&pen, Gdiplus::PointF(cx + g4, cy - g4), Gdiplus::PointF(cx - g4, cy + g4));
        }
        else if (i == 1) // min: -
        {
            g.DrawLine(&pen, Gdiplus::PointF(cx - g4, cy), Gdiplus::PointF(cx + g4, cy));
        }
        else // max: two triangles
        {
            Gdiplus::SolidBrush glyph(Gdiplus::Color(130, 0, 0, 0));
            const float t = g4 * 1.05f;
            Gdiplus::PointF tl[] = {
                Gdiplus::PointF(cx - t, cy - 0.55f),
                Gdiplus::PointF(cx - t, cy - t),
                Gdiplus::PointF(cx - 0.55f, cy - t),
            };
            g.FillPolygon(&glyph, tl, 3);
            Gdiplus::PointF br[] = {
                Gdiplus::PointF(cx + t, cy + 0.55f),
                Gdiplus::PointF(cx + t, cy + t),
                Gdiplus::PointF(cx + 0.55f, cy + t),
            };
            g.FillPolygon(&glyph, br, 3);
        }
    }
}

void NavItem(Gdiplus::Graphics& g, const RectF& r, IconKind icon, const wchar_t* label,
             bool selected, HoverState hs)
{
    const Pal& pal = Palette();
    if (selected)
        RoundRect(g, r, 8, &pal.accent, nullptr, 0);
    else if (hs != HoverState::None)
        RoundRect(g, r, 8, &pal.hoverFill, nullptr, 0);

    const Gdiplus::Color fg = selected ? Gdiplus::Color(255, 255, 255) : pal.text;
    Icon(g, icon, RectF(r.x + S(11), r.y + (r.h - S(19)) / 2, S(19), S(19)), fg, 1.6f);
    Text(g, label, FBodySem, fg, RectF(r.x + S(40), r.y, r.right() - S(8) - r.x - S(40), r.h),
         Align::Left);
}

void StepBullet(Gdiplus::Graphics& g, const RectF& r, int state, int index, int spinnerPhase)
{
    const Pal& pal = Palette();
    const float d = r.w;

    if (state == 0) // pending
    {
        Gdiplus::Pen pen(pal.text3, 1.4f);
        g.DrawEllipse(&pen, r.gp());
        Text(g, std::to_wstring(index + 1), FSmallSem, pal.text3, r, Align::Center);
    }
    else if (state == 1) // active: spinner arc
    {
        Gdiplus::Pen pen(pal.accent, 2.2f);
        const float a = float(spinnerPhase % 360);
        g.DrawArc(&pen, r.inset(1).gp(), a, 260);
    }
    else if (state == 2) // done
    {
        Gdiplus::SolidBrush brush(pal.success);
        g.FillEllipse(&brush, r.gp());
        const float cx = r.x + d / 2, cy = r.y + d / 2;
        const float t = d * 0.22f;
        Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255), 1.8f * (d / 26.0f));
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&pen, Gdiplus::PointF(cx - t * 1.1f, cy), Gdiplus::PointF(cx - t * 0.2f, cy + t));
        g.DrawLine(&pen, Gdiplus::PointF(cx - t * 0.2f, cy + t), Gdiplus::PointF(cx + t * 1.2f, cy - t));
    }
    else // error
    {
        Gdiplus::SolidBrush brush(pal.danger);
        g.FillEllipse(&brush, r.gp());
        const float cx = r.x + d / 2, cy = r.y + d / 2;
        const float t = d * 0.2f;
        Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255), 1.8f * (d / 26.0f));
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&pen, Gdiplus::PointF(cx - t, cy - t), Gdiplus::PointF(cx + t, cy + t));
        g.DrawLine(&pen, Gdiplus::PointF(cx + t, cy - t), Gdiplus::PointF(cx - t, cy + t));
    }
}

void StatusDot(Gdiplus::Graphics& g, const RectF& r, const Gdiplus::Color& c)
{
    Gdiplus::SolidBrush halo(WithAlpha(c, 0.28f));
    g.FillEllipse(&halo, r.inset(-r.w * 0.18f).gp());
    Gdiplus::SolidBrush brush(c);
    g.FillEllipse(&brush, r.gp());
}

} // namespace ui
} // namespace micp
