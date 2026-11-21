#pragma once
// Tails the proxy log file (proxy.log). The UI polls this every 500 ms;
// candidates are probed in order and the active source is switched
// automatically when files appear/disappear.

#include "common.h"

namespace micp {

class LogTail
{
public:
    struct Result
    {
        bool reset = false;            // the source changed / was reloaded
        std::wstring source;           // empty when nothing is active
        std::vector<std::wstring> lines;
    };

    // overridePath: explicit user choice (may not exist yet); when empty the
    // auto-detect candidates are probed.
    Result Poll(const std::wstring& overridePath);
    void Rescan(const std::wstring& overridePath);

private:
    void PickActive(const std::wstring& overridePath);

    HANDLE m_file = nullptr; // INVALID_HANDLE_VALUE when closed
    std::wstring m_active;
    unsigned long long m_pos = 0;
    std::string m_pending;
};

} // namespace micp
