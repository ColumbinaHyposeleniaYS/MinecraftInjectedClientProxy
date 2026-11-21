#include "core/Settings.h"

#include "core/AppPaths.h"

#include <QSettings>

namespace {
constexpr const char* kKeyThemePref = "appearance/themePref";
constexpr const char* kKeyBackdrop = "appearance/backdrop";
constexpr const char* kKeyDllPath = "injection/dllPath";
constexpr const char* kKeyKeyword = "injection/windowKeyword";
constexpr const char* kKeyAutoSwitch = "injection/autoSwitchOnDone";
constexpr const char* kKeyToastOnDone = "injection/toastOnDone";
constexpr const char* kKeyLogOverride = "logs/pathOverride";
constexpr const char* kKeyFollowLog = "logs/follow";
constexpr const char* kKeyGeometry = "window/geometry";
} // namespace

Settings::Settings()
    : QObject(nullptr), m_store(new QSettings(AppPaths::settingsIni(), QSettings::IniFormat, this))
{
}

Settings* Settings::instance()
{
    static Settings s;
    return &s;
}

Settings::ThemePref Settings::themePref() const
{
    const int v = m_store->value(kKeyThemePref, 0).toInt();
    if (v == 1)
        return ThemePref::Light;
    if (v == 2)
        return ThemePref::Dark;
    return ThemePref::System;
}

void Settings::setThemePref(ThemePref pref)
{
    m_store->setValue(kKeyThemePref, int(pref));
    m_store->sync();
    emit themePrefChanged();
}

Settings::Backdrop Settings::backdrop() const
{
    const int v = m_store->value(kKeyBackdrop, 0).toInt();
    if (v == 1)
        return Backdrop::Mica;
    if (v == 2)
        return Backdrop::Acrylic;
    return Backdrop::None;
}

void Settings::setBackdrop(Backdrop backdrop)
{
    m_store->setValue(kKeyBackdrop, int(backdrop));
    m_store->sync();
    emit backdropChanged();
}

QString Settings::dllPath() const
{
    return m_store->value(kKeyDllPath).toString();
}

void Settings::setDllPath(const QString& path)
{
    m_store->setValue(kKeyDllPath, path);
    m_store->sync();
    emit injectionSettingsChanged();
}

QString Settings::windowKeyword() const
{
    const QString v = m_store->value(kKeyKeyword).toString();
    return v.isEmpty() ? AppPaths::defaultKeyword() : v;
}

void Settings::setWindowKeyword(const QString& keyword)
{
    m_store->setValue(kKeyKeyword, keyword);
    m_store->sync();
    emit injectionSettingsChanged();
}

bool Settings::autoSwitchOnDone() const
{
    return m_store->value(kKeyAutoSwitch, true).toBool();
}

void Settings::setAutoSwitchOnDone(bool on)
{
    m_store->setValue(kKeyAutoSwitch, on);
    m_store->sync();
}

bool Settings::toastOnDone() const
{
    return m_store->value(kKeyToastOnDone, true).toBool();
}

void Settings::setToastOnDone(bool on)
{
    m_store->setValue(kKeyToastOnDone, on);
    m_store->sync();
}

QString Settings::logPathOverride() const
{
    return m_store->value(kKeyLogOverride).toString();
}

void Settings::setLogPathOverride(const QString& path)
{
    m_store->setValue(kKeyLogOverride, path);
    m_store->sync();
    emit logSettingsChanged();
}

bool Settings::followLog() const
{
    return m_store->value(kKeyFollowLog, true).toBool();
}

void Settings::setFollowLog(bool on)
{
    m_store->setValue(kKeyFollowLog, on);
    m_store->sync();
    emit logSettingsChanged();
}

QByteArray Settings::windowGeometry() const
{
    return m_store->value(kKeyGeometry).toByteArray();
}

void Settings::saveWindowGeometry(const QByteArray& geometry)
{
    m_store->setValue(kKeyGeometry, geometry);
    m_store->sync();
}

void Settings::resetAll()
{
    const QByteArray geometry = windowGeometry();
    m_store->clear();
    m_store->sync();
    m_store->setValue(kKeyGeometry, geometry); // keep the window placement
    m_store->sync();

    emit themePrefChanged();
    emit backdropChanged();
    emit injectionSettingsChanged();
    emit logSettingsChanged();
}
