#include "core/ProcessFinder.h"

#include <tlhelp32.h>

namespace micp {
namespace {

struct EnumContext
{
    std::wstring keyword;
    std::vector<ProxyTarget> found;
};

bool ProcessEntryForPid(DWORD pid, PROCESSENTRY32W* outEntry)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (entry.th32ProcessID == pid)
            {
                *outEntry = entry;
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

BOOL CALLBACK CollectWindowsProc(HWND hwnd, LPARAM param)
{
    auto* ctx = reinterpret_cast<EnumContext*>(param);
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512];
    const int len = GetWindowTextW(hwnd, title, int(MICP_ARRLEN(title)));
    if (len <= 0)
        return TRUE;
    if (wcsstr(title, ctx->keyword.c_str()) == nullptr)
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid)
        return TRUE;

    std::wstring processName;
    if (!ProcessFinder::IsJavaProcess(pid, &processName))
        return TRUE;

    ProxyTarget target;
    target.pid = pid;
    target.windowTitle.assign(title, size_t(len));
    target.processName = processName;
    ctx->found.push_back(std::move(target));
    return TRUE;
}

} // namespace

bool ProcessFinder::IsJavaProcess(DWORD pid, std::wstring* processName)
{
    PROCESSENTRY32W entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!ProcessEntryForPid(pid, &entry))
        return false;

    std::wstring name = entry.szExeFile;
    for (auto& c : name)
        c = towlower(c);
    const bool java = (name == L"java.exe" || name == L"javaw.exe");
    if (java && processName)
        *processName = name;
    return java;
}

std::vector<ProxyTarget> ProcessFinder::FindWindows(const std::wstring& keyword)
{
    EnumContext ctx;
    ctx.keyword = keyword;
    if (ctx.keyword.empty())
        return {};

    EnumWindows(CollectWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return std::move(ctx.found);
}

ProxyTarget ProcessFinder::NewestMatch(const std::wstring& keyword)
{
    std::vector<ProxyTarget> all = FindWindows(keyword);
    if (all.empty())
        return {};

    // De-duplicate by pid, then pick the target whose process started last.
    std::vector<DWORD> seenPids;
    ProxyTarget best;
    unsigned long long bestCreated = 0;
    for (const ProxyTarget& t : all)
    {
        bool seen = false;
        for (DWORD pid : seenPids)
            if (pid == t.pid)
                seen = true;
        if (seen)
            continue;
        seenPids.push_back(t.pid);

        const unsigned long long created = CreationTime(t.pid);
        if (!best.pid || created > bestCreated)
        {
            best = t;
            bestCreated = created;
        }
    }
    return best;
}

bool ProcessFinder::IsProcessAlive(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        // Access denied still means something is running with that pid.
        return GetLastError() == ERROR_ACCESS_DENIED;
    }
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(process, &code);
    CloseHandle(process);
    return ok && code == STILL_ACTIVE;
}

unsigned long long ProcessFinder::CreationTime(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return 0;

    FILETIME created, exited, kernel, user;
    unsigned long long value = 0;
    if (GetProcessTimes(process, &created, &exited, &kernel, &user))
    {
        ULARGE_INTEGER time;
        time.LowPart = created.dwLowDateTime;
        time.HighPart = created.dwHighDateTime;
        value = time.QuadPart;
    }
    CloseHandle(process);
    return value;
}

} // namespace micp
