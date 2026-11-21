#pragma once
// Locates the target Minecraft client window/process by title keyword.

#include "common.h"
#include <string>
#include <vector>

namespace micp {

struct ProxyTarget
{
    DWORD pid = 0;
    std::wstring windowTitle;
    std::wstring processName;

    bool valid() const { return pid != 0; }
};

class ProcessFinder
{
public:
    static bool IsJavaProcess(DWORD pid, std::wstring* processName = nullptr);

    static std::vector<ProxyTarget> FindWindows(const std::wstring& keyword);

    // De-duplicated by pid, newest process creation time wins.
    static ProxyTarget NewestMatch(const std::wstring& keyword);

    static bool IsProcessAlive(DWORD pid);
    static unsigned long long CreationTime(DWORD pid);
};

} // namespace micp
