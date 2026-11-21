#pragma once
// Persisted application settings, stored as an INI file inside
// %APPDATA%\MinecraftInjectedClientProxy (same file layout as earlier
// releases, so existing configs keep working).

#include "app/Theme.h"
#include "common.h"

namespace micp {

struct SettingsData
{
    ThemeMode theme = ThemeMode::System;
    Backdrop backdrop = Backdrop::None;

    std::wstring dllPath;   // user-chosen override; empty -> auto-detect
    std::wstring keyword;   // window title keyword (default 布吉岛)
    int port = 25565;

    bool autoSwitchOnDone = true; // jump to the log page after success
    bool toastOnDone = true;      // toast when a run finishes

    std::wstring logPathOverride; // empty -> auto-detect candidates
    bool followLog = true;        // keep the log view pinned to the bottom

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
