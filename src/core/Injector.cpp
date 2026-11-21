#include "core/Injector.h"

#include "core/injectcore/Inject.h"

namespace micp {
namespace {

// Map the engine's English diagnostics to actionable Chinese messages.
std::wstring TranslateEngineMessage(const std::string& raw, uint32_t winError)
{
    auto has = [&raw](const char* needle) { return raw.find(needle) != std::string::npos; };

    if (has("Failed to open the DLL file"))
        return L"无法打开 DLL 文件，请检查路径是否有效";
    if (has("Failed to get the DLL file size"))
        return L"无法读取 DLL 文件大小，文件可能已损坏";
    if (has("Failed to read the DLL file"))
        return L"读取 DLL 文件失败";
    if (has("Failed to open the target process"))
        return L"打开目标进程失败（游戏可能以管理员身份运行，请以管理员身份重启本程序）";
    if (has("Failed to inject the DLL"))
        return FormatStr(L"注入 DLL 失败（错误码 %u），请确认目标进程与系统版本匹配", winError);
    if (has("Could not signal resident proxy"))
        return FormatStr(L"向驻留代理发送恢复信号失败（错误码 %u）", winError);
    if (has("Reactivation requested"))
        return L"已通知驻留代理恢复运行（未重新加载磁盘 DLL）";
    if (has("Injected the DLL"))
        return L"DLL 注入成功";
    if (!raw.empty())
    {
        std::wstring wide(raw.begin(), raw.end());
        return wide;
    }
    return L"未知错误";
}

} // namespace

namespace Injector {

bool FileReadable(const std::wstring& dllPath)
{
    if (TrimW(dllPath).empty())
        return false;
    const DWORD attr = GetFileAttributesW(dllPath.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

InjectionResult Run(DWORD pid, const std::wstring& dllPath)
{
    InjectionResult result;

    if (!FileReadable(dllPath))
    {
        result.message = FormatStr(L"找不到代理 DLL：%s", dllPath.c_str());
        return result;
    }

    // The engine takes an ANSI path; convert via the active code page the same
    // way previous releases did (user paths with Chinese characters work).
    const int bytes = WideCharToMultiByte(CP_ACP, 0, dllPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string ansiPath;
    if (bytes > 0)
    {
        ansiPath.resize(size_t(bytes));
        WideCharToMultiByte(CP_ACP, 0, dllPath.c_str(), -1, &ansiPath[0], bytes, nullptr, nullptr);
        // strip the terminating NUL written into the std::string
        while (!ansiPath.empty() && ansiPath.back() == '\0')
            ansiPath.pop_back();
    }
    if (ansiPath.empty())
    {
        result.message = L"路径转换失败";
        return result;
    }

    char message[1024] = {0};
    BOOL resumed = FALSE;
    const BOOL ok = DoInjectEx(pid, ansiPath.c_str(), message, int(sizeof(message)), &resumed);

    result.ok = (ok == TRUE);
    result.resumed = (resumed == TRUE);
    result.injected = result.ok && !result.resumed;

    const std::string raw(message);
    uint32_t winError = 0;
    const size_t idx = raw.rfind("Error=");
    if (idx != std::string::npos)
        winError = uint32_t(atoi(raw.c_str() + idx + 6));

    result.winError = winError;
    result.message = TranslateEngineMessage(raw, winError);
    return result;
}

} // namespace Injector
} // namespace micp
