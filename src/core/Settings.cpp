#include "core/Settings.h"

#include "core/AppPaths.h"

namespace micp {
namespace {

const wchar_t* kSectionUi = L"appearance";
const wchar_t* kSectionInject = L"injection";
const wchar_t* kSectionLog = L"logs";
const wchar_t* kSectionWindow = L"window";

std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback)
{
    wchar_t buf[1024] = {};
    GetPrivateProfileStringW(section, key, fallback ? fallback : L"", buf, MICP_ARRLEN(buf),
                             AppPaths::SettingsIni().c_str());
    return buf;
}

int ReadInt(const wchar_t* section, const wchar_t* key, int fallback)
{
    return GetPrivateProfileIntW(section, key, fallback, AppPaths::SettingsIni().c_str());
}

void WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value)
{
    WritePrivateProfileStringW(section, key, value.c_str(), AppPaths::SettingsIni().c_str());
}

void WriteInt(const wchar_t* section, const wchar_t* key, int value)
{
    WriteString(section, key, std::to_wstring(value));
}

} // namespace

SettingsData SettingsStore::Load()
{
    SettingsData d;

    d.theme = static_cast<ThemeMode>(ReadInt(kSectionUi, L"themePref", 0));
    if (d.theme != ThemeMode::System && d.theme != ThemeMode::Light && d.theme != ThemeMode::Dark)
        d.theme = ThemeMode::System;

    const int backdrop = ReadInt(kSectionUi, L"backdrop", 0);
    d.backdrop = backdrop == 1 ? Backdrop::Mica
               : backdrop == 2 ? Backdrop::Acrylic
                               : Backdrop::None;

    // Language: never-set (-1) resolves from the system locale so the app
    // follows the user's Windows on first run, then sticks to the INI value.
    d.lang = ReadInt(kSectionUi, L"lang", -1);
    if (d.lang < 0 || d.lang >= lang::kLangCount)
    {
        d.lang = int(lang::DetectSystemLang());
        // Not marked dirty here; the value persists on the next Save anyway.
    }


    d.keyword = ReadString(kSectionInject, L"windowKeyword", L"");
    if (TrimW(d.keyword).empty())
        d.keyword = AppPaths::DefaultKeyword();

    d.port = ReadInt(kSectionInject, L"port", AppPaths::DefaultPort());
    if (d.port < 1 || d.port > 65535)
        d.port = AppPaths::DefaultPort();

    d.autoSwitchOnDone = ReadInt(kSectionInject, L"autoSwitchOnDone", 1) != 0;
    d.toastOnDone = ReadInt(kSectionInject, L"toastOnDone", 1) != 0;

    d.logPathOverride = ReadString(kSectionLog, L"pathOverride", L"");
    d.followLog = ReadInt(kSectionLog, L"follow", 1) != 0;

    d.winX = ReadInt(kSectionWindow, L"x", 0);
    d.winY = ReadInt(kSectionWindow, L"y", 0);
    d.winW = ReadInt(kSectionWindow, L"w", 0);
    d.winH = ReadInt(kSectionWindow, L"h", 0);
    d.maximized = ReadInt(kSectionWindow, L"maximized", 0) != 0;
    d.hasPlacement = (d.winW >= 400 && d.winH >= 300);

    return d;
}

void SettingsStore::Save(const SettingsData& d)
{
    WriteInt(kSectionUi, L"themePref", static_cast<int>(d.theme));
    WriteInt(kSectionUi, L"backdrop", d.backdrop == Backdrop::Mica ? 1
                                  : d.backdrop == Backdrop::Acrylic ? 2 : 0);
    WriteInt(kSectionUi, L"lang", d.lang);
    WriteString(kSectionInject, L"windowKeyword", d.keyword);
    WriteInt(kSectionInject, L"port", d.port);
    WriteInt(kSectionInject, L"autoSwitchOnDone", d.autoSwitchOnDone ? 1 : 0);
    WriteInt(kSectionInject, L"toastOnDone", d.toastOnDone ? 1 : 0);
    WriteString(kSectionLog, L"pathOverride", d.logPathOverride);
    WriteInt(kSectionLog, L"follow", d.followLog ? 1 : 0);

    WriteInt(kSectionWindow, L"x", d.winX);
    WriteInt(kSectionWindow, L"y", d.winY);
    WriteInt(kSectionWindow, L"w", d.winW);
    WriteInt(kSectionWindow, L"h", d.winH);
    WriteInt(kSectionWindow, L"maximized", d.maximized ? 1 : 0);
}

} // namespace micp
