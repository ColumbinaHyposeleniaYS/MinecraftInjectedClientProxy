#include "app/Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>

#include <windows.h>
#include <winternl.h>

namespace {

// Windows 11 22H2 (build 22621) introduced DWMWA_SYSTEMBACKDROP_TYPE.
bool systemBackdropSupported()
{
    typedef LONG(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return false;
    auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
    if (!rtlGetVersion)
        return false;
    RTL_OSVERSIONINFOW info;
    ZeroMemory(&info, sizeof(info));
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) != 0)
        return false;
    return info.dwBuildNumber >= 22621;
}

QColor rgb(int r, int g, int b)
{
    return QColor(r, g, b);
}

QColor rgba(int r, int g, int b, int a)
{
    return QColor(r, g, b, a);
}

} // namespace

Theme::Theme()
    : QObject(nullptr)
{
    m_pref = Pref::System;
    m_backdrop = Backdrop::None;
    recompute();

    // Follow the OS light/dark scheme when the preference is "System".
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] {
                if (m_pref == Pref::System)
                    emitChanged();
            });
}

Theme* Theme::instance()
{
    static Theme t;
    return &t;
}

void Theme::recompute()
{
    m_pal = (currentMode() == Mode::Dark) ? buildDark() : buildLight();
}

void Theme::emitChanged()
{
    recompute();
    emit changed();
}

Theme::Palette Theme::buildLight()
{
    Palette p;
    p.dark = false;

    p.windowBg = rgb(0xF5, 0xF5, 0xF7);
    p.sidebarBg = rgb(0xEF, 0xEF, 0xF2);
    p.card = rgb(0xFF, 0xFF, 0xFF);
    p.border = rgba(0x1D, 0x1D, 0x1F, 26);

    p.textPrimary = rgb(0x1D, 0x1D, 0x1F);
    p.textSecondary = rgb(0x6E, 0x6E, 0x73);
    p.textTertiary = rgb(0xAE, 0xAE, 0xB2);

    p.accent = rgb(0x0A, 0x84, 0xFF);
    p.accentHover = rgb(0x2E, 0x96, 0xFF);
    p.accentPressed = rgb(0x07, 0x6E, 0xD1);
    p.onAccent = rgb(0xFF, 0xFF, 0xFF);
    p.accentSoft = rgba(0x0A, 0x84, 0xFF, 28);

    p.controlBg = rgba(120, 120, 128, 31);
    p.controlBgHover = rgba(120, 120, 128, 46);
    p.controlBgActive = rgba(120, 120, 128, 60);

    p.fieldBg = rgb(0xFF, 0xFF, 0xFF);
    p.fieldBorder = rgba(0x1D, 0x1D, 0x1F, 40);
    p.separator = rgba(0x1D, 0x1D, 0x1F, 24);

    p.success = rgb(0x2E, 0xA0, 0x43);
    p.warning = rgb(0xE0, 0x87, 0x00);
    p.danger = rgb(0xE0, 0x36, 0x2C);
    p.successSoft = rgba(52, 168, 83, 26);
    p.warningSoft = rgba(224, 135, 0, 28);
    p.dangerSoft = rgba(224, 54, 44, 26);

    p.toastBg = rgba(0x1D, 0x1D, 0x1F, 224);
    p.toastText = rgb(0xF5, 0xF5, 0xF7);
    p.scrollbarThumb = rgba(0x1D, 0x1D, 0x1F, 56);

    p.trafficRed = rgb(0xFF, 0x5F, 0x57);
    p.trafficYellow = rgb(0xFE, 0xBC, 0x2E);
    p.trafficGreen = rgb(0x28, 0xC8, 0x40);
    p.trafficInactive = rgb(0xD2, 0xD2, 0xD6);

    p.segmentedThumb = rgb(0xFF, 0xFF, 0xFF);
    return p;
}

Theme::Palette Theme::buildDark()
{
    Palette p;
    p.dark = true;

    p.windowBg = rgb(0x1C, 0x1C, 0x1E);
    p.sidebarBg = rgb(0x21, 0x21, 0x24);
    p.card = rgb(0x2A, 0x2A, 0x2E);
    p.border = rgba(255, 255, 255, 26);

    p.textPrimary = rgb(0xF5, 0xF5, 0xF7);
    p.textSecondary = rgb(0x98, 0x98, 0x9D);
    p.textTertiary = rgb(0x63, 0x63, 0x66);

    p.accent = rgb(0x0A, 0x84, 0xFF);
    p.accentHover = rgb(0x36, 0x9D, 0xFF);
    p.accentPressed = rgb(0x07, 0x6E, 0xD1);
    p.onAccent = rgb(0xFF, 0xFF, 0xFF);
    p.accentSoft = rgba(0x0A, 0x84, 0xFF, 56);

    p.controlBg = rgba(120, 120, 128, 61);
    p.controlBgHover = rgba(120, 120, 128, 79);
    p.controlBgActive = rgba(120, 120, 128, 94);

    p.fieldBg = rgb(0x1E, 0x1E, 0x20);
    p.fieldBorder = rgba(255, 255, 255, 50);
    p.separator = rgba(255, 255, 255, 24);

    p.success = rgb(0x30, 0xD1, 0x58);
    p.warning = rgb(0xFF, 0x9F, 0x0A);
    p.danger = rgb(0xFF, 0x45, 0x3A);
    p.successSoft = rgba(48, 209, 88, 40);
    p.warningSoft = rgba(255, 159, 10, 42);
    p.dangerSoft = rgba(255, 69, 58, 40);

    p.toastBg = rgba(58, 58, 60, 235);
    p.toastText = rgb(0xF5, 0xF5, 0xF7);
    p.scrollbarThumb = rgba(255, 255, 255, 66);

    p.trafficRed = rgb(0xFF, 0x5F, 0x57);
    p.trafficYellow = rgb(0xFE, 0xBC, 0x2E);
    p.trafficGreen = rgb(0x28, 0xC8, 0x40);
    p.trafficInactive = rgb(0x48, 0x48, 0x4A);

    p.segmentedThumb = rgb(0x5E, 0x5E, 0x63);
    return p;
}

