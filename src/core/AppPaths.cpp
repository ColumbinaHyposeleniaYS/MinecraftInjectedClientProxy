#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

QString AppPaths::appDir()
{
    return QCoreApplication::applicationDirPath();
}

QString AppPaths::settingsDir()
{
    const QString appData = qEnvironmentVariable("APPDATA");
    QString base;
    if (!appData.isEmpty())
        base = appData + QStringLiteral("/MinecraftInjectedClientProxy");
    else
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base;
}

QString AppPaths::settingsIni()
{
    return settingsDir() + QStringLiteral("/settings.ini");
}

QStringList AppPaths::logCandidates()
{
    QStringList out;
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString temp = qEnvironmentVariable("TEMP");
    if (!appData.isEmpty())
        out << (appData + QStringLiteral("/.minecraft/proxy.log"));
    if (!temp.isEmpty())
        out << (temp + QStringLiteral("/MinecraftProxy.log"));
    out << QStringLiteral("C:/MinecraftProxy.log");
    return out;
}

QStringList AppPaths::defaultDllCandidates()
{
    QStringList out;
    const QString dir = appDir();
    out << (dir + QStringLiteral("/Aurora.dll"))
        << (dir + QStringLiteral("/dll/Aurora.dll"))
        << (dir + QStringLiteral("/build/Aurora.dll"))
        << (dir + QStringLiteral("/../build/Aurora.dll"))
        << QStringLiteral("Aurora.dll");
    return out;
}

QStringList AppPaths::lanIpAddresses()
{
    QStringList out;

    ULONG size = 0;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER
                  | GAA_FLAG_SKIP_FRIENDLY_NAME;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW
        || size == 0)
        return out;

    QByteArray buffer(int(size), Qt::Uninitialized);
    auto* adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR)
        return out;

    for (auto* adapter = adapters; adapter; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;
        for (auto* addr = adapter->FirstUnicastAddress; addr; addr = addr->Next)
        {
            if (addr->Address.lpSockaddr == nullptr
                || addr->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            const auto* sin = reinterpret_cast<const sockaddr_in*>(addr->Address.lpSockaddr);
            const quint32 ip = ntohl(sin->sin_addr.S_un.S_addr);
            if (ip == INADDR_LOOPBACK || (ip >> 28) == 0xE) // skip 127.0.0.1 and 224+ multicast
                continue;
            char text[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &sin->sin_addr, text, sizeof(text)))
                out << QString::fromLatin1(text);
        }
    }
    out.removeDuplicates();
    return out;
}

QString AppPaths::defaultKeyword()
{
    return QStringLiteral("\u5e03\u5409\u5c9b"); // 布吉岛
}
