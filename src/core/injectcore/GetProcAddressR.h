#pragma once
#ifndef _REFLECTIVEDLLINJECTION_GETPROCADDRESSR_H
#define _REFLECTIVEDLLINJECTION_GETPROCADDRESSR_H

#include "ReflectiveDLLInjection.h"

#ifdef __cplusplus
extern "C" {
#endif

FARPROC WINAPI GetProcAddressR( HANDLE hModule, LPCSTR lpProcName );

#ifdef __cplusplus
}
#endif

#endif
