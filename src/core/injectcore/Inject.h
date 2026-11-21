#pragma once
#ifndef MICP_INJECTCORE_INJECT_H
#define MICP_INJECTCORE_INJECT_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Loads the reflective proxy DLL into the target process.
 *
 * If a proxy is already resident in the target process, no new DLL is loaded;
 * instead the resident instance is signalled to resume, and *outResumed is
 * set to TRUE. A rebuilt DLL on disk only takes effect after the target
 * process restarts.
 *
 * Returns TRUE on success. outMessage receives a short engine message.
 */
BOOL DoInjectEx(DWORD dwProcessId, const char* cpDllFile, char* outMessage, int maxLen, BOOL* outResumed);

#ifdef __cplusplus
}
#endif

#endif
