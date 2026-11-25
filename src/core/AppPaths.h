#pragma once
// Well-known locations: settings directory, proxy log file candidates
// (probed in order), default DLL lookup paths and the LAN addresses B
// clients can connect to.

#include "common.h"

namespace micp::AppPaths {

std::wstring AppDir();          // directory of the running .exe
std::wstring SettingsDir();     // %APPDATA%\MinecraftInjectedClientProxy
std::wstring SettingsIni();     // settings.ini inside SettingsDir()

// Proxy log candidates, most preferred first (matches engine fallback order).
std::vector<std::wstring> LogCandidates();


// Non-loopback IPv4 addresses of this machine, for the "B 如何连接" hint.
std::vector<std::wstring> LanIpAddresses();

constexpr uint16_t DefaultPort() { return 1314; }
std::wstring DefaultKeyword(); // 布吉岛

} // namespace micp::AppPaths
