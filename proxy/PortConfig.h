#pragma once
// Port configuration shared by the injected proxy DLL.
//
// The GUI persists the chosen proxy port into
// %APPDATA%\MinecraftInjectedClientProxy\settings.ini under [inject] port.
// The DLL reads the same INI so its actual bind port matches what the GUI
// waits for. Historically the DLL used a hard-coded 25565 while the GUI
// waited on a configurable port, so changing the port in the UI never took
// effect and a port collision could never be fixed from the UI.
//
// This header is intentionally dependency-free (only windows.h + jni's jint
// substitute) so it can be included from both the native core and the GUI
// side without pulling in the JNI headers.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdlib>
#include <cstdio>
#include <cwchar>

namespace proxy_port {

// Fallback used only when the INI is missing or contains an invalid value.
constexpr int kDefaultPort = 1314;

// Reads [inject] port from the GUI settings.ini. Returns kDefaultPort when
// the value is absent, out of range, or the INI cannot be located.
//
// The GUI writes this file with WritePrivateProfileStringW (wide chars). Use
// the wide-char environment/path and GetPrivateProfileIntW too, so a username
// containing non-ASCII characters (a non-ASCII %APPDATA% path) resolves to the
// same file; the ANSI A-variant would silently fall back to the default port there.
inline int ReadConfiguredPort() {
    const wchar_t* appdata = _wgetenv(L"APPDATA");
    if (!appdata)
        return kDefaultPort;
    wchar_t ini[MAX_PATH] = {0};
    const int written =
        swprintf(ini, MAX_PATH, L"%s\\MinecraftInjectedClientProxy\\settings.ini", appdata);
    if (written < 0 || written >= MAX_PATH)
        return kDefaultPort;
    int port = GetPrivateProfileIntW(L"inject", L"port", kDefaultPort, ini);
    if (port < 1 || port > 65535)
        return kDefaultPort;
    return port;
}

} // namespace proxy_port
