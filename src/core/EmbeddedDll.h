#pragma once
// Embedded Aurora.dll access. When the build embeds the DLL as the
// AURORA_DLL RCDATA resource (MICP_HAS_EMBEDDED_DLL defined), ExtractToTemp
// writes the bytes to a temp file and returns its path. Without the resource
// (MinGW GUI-only builds) it returns an empty string and callers fall back to
// the external DLL search paths.

#include <string>

namespace EmbeddedDll {

// Empty string = no embedded payload (caller treats as file-not-found).
std::wstring ExtractToTemp();

// True when this executable carries the embedded Aurora.dll resource.
bool Available();

}
