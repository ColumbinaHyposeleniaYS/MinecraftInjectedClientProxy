#pragma once

#include <QString>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

/*
 * Thin C++ wrapper around the injectcore engine. Blocking; callers are
 * expected to run it off the UI thread (see InjectionSequence).
 */
struct InjectionResult
{
    bool ok = false;
    bool resumed = false;  // resident proxy was signalled, nothing new loaded
    bool injected = false; // a fresh DLL was actually loaded into the target
    QString message;       // human readable (Chinese) summary
    quint32 winError = 0;  // GetLastError of the failing engine step, if any
};

namespace Injector {

// Validates that the DLL file exists and is readable.
bool fileReadable(const QString& dllPath);

// Runs the full injection/reactivation for one process. Blocking.
InjectionResult run(DWORD pid, const QString& dllPath);

} // namespace Injector
