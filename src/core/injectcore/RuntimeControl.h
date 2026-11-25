#pragma once

/*
 * Reactivation signalling for a proxy DLL that is already resident inside the
 * target process. A resident proxy exposes a named auto-reset event; setting
 * it asks the proxy to resume (rebind the local listener and re-announce).
 *
 * Two prefixes are signalled, in order:
 *   - the current product prefix
 *   - a legacy prefix, kept so that DLLs built from older sources still
 *     understand the reactivation request
 */

#include <windows.h>
#include <stdio.h>

// Opaque, product-neutral event names: a hostile module can enumerate local
// kernel objects and match on the old "MinecraftInjectedClientProxy" prefix.
// GUI and injected DLL share this header, so both sides stay in sync.
#define MICP_RESUME_PREFIX         "Local\\{7f3a9c21-4e8b-4d6a-9f12-8b5c0d7e2a41}.r"
#define MICP_RESUME_PREFIX_LEGACY  "Local\\SakuraTools.Resume.v1."
#define MICP_DISCONNECT_PREFIX     "Local\\{7f3a9c21-4e8b-4d6a-9f12-8b5c0d7e2a41}.d"

/* 1: resident signalled, 0: no resident, -1: access/signalling failure. */
static int TryResumeProxyWithPrefix(const char* cpPrefix, DWORD dwProcessId)
{
    char   cName[96];
    HANDLE hEvent = NULL;

    snprintf(cName, sizeof(cName), "%s%lu", cpPrefix, (unsigned long)dwProcessId);

    hEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, cName);
    if (!hEvent)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;

    if (!SetEvent(hEvent))
    {
        CloseHandle(hEvent);
        return -1;
    }

    CloseHandle(hEvent);
    return 1;
}

static int TryResumeProxy(DWORD dwProcessId)
{
    int iResult = TryResumeProxyWithPrefix(MICP_RESUME_PREFIX, dwProcessId);
    if (iResult != 0)
        return iResult;
    return TryResumeProxyWithPrefix(MICP_RESUME_PREFIX_LEGACY, dwProcessId);
}

/* 1: resident signalled, 0: no resident, -1: access/signalling failure. */
static int TryDisconnectProxy(DWORD dwProcessId)
{
    return TryResumeProxyWithPrefix(MICP_DISCONNECT_PREFIX, dwProcessId);
}
