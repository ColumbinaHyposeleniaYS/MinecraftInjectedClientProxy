#pragma once

#include <QString>
#include <QStringList>

/*
 * Well-known locations used by the application: settings directory, proxy log
 * file candidates (probed in order), default DLL lookup paths and the LAN
 * addresses B clients can connect to.
 */
class AppPaths
{
public:
    static QString appDir();              // directory of the running .exe
    static QString settingsDir();         // %APPDATA%\MinecraftInjectedClientProxy
    static QString settingsIni();         // settings.ini inside settingsDir()

    // Proxy log candidates, most preferred first (matches engine fallback order).
    static QStringList logCandidates();

    // Where we look for the proxy DLL (Aurora.dll) if the user never chose one.
    static QStringList defaultDllCandidates();

    // Non-loopback IPv4 addresses of this machine, for the "B 如何连接" hint.
    static QStringList lanIpAddresses();

    static constexpr quint16 defaultPort() { return 25565; }

    static QString defaultKeyword();      // 布吉岛
};
