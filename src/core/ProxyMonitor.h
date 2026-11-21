#pragma once
// Periodic proxy health snapshot: is the game window around, is the injected
// proxy still listening, how many B clients are attached.

#include "common.h"

namespace micp {

struct ProxySnapshot
{
    bool windowFound = false;
    std::wstring windowTitle;
    DWORD windowPid = 0;

    bool observing = false;  // we have adopted a pid to watch
    DWORD pid = 0;
    bool alive = false;
    bool listening = false;
    bool wasActive = false;  // saw the listener up at least once
    int bConnections = 0;
};

class ProxyMonitor
{
public:
    void Configure(const std::wstring& keyword, uint16_t port);
    void Observe(DWORD pid);
    void ClearObservation();

    ProxySnapshot Tick();

private:
    std::wstring m_keyword;
    uint16_t m_port = 25565;
    DWORD m_pid = 0;
    bool m_wasActive = false;
};

} // namespace micp
