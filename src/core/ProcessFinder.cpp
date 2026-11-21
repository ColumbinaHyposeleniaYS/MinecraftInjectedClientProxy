#include "core/ProcessFinder.h"

#include <tlhelp32.h>

namespace {

struct EnumContext
{
    std::wstring keyword;
    QList<ProxyTarget> found;
};

bool processEntryForPid(DWORD pid, PROCESSENTRY32W* outEntry)
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

BOOL CALLBACK collectWindowsProc(HWND hwnd, LPARAM param)
{
    auto* ctx = reinterpret_cast<EnumContext*>(param);
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512];
    const int len = GetWindowTextW(hwnd, title, int(sizeof(title) / sizeof(title[0])));
    if (len <= 0)
        return TRUE;
    if (wcsstr(title, ctx->keyword.c_str()) == nullptr)
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid)
        return TRUE;

    QString processName;
    if (!ProcessFinder::isJavaProcess(pid, &processName))
        return TRUE;

    ProxyTarget target;
    target.pid = pid;
    target.windowTitle = QString::fromWCharArray(title, len);
    target.processName = processName;
    ctx->found.append(target);
    return TRUE;
}

} // namespace

bool ProcessFinder::isJavaProcess(DWORD pid, QString* processName)
{
    PROCESSENTRY32W entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!processEntryForPid(pid, &entry))
        return false;

    const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
    const bool java = (name == QStringLiteral("java.exe") || name == QStringLiteral("javaw.exe"));
    if (java && processName)
        *processName = name;
    return java;
}

QList<ProxyTarget> ProcessFinder::findWindows(const QString& keyword)
{
    EnumContext ctx;
    ctx.keyword = keyword.toStdWString();
    if (ctx.keyword.empty())
        return {};

    EnumWindows(collectWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

ProxyTarget ProcessFinder::newestMatch(const QString& keyword)
{
    const QList<ProxyTarget> all = findWindows(keyword);
    if (all.isEmpty())
        return {};

    // De-duplicate by pid, then pick the target whose process started last.
    QList<DWORD> seenPids;
    ProxyTarget best;
    ULONGLONG bestCreated = 0;
    for (const ProxyTarget& t : all)
    {
        if (seenPids.contains(t.pid))
            continue;
        seenPids.append(t.pid);
        const ULONGLONG created = creationTime(t.pid);
        if (!best.pid || created > bestCreated)
        {
            best = t;
            bestCreated = created;
        }
    }
    return best;
}

bool ProcessFinder::isProcessAlive(DWORD pid)
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

ULONGLONG ProcessFinder::creationTime(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return 0;

    FILETIME created, exited, kernel, user;
    ULONGLONG value = 0;
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
