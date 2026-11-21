#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSettings;

/*
 * Persisted application settings. Stored as an INI file inside
 * %APPDATA%\MinecraftInjectedClientProxy so everything is portable and
 * easy to reset.
 */
class Settings : public QObject
{
    Q_OBJECT

public:
    enum class ThemePref { System, Light, Dark };
    enum class Backdrop { None, Mica, Acrylic };

    static Settings* instance();

    ThemePref themePref() const;
    void setThemePref(ThemePref pref);

    Backdrop backdrop() const;
    void setBackdrop(Backdrop backdrop);

    QString dllPath() const;
    void setDllPath(const QString& path);

    QString windowKeyword() const;
    void setWindowKeyword(const QString& keyword);

    bool autoSwitchOnDone() const;
    void setAutoSwitchOnDone(bool on);

    bool toastOnDone() const;
    void setToastOnDone(bool on);

    QString logPathOverride() const;
    void setLogPathOverride(const QString& path); // empty -> auto-detect

    bool followLog() const;
    void setFollowLog(bool on);

    QByteArray windowGeometry() const;
    void saveWindowGeometry(const QByteArray& geometry);

    void resetAll(); // restore factory defaults (keeps window geometry)

signals:
    void themePrefChanged();
    void backdropChanged();
    void injectionSettingsChanged();
    void logSettingsChanged();

private:
    explicit Settings();

    QSettings* m_store;
};
