#include "app/App.h"
#include "app/Lang.h"

#include "core/AppPaths.h"
#include "core/EmbeddedDll.h"

// AlphaBlend for the page-transition crossfade: declared in wingdi.h (pulled
// in through the windows.h chain); msimg32.lib provides the import.

namespace micp {
namespace {

// ---------------------------------------------------------- back buffer

struct BackBuffer
{
    HDC dc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ old = nullptr;
    void* bits = nullptr;
    int w = 0, h = 0;

    ~BackBuffer()
    {
        if (dc)
        {
            if (old)
                SelectObject(dc, old);
            DeleteDC(dc);
        }
        if (dib)
            DeleteObject(dib);
    }

    void Ensure(int W, int H)
    {
        if (W <= 0)
            W = 1;
        if (H <= 0)
            H = 1;
        if (W == w && H == h && dc)
            return;
        if (dc)
        {
            if (old)
                SelectObject(dc, old);
            DeleteDC(dc);
            dc = nullptr;
            old = nullptr;
        }
        if (dib)
        {
            DeleteObject(dib);
            dib = nullptr;
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = W;
        bi.bmiHeader.biHeight = -H; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        dc = CreateCompatibleDC(nullptr);
        old = SelectObject(dc, dib);
        w = W;
        h = H;
    }
};

BackBuffer s_buf;      // the window's back buffer (last painted frame)
BackBuffer s_snap;     // outgoing page snapshot for the crossfade
BackBuffer s_newPage;  // incoming page render target during the crossfade

Gdiplus::Color ApplyAlpha(const Gdiplus::Color& c, float a)
{
    return Gdiplus::Color(BYTE(255 * a), c.GetR(), c.GetG(), c.GetB());
}

// Fades a color toward transparency while keeping its designed base alpha
// (toasts are translucent pills, not opaque ones).
Gdiplus::Color FadeKeep(const Gdiplus::Color& c, float k)
{
    return Gdiplus::Color(BYTE(c.GetA() * k), c.GetR(), c.GetG(), c.GetB());
}

void Card(Gdiplus::Graphics& g, const RectF& r, const wchar_t* title = nullptr)
{
    const Pal& pal = Palette();
    const App& app = g_app;
    Gdiplus::Color fill = pal.cardBg;
    if (app.m_backdropActive)
        fill = ApplyAlpha(pal.cardBg, pal.micaCardAlpha);
    ui::RoundRect(g, r, 12.0f, &fill, &pal.cardBorder, 1.0f);
    if (title)
        ui::Text(g, title, FHead, pal.text,
                 RectF(r.x + float(S(18)), r.y + float(S(16)), r.w - float(S(36)),
                       float(LineH(FHead))));
}

float HoverTOf(int id) { return g_app.HoverT(id); }
bool PressedOf(int id) { return g_app.m_pressId == id && g_app.m_hoverId == id; }

std::wstring FileNameOf(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return path;
    return path.substr(slash + 1);
}

// macOS overlay scrollbar thumb color: clearly visible, no track line.
Gdiplus::Color SbThumbColor(float alpha01)
{
    if (IsDark())
        return Gdiplus::Color(BYTE(255 * alpha01), 235, 235, 245);
    return Gdiplus::Color(BYTE(255 * alpha01), 60, 60, 67);
}

} // namespace

// ------------------------------------------------------------------ pages

static void PaintOverview(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    ui::Text(g, lang::Tr(lang::Str::PageOverview), FTitle, pal.text, // 总览
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    // --- warning card -----------------------------------------------------
    if (L.ovWarn.w > 0)
    {
        Card(g, L.ovWarn);
        ui::Icon(g, IconKind::Warn, RectF(L.ovWarn.x + float(S(18)), L.ovWarn.y + float(S(26)),
                                          float(S(22)), float(S(22))), pal.warning, 1.6f);
        ui::Text(g, lang::Tr(lang::Str::OvDllMissing), FBodySem, pal.danger,
                 RectF(L.ovWarn.x + float(S(52)), L.ovWarn.y + float(S(20)),
                       L.ovWarn.w - float(S(52)) - float(S(140)), float(LineH(FBodySem))));
        ui::Text(g, lang::Tr(lang::Str::OvDllMissingHint),
                 FSmall, pal.text2,
                 RectF(L.ovWarn.x + float(S(52)), L.ovWarn.y + float(S(24)) + float(LineH(FBodySem)),
                       L.ovWarn.w - float(S(52)) - float(S(140)), float(LineH(FSmall))));
    }

    // --- status card ------------------------------------------------------
    if (L.ovStatus.w > 0)
    {
        Card(g, L.ovStatus);

        const bool running = app.m_seq.IsRunning();
        const bool listening = app.m_snap.observing && app.m_snap.listening;
        const bool dead = app.m_snap.observing && !app.m_snap.alive;
        const bool winFound = app.m_snap.windowFound;

        const wchar_t* headline;
        Gdiplus::Color dot;
        if (running)
        {
            headline = lang::Tr(lang::Str::OvStatusInjecting); // 注入进行中…
            dot = pal.accent;
        }
        else if (listening)
        {
            headline = lang::Tr(lang::Str::OvStatusRunning); // 代理运行中
            dot = pal.success;
        }
        else if (dead)
        {
            headline = lang::Tr(lang::Str::OvStatusDead); // 目标客户端已退出
            dot = pal.text3;
        }
        else if (winFound)
        {
            headline = lang::Tr(lang::Str::OvStatusFound); // 已找到目标窗口
            dot = pal.accent;
        }
        else
        {
            headline = lang::Tr(lang::Str::OvStatusWaiting); // 等待目标窗口
            dot = pal.text3;
        }

        const float leftX = L.ovStatus.x + float(S(20));
        const float headY = L.ovStatus.y + float(S(28));
        ui::StatusDot(g, RectF(leftX, headY + float(LineH(FTitle) - S(16)) / 2, float(S(16)),
                               float(S(16))), dot);
        ui::Text(g, headline, FTitle, pal.text,
                 RectF(leftX + float(S(30)), headY, L.ovStatus.w - float(S(210)),
                       float(LineH(FTitle))));
        if (running)
        {
            // small spinner next to the headline
            Gdiplus::Pen pen(pal.accent, 2.2f);
            g.DrawArc(&pen, Gdiplus::RectF(leftX + float(S(30)) + float(S(190)), headY,
                                           float(S(22)), float(S(22))),
                      float(app.m_spin % 360), 260);
        }

        std::wstring sub1, sub2;
        if (running)
            sub1 = lang::Tr(lang::Str::OvSubInjecting); // 正在执行注入流程，可在「注入」页查看进度
        else if (listening)
            sub1 = FormatStr(lang::Tr(lang::Str::OvSubRunning), // PID … · … 个 B 客户端已连接 · 监听 …
                             app.m_snap.pid, app.m_snap.bConnections, app.m_set.port);
        else if (dead)
            sub1 = lang::Tr(lang::Str::OvSubDead); // 注入过的客户端进程已退出，代理随之停止
        else if (winFound)
            sub1 = FormatStr(lang::Tr(lang::Str::OvSubFound), app.m_snap.windowTitle.c_str(),
                             app.m_snap.windowPid);
        else
            sub1 = lang::Tr(lang::Str::OvSubIdle); // 启动 A 客户端并进入目标服务器后，即可一键注入
        if (app.m_snap.observing && !app.m_snap.windowTitle.empty())
            sub2 = app.m_snap.windowTitle;

        const float subY = headY + float(LineH(FTitle)) + float(S(10));
        ui::Text(g, sub1, FBody, pal.text2,
                 RectF(leftX, subY, L.ovStatus.w - float(S(180)), float(LineH(FBody))));
        if (!sub2.empty())
            ui::Text(g, sub2, FSmall, pal.text3,
                     RectF(leftX, subY + float(LineH(FBody)) + float(S(4)),
                           L.ovStatus.w - float(S(180)), float(LineH(FSmall))));

        // session counter at the bottom-left
        ui::Text(g, FormatStr(lang::Tr(lang::Str::OvSessionOkCount), app.m_okCount), // 本次会话成功注入 N 次
                 FSmall, pal.text3,
                 RectF(leftX, L.ovStatus.bottom() - float(S(26)), L.ovStatus.w - float(S(180)),
                       float(LineH(FSmall))));
    }

    // --- config summary ----------------------------------------------------
    if (L.ovInfo.w > 0)
    {
        Card(g, L.ovInfo, lang::Tr(lang::Str::OvCurrentConfig)); // 当前配置
        const float rowY0 = L.ovInfo.y + float(S(24)) + float(LineH(FHead)) + float(S(14));
        const float labelX = L.ovInfo.x + float(S(18));
        const float labelW = float(S(110));

        struct Row
        {
            const wchar_t* label;
            std::wstring value;
            bool ok;
            bool hasState;
            Gdiplus::Color stateColor;
            const wchar_t* stateText;
        };
        Row rows[4] = {};
        rows[0] = { lang::Tr(lang::Str::OvProxyDll), std::wstring(L"Proxy.dll"), app.m_dllFound, true,
                    app.m_dllFound ? pal.success : pal.danger,
                    app.m_dllFound ? lang::Tr(lang::Str::OvStateFound) : lang::Tr(lang::Str::OvStateNotFound) };
        rows[1] = { lang::Tr(lang::Str::OvKeywordLabel), app.m_set.keyword, false, false, pal.text, L"" };
        rows[2] = { lang::Tr(lang::Str::OvPortLabel), std::to_wstring(app.m_set.port) + lang::Tr(lang::Str::OvPortNote), false, false, pal.text, L"" };
        rows[3] = { lang::Tr(lang::Str::LogSourceCard),
                     app.m_logSource.empty() ? lang::Tr(lang::Str::OvAutoDetecting) : FileNameOf(app.m_logSource),
                     false, false, pal.text, L"" };

        for (int i = 0; i < 4; ++i)
        {
            const float y = rowY0 + i * float(S(30));
            ui::Text(g, rows[i].label, FSmall, pal.text2,
                     RectF(labelX, y, labelW, float(S(30))));
            // Values align to the card's right padding; only state rows keep
            // a reserve for the trailing status text.
            float reserve = rows[i].hasState ? float(S(150)) : 0.0f;
            RectF vr(labelX + labelW, y,
                     L.ovInfo.w - labelW - float(S(36)) - reserve, float(S(30)));
            const bool emptyValue = TrimW(rows[i].value).empty();
            if (emptyValue)
                ui::Text(g, lang::Tr(lang::Str::OvNotSet), FSmall, pal.text3, vr, ui::Align::Right); // 未设置
            else
                ui::Text(g, rows[i].value, FBody, emptyValue ? pal.text3 : pal.text, vr,
                         ui::Align::Right);
            if (rows[i].hasState)
                ui::Text(g, rows[i].stateText, FSmall, rows[i].stateColor,
                         RectF(labelX + labelW, y, L.ovInfo.w - labelW - float(S(36)), float(S(30))),
                         ui::Align::Right);
            if (i < 3)
                ui::HLine(g, labelX, y + float(S(30)) - 0.5f,
                          L.ovInfo.w - float(S(36)), pal.separator);
        }
    }

    // --- connect hint ------------------------------------------------------
    if (L.ovHint.w > 0)
    {
        Card(g, L.ovHint, lang::Tr(lang::Str::OvHowConnect)); // B 客户端如何连接
        const float x = L.ovHint.x + float(S(18));
        const float y0 = L.ovHint.y + float(S(24)) + float(LineH(FHead)) + float(S(14));
        const float w = L.ovHint.w - float(S(36));

        ui::Icon(g, IconKind::Plug, RectF(x, y0 + float(S(2)), float(S(18)), float(S(18))),
                 pal.accent, 1.5f);
        ui::Text(g, FormatStr(lang::Tr(lang::Str::OvSamePc), // 同一台电脑：B 客户端「直接连接」…
                     app.m_set.port), FBody, pal.text,
                 RectF(x + float(S(28)), y0, w - float(S(28)), float(LineH(FBody))));

        std::vector<std::wstring> ips = AppPaths::LanIpAddresses();
        std::wstring lan;
        for (size_t i = 0; i < ips.size() && i < 3; ++i)
            lan += (i ? lang::Tr(lang::Str::LanIpSep) : L"") + ips[i] + L":" + std::to_wstring(app.m_set.port);
        const std::wstring line2 = lan.empty()
            ? lang::Tr(lang::Str::OvLanNoAddr)
            : lang::Tr(lang::Str::OvLanPrefix) + lan;

        ui::Text(g, line2, FBody, pal.text,
                 RectF(x + float(S(28)), y0 + float(LineH(FBody)) + float(S(6)),
                       w - float(S(28)), float(LineH(FBody))));
        ui::Text(g, lang::Tr(lang::Str::OvRolesNote), // A 客户端为被注入的一端；…
                 FSmall, pal.text3,
                 RectF(x, y0 + 2 * (float(LineH(FBody)) + float(S(6))) + float(S(4)), w,
                       float(LineH(FSmall))));
    }
}

static void PaintInject(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    ui::Text(g, lang::Tr(lang::Str::PageInject), FTitle, pal.text, // 注入
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    // --- config card -------------------------------------------------------
    if (L.ijConfig.w > 0)
    {
        Card(g, L.ijConfig, lang::Tr(lang::Str::InjectConfig));
        const float labelX = L.ijConfig.x + float(S(18));
        const float labelW = float(S(96));

        auto rowLabel = [&](const RectF& row, const wchar_t* label)
        {
            ui::Text(g, label, FSmall, pal.text2, RectF(labelX, row.y, labelW, row.h));
        };
        // Hints live under their fields: full card width, wrapping.
        auto rowHint = [&](const RectF& r, const std::wstring& text)
        {
            if (r.w > 0)
                ui::Text(g, text, FSmall, pal.text3, r, ui::Align::Left, true);
        };

        rowLabel(L.ijRowKw, lang::Tr(lang::Str::OvKeywordLabel));
        rowLabel(L.ijRowPort, lang::Tr(lang::Str::OvPortLabel));
        rowHint(L.ijHintKw, std::wstring(lang::Tr(lang::Str::IjKwHint))); // 匹配窗口标题包含此关键字的 Java 进程
        rowHint(L.ijHintPort, std::wstring(lang::Tr(lang::Str::IjPortHint))); // B 客户端「直接连接」访问此端口
    }

    // --- steps card --------------------------------------------------------
    if (L.ijSteps.w > 0)
    {
        Card(g, L.ijSteps, lang::Tr(lang::Str::InjectProgress));
        // Not static: the pointers follow the active language table.
        const wchar_t* kStepNames[4] = {
            lang::Tr(lang::Str::StepCheckDll),   // 检查代理 DLL
            lang::Tr(lang::Str::StepFindWindow), // 定位目标窗口
            lang::Tr(lang::Str::StepInjectDll),              // 注入 DLL
            lang::Tr(lang::Str::StepWaitProxy), // 等待代理监听
        };
        const Pal& p = pal;
        for (int i = 0; i < 4; ++i)
        {
            const RectF& b = L.ijStepBullets[i];
            const RectF& t = L.ijStepTexts[i];
            ui::StepBullet(g, b, app.m_stepState[i], i, app.m_spin);
            if (i < 3)
            {
                const RectF& nb = L.ijStepBullets[i + 1];
                Gdiplus::Pen pen(p.separator, 2.0f);
                g.DrawLine(&pen, Gdiplus::PointF(b.x + b.w / 2, b.bottom() + float(S(4))),
                           Gdiplus::PointF(nb.x + nb.w / 2, nb.y - float(S(4))));
            }
            // The title line is centered on the bullet's middle; the detail
            // line sits right below it (they used to float above the dots).
            const float yc = b.y + b.w / 2;
            const Gdiplus::Color nameColor = app.m_stepState[i] == 3
                ? p.danger
                : (app.m_stepState[i] == 0 ? p.text2 : p.text);
            ui::Text(g, kStepNames[i], FBodySem, nameColor,
                     RectF(t.x, yc - float(LineH(FBodySem)) / 2, t.w, float(LineH(FBodySem))));
            if (!app.m_stepDetail[i].empty())
                ui::Text(g, app.m_stepDetail[i], FSmall, p.text2,
                         RectF(t.x, yc + float(LineH(FBodySem)) / 2 + float(S(2)), t.w,
                               float(LineH(FSmall))));
        }

        // WaitPort progress bar
        if (app.m_stepState[3] == 1 && app.m_stepProgress > 0)
        {
            ui::RoundRect(g, L.ijBar, 3, &p.hoverFill, nullptr, 0);
            const float w = L.ijBar.w * app.m_stepProgress;
            if (w > 1)
                ui::RoundRect(g, RectF(L.ijBar.x, L.ijBar.y, w, L.ijBar.h), 3, &p.accent,
                              nullptr, 0);
        }

        // banner
        if (app.m_bannerKind != 0)
        {
            const bool ok = app.m_bannerKind == 1;
            const bool cancelled = app.m_bannerKind == 3;
            const Gdiplus::Color tint = ok ? p.success : cancelled ? p.text3 : p.danger;
            const Gdiplus::Color bannerFill = ApplyAlpha(tint, 0.12f);
            const Gdiplus::Color bannerBorder = ApplyAlpha(tint, 0.30f);
            ui::RoundRect(g, L.ijBanner, 8, &bannerFill, &bannerBorder, 1.0f);
            ui::Icon(g, ok ? IconKind::Check : cancelled ? IconKind::Close : IconKind::Warn,
                     RectF(L.ijBanner.x + float(S(14)), L.ijBanner.y + (L.ijBanner.h - float(S(18))) / 2,
                           float(S(18)), float(S(18))), tint, 1.7f);
            ui::Text(g, app.m_bannerText, FBodySem, tint,
                     RectF(L.ijBanner.x + float(S(42)), L.ijBanner.y, L.ijBanner.w - float(S(56)),
                           L.ijBanner.h), ui::Align::Left, true);
        }
    }
}

static void PaintLogs(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    // toolbar
    for (const SegDef& seg : L.segs)
    {
        if (seg.id != ID_SEG_LOGFILTER)
            continue;
        int hoverItem = -1;
        float hoverFade = 0;
        if (app.m_hoverId / 100 == seg.id && app.m_hoverId % 100 >= 1)
        {
            hoverItem = app.m_hoverId % 100 - 1;
            hoverFade = app.HoverT(app.m_hoverId);
        }
        ui::Segmented(g, seg.r, seg.items, app.SegT(seg.id, seg.sel), hoverItem, hoverFade,
                      nullptr);
    }
    ui::Text(g, lang::Tr(lang::Str::AutoScroll), FSmall, pal.text2, // 自动滚动
             RectF(L.lgTglLabel.x - float(S(4)), L.lgTgl.y - float(S(3)), L.lgTglLabel.w,
                   L.lgTgl.h + float(S(6))), ui::Align::Right);
    for (const TglDef& t : L.tgls)
        if (t.id == ID_TGL_FOLLOW)
            ui::Toggle(g, t.r, app.KnobT(t.id), app.HoverT(t.id));

    // card
    const Gdiplus::Color cardFill = app.m_backdropActive
        ? ApplyAlpha(pal.cardBg, pal.micaCardAlpha) : pal.cardBg;
    ui::RoundRect(g, L.lgCard, 12.0f, &cardFill, &pal.cardBorder, 1.0f);

    // head: source + counts
    const std::wstring src = app.m_logSource.empty()
        ? lang::Tr(lang::Str::AutoDetectingDots) // 自动探测中…
        : app.m_logSource;
    ui::Text(g, src, FMonoSmall, pal.text3,
             RectF(L.lgHead.x + float(S(14)), L.lgHead.y, L.lgHead.w - float(S(28)) - float(S(160)),
                   L.lgHead.h));
    int shown = 0, total = int(app.m_lines.size());
    for (const auto& e : app.m_lines)
        if (app.PassesFilter(e))
            ++shown;
    const std::wstring counts = app.m_filter == LfAll
        ? FormatStr(lang::Tr(lang::Str::LinesTotal), total) // 共 N 行
        : FormatStr(lang::Tr(lang::Str::LinesFiltered), shown, total);
    ui::Text(g, counts, FSmall, pal.text3,
             RectF(L.lgHead.right() - float(S(14)) - float(S(150)), L.lgHead.y, float(S(150)),
                   L.lgHead.h), ui::Align::Right);
    ui::HLine(g, L.lgHead.x + float(S(14)), L.lgHead.bottom() - 0.5f,
              L.lgHead.w - float(S(28)), pal.separator);

    // rows
    const float rowH = float(S(24));
    const float timeW = float(S(56));
    const float levelW = float(S(48));
    const float msgX = L.lgBody.x + timeW + levelW + float(S(10));
    const float msgW = L.lgBody.right() - float(S(10)) - msgX;

    if (shown == 0)
    {
        ui::Icon(g, IconKind::Terminal,
                 L.lgBody.centered(float(S(34)), float(S(34))), pal.text3, 1.5f);
        ui::Text(g, lang::Tr(lang::Str::LogsEmpty), FBody, pal.text2, // 暂无日志，等待代理输出…
                 RectF(L.lgBody.x, L.lgBody.y + L.lgBody.h / 2 + float(S(22)), L.lgBody.w,
                       float(LineH(FBody))), ui::Align::Center);
        ui::Text(g, src, FSmall, pal.text3,
                 RectF(L.lgBody.x, L.lgBody.y + L.lgBody.h / 2 + float(S(22)) + float(LineH(FBody)),
                       L.lgBody.w, float(LineH(FSmall))), ui::Align::Center);
    }
    else
    {
        const float maxScroll = shown * rowH > L.lgBody.h ? shown * rowH - L.lgBody.h : 0;
        if (app.m_logScroll > maxScroll)
            app.m_logScroll = maxScroll;
        const int first = int(app.m_logScroll / rowH);
        const int visible = int(L.lgBody.h / rowH) + 2;
        int idx = -1;
        int drawn = 0;
        for (const auto& e : app.m_lines)
        {
            if (!app.PassesFilter(e))
                continue;
            ++idx;
            if (idx < first)
                continue;
            if (drawn > visible)
                break;
            const float y = L.lgBody.y + idx * rowH - app.m_logScroll;
            if (y + rowH < L.lgBody.y)
                continue;
            if (!e.time.empty())
                ui::Text(g, e.time, FMonoSmall, pal.lvTime,
                         RectF(L.lgBody.x, y, timeW, rowH));
            static const wchar_t* kLevelName[4] = { L"INFO", L"WARN", L"ERROR", L"DEBUG" };
            const Gdiplus::Color levelColor = e.level == 1 ? pal.lvWarn
                                               : e.level == 2 ? pal.lvError
                                               : e.level == 3 ? pal.lvDebug
                                                              : pal.lvInfo;
            ui::Text(g, kLevelName[e.level], FMonoSmall, levelColor,
                     RectF(L.lgBody.x + timeW + float(S(8)), y, levelW, rowH));
            ui::Text(g, e.text, FMono, pal.lvText, RectF(msgX, y, msgW, rowH));
            ++drawn;
        }
        // overlay scrollbar (auto-hiding, no track)
        if (maxScroll > 0 && L.lgBody.h > 40)
        {
            const float reveal = app.m_lgSbAnim.v;
            if (reveal > 0.01f)
            {
                const bool active = app.m_thumbDragging || app.m_hoverId == ID_LOG_SB;
                const Gdiplus::Color thumb = SbThumbColor((active ? 0.55f : 0.38f) * reveal);
                const float viewH = L.lgBody.h;
                const float contentH = shown * rowH;
                float thumbH = viewH * viewH / contentH;
                if (thumbH < float(S(28)))
                    thumbH = float(S(28));
                const float track = viewH - thumbH;
                const float thumbY = L.lgBody.y + (app.m_logScroll / maxScroll) * track;
                ui::RoundRect(g, RectF(L.lgScroll.x, thumbY, L.lgScroll.w, thumbH), 4.0f,
                              &thumb, nullptr, 0);
            }
        }
    }
}

static void PaintSettings(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    ui::Text(g, lang::Tr(lang::Str::PageSettings), FTitle, pal.text, // 设置
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    if (L.stAppearance.w > 0)
    {
        Card(g, L.stAppearance, lang::Tr(lang::Str::Appearance)); // 外观
        ui::Text(g, lang::Tr(lang::Str::ThemeLabel), FBody, pal.text, L.stSegThemeLabel); // 主题
        ui::Text(g, lang::Tr(lang::Str::BackdropLabel), FBody, pal.text, L.stSegBackdropLabel); // 窗口材质
        // Language picker field: rounded box, current endonym, chevron.
        ui::Text(g, lang::Tr(lang::Str::LanguageLabel), FBody, pal.text, L.stLangLabel); // 语言
        {
            const float hover = app.HoverT(ID_LANG_ROW);
            const Gdiplus::Color border = ui::Mix(pal.fieldBorder, pal.accent, hover * 0.55f);
            ui::RoundRect(g, L.stLangValue, 8.0f, &pal.fieldBg, &border, 1.0f);
            ui::Text(g, lang::DisplayName(lang::Current()), FBody, pal.text,
                     RectF(L.stLangValue.x + float(S(12)), L.stLangValue.y,
                           L.stLangValue.w - float(S(36)), L.stLangValue.h));
            const float cx = L.stLangValue.right() - float(S(14));
            const float cy = L.stLangValue.y + L.stLangValue.h / 2 + float(app.m_langOpen ? S(1) : -S(1));
            const float s = float(S(4));
            Gdiplus::Pen pen(pal.text2, 1.6f);
            if (app.m_langOpen) // chevron points up while the popup is open
            {
                g.DrawLine(&pen, Gdiplus::PointF(cx - s, cy + s / 2),
                          Gdiplus::PointF(cx, cy - s / 2));
                g.DrawLine(&pen, Gdiplus::PointF(cx, cy - s / 2),
                          Gdiplus::PointF(cx + s, cy + s / 2));
            }
            else
            {
                g.DrawLine(&pen, Gdiplus::PointF(cx - s, cy - s / 2),
                          Gdiplus::PointF(cx, cy + s / 2));
                g.DrawLine(&pen, Gdiplus::PointF(cx, cy + s / 2),
                          Gdiplus::PointF(cx + s, cy - s / 2));
            }
        }
        const std::wstring note = lang::Tr(lang::Str::BackdropNote); // 云母 / 亚克力为系统材质…
        ui::Text(g, note, FSmall, pal.text3,
                 RectF(L.stAppearance.x + float(S(18)), L.stAppearance.bottom() - float(S(30)),
                       L.stAppearance.w - float(S(36)), float(S(28))),
                 ui::Align::Left, true);
    }

    if (L.stBehavior.w > 0)
    {
        Card(g, L.stBehavior, lang::Tr(lang::Str::Behavior)); // 行为
        // Not static: the pointers follow the active language table.
        const wchar_t* kLabels[3] = {
            lang::Tr(lang::Str::AutoSwitchLabel), // 注入完成后自动跳转日志
            lang::Tr(lang::Str::ToastDoneLabel), // 注入完成时显示浮窗提示
            lang::Tr(lang::Str::FollowLogLabel),           // 日志页自动跟随滚动
        };
        for (int i = 0; i < 3; ++i)
        {
            ui::Text(g, kLabels[i], FBody, pal.text, L.stTglLabel[i]);
            if (i < 2)
                ui::HLine(g, L.stTglLabel[i].x, L.stTglLabel[i].y + float(S(44)),
                          L.stBehavior.w - float(S(36)), pal.separator);
        }
    }

    if (L.stLog.w > 0)
    {
        Card(g, L.stLog, lang::Tr(lang::Str::LogSourceCard)); // 日志来源
        // wrapping hint + settings-file line, stacked without overlap
        ui::Text(g, lang::Tr(lang::Str::LogSourceHint), // 默认自动探测…；点击上方路径可手动指定文件
                 FSmall, pal.text3, L.stIniLabel, ui::Align::Left, true);
        ui::Text(g, lang::Tr(lang::Str::SettingsFileLabel) + AppPaths::SettingsIni(), // 设置文件：…
                 FSmall, pal.text3,
                 RectF(L.stIniLabel.x, L.stIniLabel.bottom() + float(S(4)),
                       L.stLog.w - float(S(36)), float(LineH(FSmall))));
    }

    if (L.stResetCard.w > 0)
    {
        Card(g, L.stResetCard);
        ui::Text(g, lang::Tr(lang::Str::ResetHint), // 恢复全部默认值…；不影响窗口位置
                 FSmall, pal.text2,
                 RectF(L.stResetCard.x + float(S(170)), L.stResetCard.y,
                       L.stResetCard.w - float(S(190)), L.stResetCard.h),
                 ui::Align::Left, true);
    }
}

static void PaintAbout(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();

    // app glyph: rounded square with a vertical accent gradient + white bolt
    {
        const RectF& r = L.abIcon;
        Gdiplus::GraphicsPath path;
        const float d = 24.0f * 2; // diameter for arcs
        path.AddArc(r.x, r.y, d, d, 180, 90);
        path.AddArc(r.right() - d, r.y, d, d, 270, 90);
        path.AddArc(r.right() - d, r.bottom() - d, d, d, 0, 90);
        path.AddArc(r.x, r.bottom() - d, d, d, 90, 90);
        path.CloseFigure();
        Gdiplus::LinearGradientBrush brush(r.gp(), pal.accentHover, pal.accentPress,
                                           Gdiplus::LinearGradientModeVertical);
        g.FillPath(&brush, &path);
        Gdiplus::Pen border(Gdiplus::Color(40, 255, 255, 255), 1.2f);
        g.DrawPath(&border, &path);
        ui::Icon(g, IconKind::Zap, r.centered(r.w * 0.62f, r.w * 0.62f),
                 Gdiplus::Color(255, 255, 255), 1.7f);
    }

    ui::Text(g, kAppName, FTitle, pal.text, L.abName, ui::Align::Center);

    // version pill
    {
        const std::wstring v = FormatStr(lang::Tr(lang::Str::AbVersionPill), // v… · 原生 Win32 单文件
                                         AppVersion());
        const float vw = ui::MeasureText(v, FSmall);
        RectF pill = L.abVersion;
        pill.w = vw + float(S(28));
        pill.x = L.pageBox.x + L.pageBox.w / 2 - pill.w / 2;
        pill.h = float(S(24));
        ui::RoundRect(g, pill, pill.h / 2, &pal.hoverFill, nullptr, 0);
        ui::Text(g, v, FSmall, pal.text2, pill, ui::Align::Center);
    }

    ui::Text(g, lang::Tr(lang::Str::AbTagline), // Minecraft 1.20.1 客户端反向代理 · 注入与管理工具
             FBody, pal.text2, L.abTag, ui::Align::Center);

    const float fy = L.abFoot.y + float(S(6));
    ui::HLine(g, L.pageBox.x + float(S(60)), fy, L.pageBox.w - float(S(120)), pal.separator);
    struct AboutLine
    {
        const wchar_t* text;
        bool dim;
    };
    const AboutLine lines[] = {
        { lang::Tr(lang::Str::AbLine1), false }, // 原生 Win32 / GDI+ · 无任何运行时依赖 · 单文件 exe
        { lang::Tr(lang::Str::AbLine2), false }, // 代理核心 Proxy.dll 已内置
        { lang::Tr(lang::Str::AbLine3), true }, // 仅供学习交流使用，请遵守…
    };
    float ly = fy + float(S(16));
    for (const AboutLine& line : lines)
    {
        ui::Text(g, line.text, FSmall, line.dim ? pal.text3 : pal.text2,
                 RectF(L.pageBox.x, ly, L.pageBox.w, float(LineH(FSmall))), ui::Align::Center);
        ly += float(LineH(FSmall)) + float(S(6));
    }
}

// -------------------------------------------------------------- page body

// Renders the current page (body + registered controls + EDIT field frames)
// into an arbitrary GDI+ context. The clip/scroll transform is applied here
// so the transition buffer and the main back buffer share one code path.
static void PaintPageBody(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    const Gdiplus::GraphicsState pageState = g.Save();
    Gdiplus::RectF viewport = L.pageBox.gp();
    viewport.Width += float(S(6)); // let card shadows breathe at the right edge
    g.SetClip(viewport);
    if (app.m_pageScroll > 0.01f)
        g.TranslateTransform(0.0f, -app.m_pageScroll);

    switch (app.m_page)
    {
    case PgOverview: PaintOverview(g, L); break;
    case PgInject: PaintInject(g, L); break;
    case PgLogs: PaintLogs(g, L); break;
    case PgSettings: PaintSettings(g, L); break;
    case PgAbout: PaintAbout(g, L); break;
    default: break;
    }

    // registered controls live on top of the page bodies
    for (const BtnDef& b : L.btns)
        ui::Button(g, b.r, b.label, b.style, HoverTOf(b.id), PressedOf(b.id), b.enabled,
                   b.danger);
    for (const RowDef& row : L.rows)
        ui::PathRow(g, row.r, row.text, row.icon, HoverTOf(row.id), row.alert);
    for (const SegDef& seg : L.segs)
    {
        if (seg.id == ID_SEG_LOGFILTER)
            continue; // painted with the logs page
        int hoverItem = -1;
        float hoverFade = 0;
        if (app.m_hoverId / 100 == seg.id && app.m_hoverId % 100 >= 1)
        {
            hoverItem = app.m_hoverId % 100 - 1;
            hoverFade = app.HoverT(app.m_hoverId);
        }
        ui::Segmented(g, seg.r, seg.items, app.SegT(seg.id, seg.sel), hoverItem, hoverFade,
                      nullptr);
    }
    for (const TglDef& t : L.tgls)
    {
        if (t.id == ID_TGL_FOLLOW && app.m_page == PgLogs)
            continue; // painted with the logs page
        ui::Toggle(g, t.r, app.KnobT(t.id), app.HoverT(t.id));
    }

    // EDIT field frames (borderless EDIT children render on top of these)
    if (app.m_page == PgInject)
    {
        auto fieldFrame = [&](const RectF& r, int idx)
        {
            const bool invalid = (idx == 1) && app.m_portInvalid;
            const Gdiplus::Color base = invalid ? pal.danger : pal.fieldBorder;
            // The focus ring fades in and out with the animation clock.
            const float f = app.m_editFocusAnim[idx].v;
            const Gdiplus::Color border = ui::Mix(base, pal.accent, f);
            // Same field surface as ui::PathRow (the DLL / log-source rows),
            // so every input in the UI reads as one family.
            const Gdiplus::Color fill = pal.fieldBg;
            ui::RoundRect(g, r, 8.0f, &fill, &border, 1.0f + 0.5f * f);
        };
        fieldFrame(L.ijEditKw, 0);
        fieldFrame(L.ijEditPort, 1);
    }

    g.Restore(pageState);
}

// ------------------------------------------------------------------ chrome

void App::SnapshotPageContent()
{
    // Grab the outgoing view from the back buffer for the crossfade. Must be
    // called before m_page changes (the buffer still shows the old page).
    // On failure the snapshot is marked unusable; the transition then simply
    // fades the new page in from the background.
    const Layout& L = Lay();
    RECT rc = {};
    const bool ok = m_hwnd && GetClientRect(m_hwnd, &rc) && rc.right > 0 && rc.bottom > 0
                    && s_buf.dc && L.content.w > 2
                    && L.content.x + int(L.content.w) <= rc.right;
    if (!ok)
    {
        s_snap.w = 0; // invalidate any previous snapshot
        return;
    }
    const int cw = int(L.content.w);
    s_snap.Ensure(cw, int(rc.bottom));
    BitBlt(s_snap.dc, 0, 0, cw, int(rc.bottom), s_buf.dc, int(L.content.x), 0, SRCCOPY);
}

namespace {

void AlphaBlendBuf(HDC dst, int x, int y, const BackBuffer& src, BYTE alpha)
{
    if (!src.dc || alpha == 0)
        return;
    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    AlphaBlend(dst, x, y, src.w, src.h, src.dc, 0, 0, src.w, src.h, bf);
}

float EaseOutCubic(float t)
{
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

} // namespace

void App::OnPaint(HDC dc, const RECT&)
{
    Lay(); // fresh layout

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0)
        return;

    s_buf.Ensure(W, H);
    Gdiplus::Graphics g(s_buf.dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    const Pal& pal = Palette();
    const Layout& L = m_lay;

    // base
    if (m_backdropActive)
    {
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        const Gdiplus::Color base = ApplyAlpha(pal.windowBg, pal.micaWindowAlpha);
        Gdiplus::SolidBrush baseBrush(base);
        g.FillRectangle(&baseBrush, L.root.gp());
    }
    else
    {
        g.Clear(pal.windowBg);
    }

    // sidebar
    {
        const Gdiplus::Color side = m_backdropActive
            ? ApplyAlpha(pal.sidebarBg, pal.micaSidebarAlpha) : pal.sidebarBg;
        Gdiplus::SolidBrush brush(side);
        g.FillRectangle(&brush, L.sidebar.gp());
    }

    // nav items
    for (int i = 0; i < PgCount; ++i)
    {
        ui::NavItem(g, L.nav[i], PageInfoOf(static_cast<PageId>(i)).icon,
                    PageInfoOf(static_cast<PageId>(i)).label, m_page == i, HoverTOf(ID_NAV + i));
    }
    // sidebar footer: version
    ui::Text(g, std::wstring(L"v") + AppVersion(), FSmall, pal.text3,
             RectF(0, L.root.h - float(S(28)), L.sidebar.w, float(S(24))), ui::Align::Center);

    // titlebar
    {
        float tlF[3] = { HoverT(ID_TL_CLOSE), HoverT(ID_TL_MIN), HoverT(ID_TL_MAX) };
        const float group = tlF[0] > tlF[1] ? (tlF[0] > tlF[2] ? tlF[0] : tlF[2])
                                            : (tlF[1] > tlF[2] ? tlF[1] : tlF[2]);
        int pressIdx = -1;
        if (m_pressId >= ID_TL_CLOSE && m_pressId <= ID_TL_MAX)
            pressIdx = m_pressId - ID_TL_CLOSE;
        ui::TrafficLights(g, L.traffic, m_windowActive, group, tlF, pressIdx);

        // macOS System Settings style: the active pane's name sits centered
        // in the toolbar (drawn after the crossfade below fades it in).
        if (L.content.w > float(S(420)) && m_pageAnim.v >= 1.0f)
            ui::Text(g, PageInfoOf(m_page).label, FSmallSem, pal.text2,
                     RectF(L.content.x, 0, L.content.w, L.titlebar.h), ui::Align::Center);
    }

    // page content: either a straight render or, right after a page switch, a
    // crossfade between the snapshot of the outgoing page and the fresh one.
    if (m_pageAnim.v < 1.0f && L.content.w > 2)
    {
        const float t = EaseOutCubic(m_pageAnim.v);
        const int cw = int(L.content.w);

        // 1) render the incoming page into its own buffer
        s_newPage.Ensure(cw, H);
        {
            Gdiplus::Graphics gn(s_newPage.dc);
            gn.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            gn.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            gn.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
            const Gdiplus::Color base = m_backdropActive
                ? ApplyAlpha(pal.windowBg, pal.micaWindowAlpha) : pal.windowBg;
            gn.Clear(base);
            const Gdiplus::GraphicsState st = gn.Save();
            gn.TranslateTransform(-L.content.x, 0.0f);
            PaintPageBody(gn, L);
            gn.Restore(st);
        }

        // 2) rebuild the content area from an opaque base, then blend the
        //    outgoing (fading up) and incoming (fading in, sliding up) views
        const Gdiplus::Color contentBase = m_backdropActive
            ? ApplyAlpha(pal.windowBg, pal.micaWindowAlpha) : pal.windowBg;
        Gdiplus::SolidBrush cbBrush(contentBase);
        g.FillRectangle(&cbBrush, Gdiplus::RectF(L.content.x, 0, L.content.w, float(H)));

        const float slide = float(S(16));
        const int dyOld = int(-slide * t + 0.5f);
        const int dyNew = int(slide * (1.0f - t) + 0.5f);
        if (s_snap.dc && s_snap.w == cw && s_snap.h == H)
            AlphaBlendBuf(s_buf.dc, int(L.content.x), dyOld, s_snap, BYTE(255 * (1.0f - t)));
        AlphaBlendBuf(s_buf.dc, int(L.content.x), dyNew, s_newPage, BYTE(255 * t));

        // The toolbar title faded out with the old view; fade the new pane's
        // name in on top of the incoming buffer.
        if (L.content.w > float(S(420)))
        {
            const Gdiplus::Color c(BYTE(255 * t), pal.text2.GetR(), pal.text2.GetG(),
                                   pal.text2.GetB());
            ui::Text(g, PageInfoOf(m_page).label, FSmallSem, c,
                     RectF(L.content.x, 0, L.content.w, L.titlebar.h), ui::Align::Center);
        }
    }
    else
    {
        PaintPageBody(g, L);
    }

    // overlay page scrollbar (auto-hiding, macOS style, no track)
    {
        // Live pill: recomputed from the current scroll offset so it tracks
        // wheel glides and thumb drags (the cached layout rect would freeze).
        const RectF pageThumb = PageThumbRect();
        if (L.pageMaxScroll > 0 && pageThumb.w > 0)
        {
            const float reveal = m_sbAnim.v;
            if (reveal > 0.01f)
            {
                const bool active = m_pageThumbDrag || m_hoverId == ID_PAGE_SB;
                const Gdiplus::Color thumb = SbThumbColor((active ? 0.55f : 0.38f) * reveal);
                ui::RoundRect(g, pageThumb, 4.0f, &thumb, nullptr, 0);
            }
        }
    }

    // language picker popup - topmost chrome, drawn after every page and
    // overlay control so it floats above the settings card.
    if (m_langOpen)
    {
        const RectF pop = LangPopupRect();
        if (pop.w > 0)
        {
            const Gdiplus::Color shadow(52, 0, 0, 0);
            ui::RoundRect(g, RectF(pop.x, pop.y + float(S(2)), pop.w, pop.h), 10.0f,
                          &shadow, nullptr, 0);
            ui::RoundRect(g, pop, 10.0f, &pal.cardBg, &pal.cardBorder, 1.0f);

            const Gdiplus::GraphicsState st = g.Save();
            g.SetClip(pop.gp());
            const float rowH = float(S(30));
            for (int i = 0; i < lang::kLangCount; ++i)
            {
                const float y = pop.y + i * rowH - m_langScroll;
                if (y + rowH < pop.y || y > pop.bottom())
                    continue;
                const RectF row(pop.x + float(S(3)), y, pop.w - float(S(6)), rowH);
                const float hf = (i == m_langHover) ? HoverT(ID_LANG_ITEM + i) : 0.0f;
                if (hf > 0.01f)
                {
                    const Gdiplus::Color fill = ui::WithAlpha(pal.hoverFill, hf);
                    ui::RoundRect(g, RectF(row.x, row.y + 1, row.w, row.h - 2), 6.0f,
                                  &fill, nullptr, 0);
                }
                const bool cur = i == int(lang::Current());
                ui::Text(g, lang::DisplayName(static_cast<lang::LangId>(i)),
                         cur ? FBodySem : FBody, cur ? pal.text : pal.text2,
                         RectF(row.x + float(S(12)), row.y, row.w - float(S(32)), row.h));
                if (cur)
                    ui::Icon(g, IconKind::Check,
                             RectF(row.right() - float(S(24)),
                                   row.y + (rowH - float(S(15))) / 2, float(S(15)), float(S(15))),
                             pal.accent, 1.9f);
            }
            g.Restore(st);
        }
    }

    // toast (slides up, holds, fades away)
    if (!m_toastText.empty())
    {
        const float k = m_toastAnim.v;
        if (k > 0.005f)
        {
            const float tw = ui::MeasureText(m_toastText, FBody);
            const float iconD = float(S(17));
            const float pw = tw + float(S(52)) + iconD;
            const float ph = float(S(38));
            const float dy = float(S(16)) * (1.0f - k);
            const RectF pill(L.root.w / 2 - pw / 2, L.root.h - float(S(22)) - ph + dy, pw, ph);
            const Gdiplus::Color pillBg = FadeKeep(pal.toastBg, k);
            ui::RoundRect(g, pill, ph / 2, &pillBg, nullptr, 0);
            const Gdiplus::Color iconColor = m_toastKind == 1 ? pal.success
                                             : m_toastKind == 2 ? pal.warning
                                             : m_toastKind == 3 ? pal.danger
                                                                : pal.accent;
            const IconKind icon = m_toastKind == 1 ? IconKind::Check
                                  : m_toastKind == 3 ? IconKind::Close
                                  : m_toastKind == 2 ? IconKind::Warn
                                                     : IconKind::Info;
            ui::Icon(g, icon, RectF(pill.x + float(S(16)), pill.y + (ph - iconD) / 2, iconD, iconD),
                     iconColor, 1.6f);
            ui::Text(g, m_toastText, FBody, FadeKeep(pal.toastText, k),
                     RectF(pill.x + float(S(40)), pill.y, tw + float(S(8)), ph));
        }
    }

    BitBlt(dc, 0, 0, W, H, s_buf.dc, 0, 0, SRCCOPY);
}

} // namespace micp
