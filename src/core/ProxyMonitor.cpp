#include "core/ProxyMonitor.h"

#include "core/PortProbe.h"
#include "core/ProcessFinder.h"

namespace micp {

void ProxyMonitor::Configure(const std::wstring& keyword, uint16_t port)
{
    m_keyword = keyword;
    m_port = port;
}

void ProxyMonitor::Observe(DWORD pid)
{
    m_pid = pid;
    m_wasActive = false;
}

void ProxyMonitor::ClearObservation()
{
    m_pid = 0;
    m_wasActive = false;
}

ProxySnapshot ProxyMonitor::Tick()
{
    ProxySnapshot snap;

    const ProxyTarget window = ProcessFinder::NewestMatch(m_keyword);
    snap.windowFound = window.valid();
    snap.windowTitle = window.windowTitle;
    snap.windowPid = window.pid;

    // Auto-adopt: the game is already injected (listener owned by its pid).
    if (m_pid == 0 && snap.windowFound)
    {
        if (PortProbe::IsListening(snap.windowPid, m_port))
        {
            m_pid = snap.windowPid;
            m_wasActive = true;
        }
    }

    if (m_pid != 0)
    {
        snap.observing = true;
        snap.pid = m_pid;
        snap.alive = ProcessFinder::IsProcessAlive(m_pid);
        snap.listening = snap.alive && PortProbe::IsListening(m_pid, m_port);
        snap.bConnections = snap.listening ? PortProbe::EstablishedCount(m_pid, m_port) : 0;
        if (snap.listening)
            m_wasActive = true;
        snap.wasActive = m_wasActive;
    }

    return snap;
}

} // namespace micp
