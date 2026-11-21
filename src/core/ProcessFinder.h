#pragma once

#include <QList>
#include <QString>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

/*
 * Locates the game window to inject into: a visible top-level window whose
 * title contains the configured keyword and whose process is java/javaw.
 * All functions are stateless WinAPI calls and safe to use from any thread.
 */
struct ProxyTarget
{
    DWORD pid = 0;
    QString windowTitle;
    QString processName; // java.exe / javaw.exe

    bool isValid() const { return pid != 0; }
};

class ProcessFinder
{
public:
    static bool isJavaProcess(DWORD pid, QString* processName = nullptr);
    static QList<ProxyTarget> findWindows(const QString& keyword);
    static ProxyTarget newestMatch(const QString& keyword); // newest process wins
    static bool isProcessAlive(DWORD pid);
    static ULONGLONG creationTime(DWORD pid); // 0 on failure
};
