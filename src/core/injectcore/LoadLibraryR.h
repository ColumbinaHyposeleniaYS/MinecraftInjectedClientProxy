#pragma once
#ifndef _REFLECTIVEDLLINJECTION_LOADLIBRARYR_H
#define _REFLECTIVEDLLINJECTION_LOADLIBRARYR_H

#include "ReflectiveDLLInjection.h"

#ifdef __cplusplus
extern "C" {
#endif

DWORD   WINAPI Rva2Offset( DWORD dwRva, UINT_PTR uiBaseAddress );
DWORD   WINAPI GetReflectiveLoaderOffset( VOID * lpReflectiveDllBuffer );
HMODULE WINAPI LoadLibraryR( LPVOID lpBuffer, DWORD dwLength );
HANDLE  WINAPI LoadRemoteLibraryR( HANDLE hProcess, LPVOID lpBuffer, DWORD dwLength, LPVOID lpParameter );

#ifdef __cplusplus
}
#endif

#endif
