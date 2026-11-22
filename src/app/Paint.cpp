#include "app/App.h"

#include "core/AppPaths.h"

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

BackBuffer s_buf;

Gdiplus::Color ApplyAlpha(const Gdiplus::Color& c, float a)
{
    return Gdiplus::Color(BYTE(255 * a), c.GetR(), c.GetG(), c.GetB());
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

HoverState HoverOf(const App& app, int id)
{
    if (app.m_pressId == id)
        return app.m_hoverId == id ? HoverState::Press : HoverState::None;
    if (app.m_hoverId == id)
        return HoverState::Hover;
    return HoverState::None;
}

std::wstring FileNameOf(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return path;
    return path.substr(slash + 1);
}

} // namespace

// ------------------------------------------------------------------ pages

static void PaintOverview(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    ui::Text(g, L"\u603b\u89c8", FTitle, pal.text, // 总览
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    // --- warning card -----------------------------------------------------
    if (L.ovWarn.w > 0)
    {
        Card(g, L.ovWarn);
        ui::Icon(g, IconKind::Warn, RectF(L.ovWarn.x + float(S(18)), L.ovWarn.y + float(S(26)),
                                          float(S(22)), float(S(22))), pal.warning, 1.6f);
        ui::Text(g, L"\u672a\u627e\u5230 Aurora.dll", FBodySem, pal.danger, // 未找到 Aurora.dll
                 RectF(L.ovWarn.x + float(S(52)), L.ovWarn.y + float(S(20)),
                       L.ovWarn.w - float(S(52)) - float(S(140)), float(LineH(FBodySem))));
        ui::Text(g, L"\u8bf7\u5c06\u4ee3\u7406 DLL \u653e\u5230\u7a0b\u5e8f\u540c\u76ee\u5f55\uff0c\u6216\u5230\u300c\u6ce8\u5165\u300d\u9875\u9009\u62e9\u6587\u4ef6", // 请将代理 DLL 放到程序同目录，或到「注入」页选择文件
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
            headline = L"\u6ce8\u5165\u8fdb\u884c\u4e2d\u2026"; // 注入进行中…
            dot = pal.accent;
        }
        else if (listening)
        {
            headline = L"\u4ee3\u7406\u8fd0\u884c\u4e2d"; // 代理运行中
            dot = pal.success;
        }
        else if (dead)
        {
            headline = L"\u76ee\u6807\u5ba2\u6237\u7aef\u5df2\u9000\u51fa"; // 目标客户端已退出
            dot = pal.text3;
        }
        else if (winFound)
        {
            headline = L"\u5df2\u627e\u5230\u76ee\u6807\u7a97\u53e3"; // 已找到目标窗口
            dot = pal.accent;
        }
        else
        {
            headline = L"\u7b49\u5f85\u76ee\u6807\u7a97\u53e3"; // 等待目标窗口
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
            sub1 = L"\u6b63\u5728\u6267\u884c\u6ce8\u5165\u6d41\u7a0b\uff0c\u53ef\u5728\u300c\u6ce8\u5165\u300d\u9875\u67e5\u770b\u8fdb\u5ea6"; // 正在执行注入流程，可在「注入」页查看进度
        else if (listening)
            sub1 = FormatStr(L"PID %u \u00b7 %d \u4e2a B \u5ba2\u6237\u7aef\u5df2\u8fde\u63a5 \u00b7 \u76d1\u542c 127.0.0.1:%d", // PID … · … 个 B 客户端已连接 · 监听 …
                             app.m_snap.pid, app.m_snap.bConnections, app.m_set.port);
        else if (dead)
            sub1 = L"\u6ce8\u5165\u8fc7\u7684\u5ba2\u6237\u7aef\u8fdb\u7a0b\u5df2\u9000\u51fa\uff0c\u4ee3\u7406\u968f\u4e4b\u505c\u6b62"; // 注入过的客户端进程已退出，代理随之停止
        else if (winFound)
            sub1 = FormatStr(L"\u300a%s\u300b \u00b7 PID %u", app.m_snap.windowTitle.c_str(),
                             app.m_snap.windowPid);
        else
            sub1 = L"\u542f\u52a8 A \u5ba2\u6237\u7aef\u5e76\u8fdb\u5165\u76ee\u6807\u670d\u52a1\u5668\u540e\uff0c\u5373\u53ef\u4e00\u952e\u6ce8\u5165"; // 启动 A 客户端并进入目标服务器后，即可一键注入
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
        ui::Text(g, FormatStr(L"\u672c\u6b21\u4f1a\u8bdd\u6210\u529f\u6ce8\u5165 %d \u6b21", app.m_okCount), // 本次会话成功注入 N 次
                 FSmall, pal.text3,
                 RectF(leftX, L.ovStatus.bottom() - float(S(26)), L.ovStatus.w - float(S(180)),
                       float(LineH(FSmall))));
    }

    // --- config summary ----------------------------------------------------
    if (L.ovInfo.w > 0)
    {
        Card(g, L.ovInfo, L"\u5f53\u524d\u914d\u7f6e"); // 当前配置
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
        rows[0] = { L"\u4ee3\u7406 DLL", FileNameOf(app.ResolvedDllPath()), app.m_dllFound, true,
                    app.m_dllFound ? pal.success : pal.danger,
                    app.m_dllFound ? L"  \u2713 \u5df2\u627e\u5230" : L"  \u2717 \u672a\u627e\u5230" };
        rows[1] = { L"\u7a97\u53e3\u5173\u952e\u5b57", app.m_set.keyword, false, false, pal.text, L"" };
        rows[2] = { L"\u76d1\u542c\u7aef\u53e3", std::to_wstring(app.m_set.port) + L"  (B \u5ba2\u6237\u7aef\u8fde\u63a5\u7aef)", false, false, pal.text, L"" };
        rows[3] = { L"\u65e5\u5fd7\u6765\u6e90",
                     app.m_logSource.empty() ? L"\u81ea\u52a8\u63a2\u6d4b\u4e2d" : FileNameOf(app.m_logSource),
                     false, false, pal.text, L"" };

        for (int i = 0; i < 4; ++i)
        {
            const float y = rowY0 + i * float(S(30));
            ui::Text(g, rows[i].label, FSmall, pal.text2,
                     RectF(labelX, y, labelW, float(S(30))));
            RectF vr(labelX + labelW, y, L.ovInfo.w - labelW - float(S(36)) - float(S(200)), float(S(30)));
            if (rows[i].hasState)
                vr.w -= float(S(90));
            ui::Text(g, rows[i].value, FBody, pal.text, vr, ui::Align::Right);
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
        Card(g, L.ovHint, L"B \u5ba2\u6237\u7aef\u5982\u4f55\u8fde\u63a5"); // B 客户端如何连接
        const float x = L.ovHint.x + float(S(18));
        const float y0 = L.ovHint.y + float(S(24)) + float(LineH(FHead)) + float(S(14));
        const float w = L.ovHint.w - float(S(36));

        ui::Icon(g, IconKind::Plug, RectF(x, y0 + float(S(2)), float(S(18)), float(S(18))),
                 pal.accent, 1.5f);
        ui::Text(g, FormatStr(L"\u540c\u4e00\u53f0\u7535\u8111\uff1aB \u5ba2\u6237\u7aef\u300c\u76f4\u63a5\u8fde\u63a5\u300d 127.0.0.1:%d", // 同一台电脑：B 客户端「直接连接」…
                     app.m_set.port), FBody, pal.text,
                 RectF(x + float(S(28)), y0, w - float(S(28)), float(LineH(FBody))));

        std::vector<std::wstring> ips = AppPaths::LanIpAddresses();
        std::wstring lan;
        for (size_t i = 0; i < ips.size() && i < 3; ++i)
            lan += (i ? L"\u3001" : L"") + ips[i] + L":" + std::to_wstring(app.m_set.port);
        const std::wstring line2 = lan.empty()
            ? L"\u540c\u4e00\u5c40\u57df\u7f51\uff1a\u672a\u68c0\u6d4b\u5230\u53ef\u7528\u5730\u5740"
            : L"\u540c\u4e00\u5c40\u57df\u7f51\uff1a" + lan;

        ui::Text(g, line2, FBody, pal.text,
                 RectF(x + float(S(28)), y0 + float(LineH(FBody)) + float(S(6)),
                       w - float(S(28)), float(LineH(FBody))));
        ui::Text(g, L"A \u5ba2\u6237\u7aef\u4e3a\u88ab\u6ce8\u5165\u7684\u4e00\u7aef\uff1bB \u5ba2\u6237\u7aef\u901a\u8fc7\u4ee3\u7406\u5171\u4eab\u8be5\u4f1a\u8bdd\u3002", // A 客户端为被注入的一端；…
                 FSmall, pal.text3,
                 RectF(x, y0 + 2 * (float(LineH(FBody)) + float(S(6))) + float(S(4)), w,
                       float(LineH(FSmall))));
    }
}

static void PaintInject(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();
    App& app = g_app;

    ui::Text(g, L"\u6ce8\u5165", FTitle, pal.text, // 注入
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    // --- config card -------------------------------------------------------
    if (L.ijConfig.w > 0)
    {
        Card(g, L.ijConfig, L"\u6ce8\u5165\u914d\u7f6e");
        const float labelX = L.ijConfig.x + float(S(18));
        const float labelW = float(S(96));

        auto rowLabel = [&](const RectF& row, const wchar_t* label)
        {
            ui::Text(g, label, FSmall, pal.text2, RectF(labelX, row.y + 4, labelW, row.h - 8));
        };
        // Hints live under their fields now: full card width, wrapping - the
        // old right-side hints overflowed the card and hit the window edge.
        auto rowHint = [&](const RectF& r, const std::wstring& text)
        {
            if (r.w > 0)
                ui::Text(g, text, FSmall, pal.text3, r, ui::Align::Left, true);
        };

        rowLabel(L.ijRowDll, L"\u4ee3\u7406 DLL");
        rowLabel(L.ijRowKw, L"\u7a97\u53e3\u5173\u952e\u5b57");
        rowLabel(L.ijRowPort, L"\u76d1\u542c\u7aef\u53e3");
        rowHint(L.ijHintDll, app.m_set.dllPath.empty()
            ? std::wstring(L"\u672a\u9009\u62e9\u65f6\u5c06\u81ea\u52a8\u5728\u7a0b\u5e8f\u76ee\u5f55\u67e5\u627e Aurora.dll") // 未选择时将自动在程序目录查找 Aurora.dll
            : std::wstring(L"\u5df2\u624b\u52a8\u6307\u5b9a DLL \u8def\u5f84")); // 已手动指定 DLL 路径
        rowHint(L.ijHintKw, std::wstring(L"\u5339\u914d\u7a97\u53e3\u6807\u9898\u5305\u542b\u6b64\u5173\u952e\u5b57\u7684 Java \u8fdb\u7a0b")); // 匹配窗口标题包含此关键字的 Java 进程
        rowHint(L.ijHintPort, std::wstring(L"B \u5ba2\u6237\u7aef\u300c\u76f4\u63a5\u8fde\u63a5\u300d\u8bbf\u95ee\u6b64\u7aef\u53e3")); // B 客户端「直接连接」访问此端口
    }

    // --- steps card --------------------------------------------------------
    if (L.ijSteps.w > 0)
    {
        Card(g, L.ijSteps, L"\u6ce8\u5165\u8fdb\u5ea6");
        static const wchar_t* kStepNames[4] = {
            L"\u68c0\u67e5\u4ee3\u7406 DLL",   // 检查代理 DLL
            L"\u5b9a\u4f4d\u76ee\u6807\u7a97\u53e3", // 定位目标窗口
            L"\u6ce8\u5165 DLL",              // 注入 DLL
            L"\u7b49\u5f85\u4ee3\u7406\u76d1\u542c", // 等待代理监听
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
            const Gdiplus::Color nameColor = app.m_stepState[i] == 3
                ? p.danger
                : (app.m_stepState[i] == 0 ? p.text2 : p.text);
            ui::Text(g, kStepNames[i], FBodySem, nameColor,
                     RectF(t.x, t.y + float(S(8)), t.w, float(LineH(FBodySem))));
            if (!app.m_stepDetail[i].empty())
                ui::Text(g, app.m_stepDetail[i], FSmall, p.text2,
                         RectF(t.x, t.y + float(S(10)) + float(LineH(FBodySem)), t.w,
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
        if (app.m_hoverId / 100 == seg.id && app.m_hoverId % 100 >= 1)
            hoverItem = app.m_hoverId % 100 - 1;
        ui::Segmented(g, seg.r, seg.items, seg.sel, hoverItem, nullptr);
    }
    ui::Text(g, L"\u81ea\u52a8\u6eda\u52a8", FSmall, pal.text2, // 自动滚动
             RectF(L.lgTglLabel.x - float(S(4)), L.lgTgl.y - float(S(3)), L.lgTglLabel.w,
                   L.lgTgl.h + float(S(6))), ui::Align::Right);
    for (const TglDef& t : L.tgls)
        if (t.id == ID_TGL_FOLLOW)
            ui::Toggle(g, t.r, t.on, HoverOf(app, t.id));

    // card
    const Gdiplus::Color cardFill = app.m_backdropActive
        ? ApplyAlpha(pal.cardBg, pal.micaCardAlpha) : pal.cardBg;
    ui::RoundRect(g, L.lgCard, 12.0f, &cardFill, &pal.cardBorder, 1.0f);

    // head: source + counts
    const std::wstring src = app.m_logSource.empty()
        ? L"\u81ea\u52a8\u63a2\u6d4b\u4e2d\u2026" // 自动探测中…
        : app.m_logSource;
    ui::Text(g, src, FMonoSmall, pal.text3,
             RectF(L.lgHead.x + float(S(14)), L.lgHead.y, L.lgHead.w - float(S(28)) - float(S(160)),
                   L.lgHead.h));
    int shown = 0, total = int(app.m_lines.size());
    for (const auto& e : app.m_lines)
        if (app.PassesFilter(e))
            ++shown;
    const std::wstring counts = app.m_filter == LfAll
        ? FormatStr(L"\u5171 %d \u884c", total) // 共 N 行
        : FormatStr(L"%d / %d \u884c", shown, total);
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
        ui::Text(g, L"\u6682\u65e0\u65e5\u5fd7\uff0c\u7b49\u5f85\u4ee3\u7406\u8f93\u51fa\u2026", FBody, pal.text2, // 暂无日志，等待代理输出…
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
        // scrollbar
        if (maxScroll > 0 && L.lgBody.h > 40)
        {
            const float viewH = L.lgBody.h;
            const float contentH = shown * rowH;
            float thumbH = viewH * viewH / contentH;
            if (thumbH < float(S(28)))
                thumbH = float(S(28));
            const float track = viewH - thumbH;
            const float thumbY = L.lgBody.y + (app.m_logScroll / maxScroll) * track;
            ui::RoundRect(g, RectF(L.lgScroll.x, thumbY, L.lgScroll.w, thumbH), 4.0f,
                          &pal.separator, nullptr, 0);
        }
    }
}

static void PaintSettings(Gdiplus::Graphics& g, const Layout& L)
{
    const Pal& pal = Palette();

    ui::Text(g, L"\u8bbe\u7f6e", FTitle, pal.text, // 设置
             RectF(L.pageBox.x, L.pageTop, L.pageBox.w, float(LineH(FTitle))));

    if (L.stAppearance.w > 0)
    {
        Card(g, L.stAppearance, L"\u5916\u89c2"); // 外观
        ui::Text(g, L"\u4e3b\u9898", FBody, pal.text, RectF(L.stSegThemeLabel.x, L.stSegThemeLabel.y, L.stSegThemeLabel.w, L.stSegThemeLabel.h)); // 主题
        ui::Text(g, L"\u7a97\u53e3\u6750\u8d28", FBody, pal.text, RectF(L.stSegBackdropLabel.x, L.stSegBackdropLabel.y, L.stSegBackdropLabel.w, L.stSegBackdropLabel.h)); // 窗口材质
        const std::wstring note = L"\u4e91\u6bcd / \u4e9a\u514b\u529b\u4e3a\u7cfb\u7edf\u6750\u8d28\uff08\u9700\u8981 Windows 11 22H2+\uff09\uff1b\u5173\u95ed\u65f6\u4f7f\u7528\u7eaf\u8272\u80cc\u666f"; // 云母 / 亚克力为系统材质…
        ui::Text(g, note, FSmall, pal.text3,
                 RectF(L.stAppearance.x + float(S(18)), L.stAppearance.bottom() - float(S(30)),
                       L.stAppearance.w - float(S(36)), float(S(28))),
                 ui::Align::Left, true);
    }

    if (L.stBehavior.w > 0)
    {
        Card(g, L.stBehavior, L"\u884c\u4e3a"); // 行为
        static const wchar_t* kLabels[3] = {
            L"\u6ce8\u5165\u5b8c\u6210\u540e\u81ea\u52a8\u8df3\u8f6c\u65e5\u5fd7", // 注入完成后自动跳转日志
            L"\u6ce8\u5165\u5b8c\u6210\u65f6\u663e\u793a\u6d6e\u7a97\u63d0\u793a", // 注入完成时显示浮窗提示
            L"\u65e5\u5fd7\u9875\u81ea\u52a8\u8ddf\u968f\u6eda\u52a8",           // 日志页自动跟随滚动
        };
        for (int i = 0; i < 3; ++i)
        {
            ui::Text(g, kLabels[i], FBody, pal.text,
                     RectF(L.stTglLabel[i].x, L.stTglLabel[i].y + (float(S(44)) - float(LineH(FBody))) / 2,
                           L.stTglLabel[i].w, float(LineH(FBody))));
            if (i < 2)
                ui::HLine(g, L.stTglLabel[i].x, L.stTglLabel[i].bottom(),
                          L.stBehavior.w - float(S(36)), pal.separator);
        }
    }

    if (L.stLog.w > 0)
    {
        Card(g, L.stLog, L"\u65e5\u5fd7\u6765\u6e90"); // 日志来源
        // wrapping hint + settings-file line, stacked without overlap
        ui::Text(g, L"\u9ed8\u8ba4\u81ea\u52a8\u63a2\u6d4b %APPDATA%\\.minecraft\\proxy.log\u3001%TEMP%\\MinecraftProxy.log \u7b49\u4f4d\u7f6e\uff1b\u70b9\u51fb\u4e0a\u65b9\u8def\u5f84\u53ef\u624b\u52a8\u6307\u5b9a\u6587\u4ef6", // 默认自动探测…；点击上方路径可手动指定文件
                 FSmall, pal.text3, L.stIniLabel, ui::Align::Left, true);
        ui::Text(g, L"\u8bbe\u7f6e\u6587\u4ef6\uff1a" + AppPaths::SettingsIni(), // 设置文件：…
                 FSmall, pal.text3,
                 RectF(L.stIniLabel.x, L.stIniLabel.bottom() + float(S(4)),
                       L.stLog.w - float(S(36)), float(LineH(FSmall))));
    }

    if (L.stResetCard.w > 0)
    {
        Card(g, L.stResetCard);
        ui::Text(g, L"\u6062\u590d\u5168\u90e8\u9ed8\u8ba4\u503c\uff08\u4e3b\u9898\u3001\u6ce8\u5165\u4e0e\u65e5\u5fd7\u8bbe\u7f6e\uff09\uff1b\u4e0d\u5f71\u54cd\u7a97\u53e3\u4f4d\u7f6e", // 恢复全部默认值…；不影响窗口位置
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
        const std::wstring v = FormatStr(L"v%s \u00b7 \u539f\u751f Win32 \u5355\u6587\u4ef6", // v… · 原生 Win32 单文件
                                         AppVersion());
        const float vw = ui::MeasureText(v, FSmall);
        RectF pill = L.abVersion;
        pill.w = vw + float(S(28));
        pill.x = L.pageBox.x + L.pageBox.w / 2 - pill.w / 2;
        pill.h = float(S(24));
        ui::RoundRect(g, pill, pill.h / 2, &pal.hoverFill, nullptr, 0);
        ui::Text(g, v, FSmall, pal.text2, pill, ui::Align::Center);
    }

    ui::Text(g, L"Minecraft 1.20.1 \u5ba2\u6237\u7aef\u53cd\u5411\u4ee3\u7406 \u00b7 \u6ce8\u5165\u4e0e\u7ba1\u7406\u5de5\u5177", // Minecraft 1.20.1 客户端反向代理 · 注入与管理工具
             FBody, pal.text2, L.abTag, ui::Align::Center);

    const float fy = L.abFoot.y + float(S(6));
    ui::HLine(g, L.pageBox.x + float(S(60)), fy, L.pageBox.w - float(S(120)), pal.separator);
    struct AboutLine
    {
        const wchar_t* text;
        bool dim;
    };
    const AboutLine lines[] = {
        { L"\u539f\u751f Win32 / GDI+ \u00b7 \u65e0\u4efb\u4f55\u8fd0\u884c\u65f6\u4f9d\u8d56 \u00b7 \u5355\u6587\u4ef6 exe", false }, // 原生 Win32 / GDI+ · 无任何运行时依赖 · 单文件 exe
        { L"\u4ee3\u7406\u6838\u5fc3 Aurora.dll \u968f Release \u9644\u5e26\uff0c\u4e0e\u672c\u7a0b\u5e8f\u540c\u76ee\u5f55\u5373\u53ef\u4f7f\u7528", false }, // 代理核心 Aurora.dll 随 Release 附带，与本程序同目录即可使用
        { L"\u4ec5\u4f9b\u5b66\u4e60\u4ea4\u6d41\u4f7f\u7528\uff0c\u8bf7\u9075\u5b88 Minecraft EULA \u4e0e\u76ee\u6807\u670d\u52a1\u5668\u89c4\u5219", true }, // 仅供学习交流使用，请遵守…
    };
    float ly = fy + float(S(16));
    for (const AboutLine& line : lines)
    {
        ui::Text(g, line.text, FSmall, line.dim ? pal.text3 : pal.text2,
                 RectF(L.pageBox.x, ly, L.pageBox.w, float(LineH(FSmall))), ui::Align::Center);
        ly += float(LineH(FSmall)) + float(S(6));
    }
}

// ------------------------------------------------------------------ chrome

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
        const HoverState hs = (m_pressId == ID_NAV + i) ? HoverState::Press
                               : (m_hoverId == ID_NAV + i) ? HoverState::Hover
                                                           : HoverState::None;
        ui::NavItem(g, L.nav[i], PageInfoOf(static_cast<PageId>(i)).icon,
                    PageInfoOf(static_cast<PageId>(i)).label, m_page == i, hs);
    }
    // sidebar footer: version
    ui::Text(g, std::wstring(L"v") + AppVersion(), FSmall, pal.text3,
             RectF(0, L.root.h - float(S(28)), L.sidebar.w, float(S(24))), ui::Align::Center);

    // titlebar
    {
        int pressIdx = -1;
        if (m_pressId >= ID_TL_CLOSE && m_pressId <= ID_TL_MAX)
            pressIdx = m_pressId - ID_TL_CLOSE;
        ui::TrafficLights(g, L.traffic, m_windowActive, m_tlHoverAny, m_tlHoverIdx, pressIdx);

        if (L.content.w > float(S(420)))
            ui::Text(g, kAppName, FSmall, pal.text3,
                     RectF(L.content.x, 0, L.content.w, L.titlebar.h), ui::Align::Center);
    }

    // page content: clipped to the content viewport and translated by the
    // page scroll offset (the logs page never translates; it scrolls inside
    // its card). Registered controls and the EDIT field frames share the same
    // transform so hit-testing (layout coords + scroll) stays consistent.
    {
        const Gdiplus::GraphicsState pageState = g.Save();
        Gdiplus::RectF viewport = L.pageBox.gp();
        viewport.Width += float(S(4)); // let card shadows breathe
        g.SetClip(viewport);
        if (m_pageScroll > 0.01f)
            g.TranslateTransform(0.0f, -m_pageScroll);

        switch (m_page)
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
            ui::Button(g, b.r, b.label, b.style, HoverOf(*this, b.id), b.enabled, b.danger);
        for (const RowDef& row : L.rows)
            ui::PathRow(g, row.r, row.text, row.icon, HoverOf(*this, row.id), row.alert);
        for (const SegDef& seg : L.segs)
        {
            if (seg.id == ID_SEG_LOGFILTER)
                continue; // painted with the logs page
            int hoverItem = -1;
            if (m_hoverId / 100 == seg.id && m_hoverId % 100 >= 1)
                hoverItem = m_hoverId % 100 - 1;
            ui::Segmented(g, seg.r, seg.items, seg.sel, hoverItem, nullptr);
        }
        for (const TglDef& t : L.tgls)
        {
            if (t.id == ID_TGL_FOLLOW && m_page == PgLogs)
                continue; // painted with the logs page
            ui::Toggle(g, t.r, t.on, HoverOf(*this, t.id));
        }

        // EDIT field frames (borderless EDIT children render on top of these)
        if (m_page == PgInject)
        {
            auto fieldFrame = [&](const RectF& r, int ctrlId)
            {
                const bool focused = m_editFocused == ctrlId;
                const bool invalid = ctrlId == ID_EDIT_PORT && m_portInvalid;
                Gdiplus::Color border = focused ? ApplyAlpha(pal.accent, 0.8f)
                                   : invalid ? pal.danger : pal.fieldBorder;
                const Gdiplus::Color fill = m_backdropActive
                    ? ApplyAlpha(pal.cardBg, pal.micaCardAlpha) : pal.cardBg;
                ui::RoundRect(g, r, 8.0f, &fill, &border, focused ? 1.4f : 1.0f);
            };
            fieldFrame(L.ijEditKw, ID_EDIT_KEYWORD);
            fieldFrame(L.ijEditPort, ID_EDIT_PORT);
        }

        g.Restore(pageState);
    }

    // overlay page scrollbar (unscrolled, right of the content column)
    if (L.pageMaxScroll > 0 && L.pageThumb.w > 0)
    {
        const Gdiplus::Color thumb = m_pageThumbDrag ? pal.text3 : pal.separator;
        ui::RoundRect(g, L.pageThumb, 4.0f, &thumb, nullptr, 0);
    }

    // toast
    if (!m_toastText.empty())
    {
        const float tw = ui::MeasureText(m_toastText, FBody);
        const float iconD = float(S(17));
        const float pw = tw + float(S(52)) + iconD;
        const float ph = float(S(38));
        const RectF pill(L.root.w / 2 - pw / 2, L.root.h - float(S(22)) - ph, pw, ph);
        ui::RoundRect(g, pill, ph / 2, &pal.toastBg, nullptr, 0);
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
        ui::Text(g, m_toastText, FBody, pal.toastText,
                 RectF(pill.x + float(S(40)), pill.y, tw + float(S(8)), ph));
    }

    BitBlt(dc, 0, 0, W, H, s_buf.dc, 0, 0, SRCCOPY);
}

} // namespace micp
