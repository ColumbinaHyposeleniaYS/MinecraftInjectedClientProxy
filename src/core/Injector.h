#pragma once
// Thin wrapper around the injectcore engine (DoInjectEx).

#include "common.h"

namespace micp {

struct InjectionResult
{
    bool ok = false;
    bool resumed = false;  // a resident proxy was signalled instead
    bool injected = false; // a fresh DLL load happened
    uint32_t winError = 0;
    std::wstring message;
};

namespace Injector {

bool FileReadable(const std::wstring& dllPath);
InjectionResult Run(DWORD pid, const std::wstring& dllPath);

} // namespace Injector
} // namespace micp
