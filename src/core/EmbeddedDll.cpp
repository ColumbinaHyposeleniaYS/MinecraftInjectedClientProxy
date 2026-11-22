#include "core/EmbeddedDll.h"

#include <windows.h>

namespace EmbeddedDll {
namespace {

constexpr wchar_t kResourceName[] = L"AURORA_DLL";

}

std::wstring ExtractToTemp()
{
#ifndef MICP_HAS_EMBEDDED_DLL
    (void)kResourceName;
    return {};
#else
    HRSRC found = FindResourceW(nullptr, kResourceName, RT_RCDATA);
    if (!found)
        return {};
    HGLOBAL handle = LoadResource(nullptr, found);
    if (!handle)
        return {};
    const void* data = LockResource(handle);
    const DWORD size = SizeofResource(nullptr, found);
    if (!data || size == 0)
        return {};

    wchar_t tempDir[MAX_PATH] = {0};
    if (!GetTempPathW(MAX_PATH, tempDir))
        return {};

    wchar_t tempFile[MAX_PATH] = {0};
    if (!GetTempFileNameW(tempDir, L"micp", 0, tempFile))
        return {};

    HANDLE file = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    if (!ok || written != size)
        return {};

    // The reflective loader takes an ANSI path; a .dll extension is expected.
    std::wstring path(tempFile);
    const size_t dot = path.rfind(L'.');
    if (dot != std::wstring::npos)
        path = path.substr(0, dot);
    path += L".dll";
    if (!MoveFileW(tempFile, path.c_str()))
        return {};
    return path;
#endif
}

bool Available()
{
#ifdef MICP_HAS_EMBEDDED_DLL
    HRSRC found = FindResourceW(nullptr, kResourceName, RT_RCDATA);
    return found != nullptr;
#else
    (void)kResourceName;
    return false;
#endif
}

} // namespace EmbeddedDll
