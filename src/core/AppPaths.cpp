#include "core/AppPaths.h"

#include <shlobj.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

namespace micp::AppPaths {

std::wstring AppDir()
{
    wchar_t path[MAX_PATH * 2] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MICP_ARRLEN(path));
    if (len == 0 || len >= MICP_ARRLEN(path))
        return L".";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
        return L".";
    *slash = 0;
    return path;
}

std::wstring SettingsDir()
{
    wchar_t base[MAX_PATH * 2] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, base)))
        return AppDir();
    std::wstring dir = std::wstring(base) + L"\\MinecraftInjectedClientProxy";
    CreateDirectoryW(dir.c_str(), nullptr); // ok if it already exists
    return dir;
}

std::wstring SettingsIni()
{
    return SettingsDir() + L"\\settings.ini";
}

std::vector<std::wstring> LogCandidates()
{
    std::vector<std::wstring> out;
    wchar_t appData[MAX_PATH * 2] = {};
    wchar_t temp[MAX_PATH * 2] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
        out.push_back(std::wstring(appData) + L"\\.minecraft\\proxy.log");
    if (GetEnvironmentVariableW(L"TEMP", temp, MICP_ARRLEN(temp)) > 0)
        out.push_back(std::wstring(temp) + L"\\MinecraftProxy.log");
    out.push_back(L"C:\\MinecraftProxy.log");
    return out;
}

std::vector<std::wstring> LanIpAddresses()
{
    std::vector<std::wstring> out;

    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER
                        | GAA_FLAG_SKIP_FRIENDLY_NAME;
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW
        || size == 0)
        return out;

    std::vector<BYTE> buffer(size);
    auto* adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR)
        return out;

    for (auto* adapter = adapters; adapter; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;
        for (auto* addr = adapter->FirstUnicastAddress; addr; addr = addr->Next)
        {
            if (addr->Address.lpSockaddr == nullptr
                || addr->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            const auto* sin = reinterpret_cast<const sockaddr_in*>(addr->Address.lpSockaddr);
            const uint32_t ip = ntohl(sin->sin_addr.S_un.S_addr);
            if (ip == INADDR_LOOPBACK || (ip >> 28) == 0xE) // skip 127.0.0.1 and multicast
                continue;
            char text[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &sin->sin_addr, text, sizeof(text)))
            {
                std::wstring wide(text, text + strlen(text));
                bool dup = false;
                for (const auto& existing : out)
                    if (existing == wide)
                        dup = true;
                if (!dup)
                    out.push_back(std::move(wide));
            }
        }
    }
    return out;
}

std::wstring DefaultKeyword()
{
    return L"\u5e03\u5409\u5c9b"; // 布吉岛
}

} // namespace micp::AppPaths
