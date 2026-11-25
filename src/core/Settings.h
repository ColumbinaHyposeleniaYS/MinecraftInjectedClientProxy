#pragma once
// Persisted application settings, stored as an INI file inside
// %APPDATA%\MinecraftInjectedClientProxy (same file layout as earlier
// releases, so existing configs keep working).

#include "app/Lang.h"
#include "app/Theme.h"
#include "common.h"

namespace micp {

struct SettingsData
{
    ThemeMode theme = ThemeMode::System;
    Backdrop backdrop = Backdrop::None;

    // UI language. -1 = never set: resolve from the system locale on load
    // (falling back to zh_CN) and persist from then on.
    int lang = -1;

    std::wstring keyword;   // window title keyword (default 布吉岛)
    int port = 1314;

    bool autoSwitchOnDone = true; // jump to the log page after success
    bool toastOnDone = true;      // toast when a run finishes

    std::wstring logPathOverride; // empty -> auto-detect candidates
    bool followLog = true;        // keep the log view pinned to the bottom
    bool debugLog = false;        // master switch for proxy.log output
    bool clearLogOnStart = true;  // delete the previous log when debug is on

    // window placement (restored on start, saved on close)
    bool hasPlacement = false;
    int winX = 0, winY = 0, winW = 0, winH = 0;
    bool maximized = false;
};

class SettingsStore
{
public:
    static SettingsData Load();
    static void Save(const SettingsData& data);
};

} // namespace micp
