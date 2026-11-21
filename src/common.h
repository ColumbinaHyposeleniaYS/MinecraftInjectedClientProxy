#pragma once
// Shared prelude: windows headers with a sane configuration, plus the small
// set of STL types used across the project. Every translation unit includes
// this first (the injectcore C files keep their own guards).

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h> // must precede windows.h
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#define MICP_ARRLEN(a) (sizeof(a) / sizeof((a)[0]))

namespace micp {

// UTF-16 printf-style formatting into a stack buffer (swprintf is C99 and
// available on both MSVC and MinGW).
std::wstring FormatStr(const wchar_t* fmt, ...);

// True when the string contains at least one CJK codepoint (the UI picks the
// Microsoft YaHei UI face for such runs so glyphs never fall back to tofu).
bool HasCJK(const std::wstring& s);

std::wstring TrimW(const std::wstring& s);

bool FileExistsW(const std::wstring& path);

} // namespace micp