Theme::Mode Theme::systemMode()
{
    const Qt::ColorScheme scheme = qGuiApp->styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark ? Mode::Dark : Mode::Light;
}

Theme::Mode Theme::currentMode()
{
    const Pref pref = instance()->m_pref;
    if (pref == Pref::Light)
        return Mode::Light;
    if (pref == Pref::Dark)
        return Mode::Dark;
    return systemMode();
}

const Theme::Palette& Theme::pal()
{
    return instance()->m_pal;
}

Theme::Pref Theme::pref()
{
    return instance()->m_pref;
}

void Theme::setPref(Pref pref)
{
    Theme* self = instance();
    if (self->m_pref == pref)
        return;
    self->m_pref = pref;
    self->emitChanged();
}

Theme::Backdrop Theme::backdrop()
{
    return instance()->m_backdrop;
}

void Theme::setBackdrop(Backdrop backdrop)
{
    Theme* self = instance();
    if (self->m_backdrop == backdrop)
        return;
    self->m_backdrop = backdrop;
    // Repaint everything; the native window effects are re-applied by MainWindow.
    emit self->changed();
}

bool Theme::backdropSupported()
{
    static const bool supported = systemBackdropSupported();
    return supported;
}

bool Theme::translucentWindow()
{
    return backdrop() != Backdrop::None && backdropSupported();
}

QString Theme::fontFamily()
{
    return QStringLiteral("Segoe UI Variable Text, Segoe UI, Inter, Microsoft YaHei UI, Microsoft YaHei");
}

QString Theme::fontFamilyQss()
{
    return QStringLiteral("Segoe UI Variable Text");
}

QString Theme::monoFamily()
{
    return QStringLiteral("Cascadia Mono, Cascadia Code, Consolas");
}

QString Theme::monoFamilyQss()
{
    return QStringLiteral("Cascadia Mono");
}

QFont Theme::font(int pixelSize, int weight)
{
    QFont f;
    f.setFamilies(QStringList() << QStringLiteral("Segoe UI Variable Text")
                                << QStringLiteral("Segoe UI")
                                << QStringLiteral("Inter")
                                << QStringLiteral("Microsoft YaHei UI")
                                << QStringLiteral("Microsoft YaHei"));
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}

QColor Theme::wash(const QColor& color, double opacity)
{
    QColor c = color;
    c.setAlphaF(qBound(0.0, opacity, 1.0));
    return c;
}

QPalette Theme::buildPalette()
{
    const Palette& p = pal();
    QPalette palette;
    palette.setColor(QPalette::Window, p.windowBg);
    palette.setColor(QPalette::WindowText, p.textPrimary);
    palette.setColor(QPalette::Base, p.card);
    palette.setColor(QPalette::AlternateBase, p.windowBg);
    palette.setColor(QPalette::Text, p.textPrimary);
    palette.setColor(QPalette::PlaceholderText, p.textTertiary);
    palette.setColor(QPalette::Button, p.windowBg);
    palette.setColor(QPalette::ButtonText, p.textPrimary);
    palette.setColor(QPalette::ToolTipBase, p.toastBg);
    palette.setColor(QPalette::ToolTipText, p.toastText);
    palette.setColor(QPalette::Highlight, p.accent);
    palette.setColor(QPalette::HighlightedText, p.onAccent);
    palette.setColor(QPalette::Disabled, QPalette::Text, p.textTertiary);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, p.textTertiary);
    palette.setColor(QPalette::Link, p.accent);
    return palette;
}

QString Theme::globalQss()
{
    const Palette& p = pal();
    const QString colorStr = [](const QColor& c) {
        return c.name(QColor::HexArgb);
    };

    return QStringLiteral(R"(
        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }

        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: %1;
            border-radius: 3px;
            min-height: 28px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover { background: %2; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background: %1;
            border-radius: 3px;
            min-width: 28px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover { background: %2; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

        QToolTip {
            background: %3;
            color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 5px 9px;
            font-family: "%6";
            font-size: 12px;
        }

        QLineEdit {
            background: transparent;
            border: none;
            padding: 0;
            selection-background-color: %8;
            selection-color: %7;
        }

        QTextEdit {
            background: transparent;
            color: %7;
            border: none;
            selection-background-color: %8;
            selection-color: %7;
            font-family: "%9";
            font-size: 12px;
        }
    )")
        .arg(colorStr(p.scrollbarThumb),
             colorStr(p.accent),
             colorStr(p.dark ? QColor(0x3A, 0x3A, 0x3C) : QColor(0x6E, 0x6E, 0x73)),
             colorStr(p.toastText),
             colorStr(p.border),
             fontFamilyQss(),
             colorStr(p.textPrimary),
             colorStr(p.dark ? QColor(0x0A, 0x84, 0xFF, 90) : QColor(0x0A, 0x84, 0xFF, 40)),
             monoFamilyQss());
}

void Theme::applyApplicationFont()
{
    QApplication::setFont(Theme::font(13, QFont::Normal));
}
