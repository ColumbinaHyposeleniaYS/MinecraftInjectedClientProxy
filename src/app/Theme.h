#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPalette>
#include <QString>

/*
 * Central design tokens for the whole application.
 *
 * Light:  background #F5F5F7, cards #FFFFFF, text #1D1D1F / #6E6E73,
 *          accent #0A84FF.
 * Dark:   derived macOS-style dark surfaces.
 *
 * Metrics: 8 pt grid, card radius 12 px, control radius 8 px, sidebar 220 px,
 * title bar 48 px.
 */

/*
 * Version shown in the UI. CI release builds inject the git tag version via
 * -DMICP_APP_VERSION=x.y.z (see CMakeLists.txt); "1.0.0" is the fallback for
 * local builds.
 */
#ifndef MICP_APP_VERSION
#define MICP_APP_VERSION "1.0.0"
#endif

class Theme : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Light, Dark };
    enum class Pref { System, Light, Dark };
    enum class Backdrop { None, Mica, Acrylic };

    struct Palette
    {
        bool dark = false;

        QColor windowBg;
        QColor sidebarBg;
        QColor card;
        QColor border;

        QColor textPrimary;   // #1D1D1F
        QColor textSecondary; // #6E6E73
        QColor textTertiary;

        QColor accent;        // #0A84FF
        QColor accentHover;
        QColor accentPressed;
        QColor onAccent;
        QColor accentSoft;    // translucent fill for capsules

        QColor controlBg;      // neutral fill for secondary buttons / hover
        QColor controlBgHover;
        QColor controlBgActive;

        QColor fieldBg;
        QColor fieldBorder;
        QColor separator;

        QColor success;
        QColor warning;
        QColor danger;
        QColor successSoft;
        QColor warningSoft;
        QColor dangerSoft;

        QColor toastBg;
        QColor toastText;
        QColor scrollbarThumb;

        QColor trafficRed;
        QColor trafficYellow;
        QColor trafficGreen;
        QColor trafficInactive;

        QColor segmentedThumb;
    };

    static Theme* instance();

    static const Palette& pal();
    static Mode systemMode();
    static Mode currentMode();

    static Pref pref();
    static void setPref(Pref pref);

    static Backdrop backdrop();
    static void setBackdrop(Backdrop backdrop);
    static bool backdropSupported(); // Windows 11 22H2+
    static bool translucentWindow(); // backdrop active -> window paints translucent

    // Metrics.
    static constexpr int SidebarWidth = 220;
    static constexpr int TitleBarHeight = 48;
    static constexpr int CardRadius = 12;
    static constexpr int ControlRadius = 8;
    static constexpr int GridUnit = 8;

    static constexpr const char* AppVersion = MICP_APP_VERSION;
    static constexpr const char* AppName = "Minecraft Injected Client Proxy";
    static constexpr const char* CompilerTag =
#if defined(_MSC_VER)
        "MSVC"
#elif defined(__clang__)
        "Clang"
#elif defined(__GNUC__) || defined(__MINGW32__) || defined(__MINGW64__)
        "GCC (MinGW)"
#else
        "unknown toolchain"
#endif
        ;

    // Typography.
    static QString fontFamily();      // full fallback chain (for QFont::setFamilies)
    static QString fontFamilyQss();   // primary family for stylesheets
    static QString monoFamily();
    static QString monoFamilyQss();
    static QFont font(int pixelSize, int weight = 400);

    // Convenience: QColor with alpha factor (for backdrop translucency).
    static QColor wash(const QColor& color, double opacity);

    static QString globalQss();
    static QPalette buildPalette();
    static void applyApplicationFont();

signals:
    void changed();

private:
    explicit Theme();

    void recompute();
    void emitChanged();

    static Palette buildLight();
    static Palette buildDark();

    Pref m_pref = Pref::System;
    Backdrop m_backdrop = Backdrop::None;
    Palette m_pal;
};
