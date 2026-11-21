#include "core/ProxyMonitor.h"

#include "core/PortProbe.h"
#include "core/ProcessFinder.h"

ProxyMonitor::ProxyMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &ProxyMonitor::tick);
    m_timer.start();
}

void ProxyMonitor::setKeyword(const QString& keyword)
{
    m_keyword = keyword;
}

void ProxyMonitor::setPort(quint16 port)
{
    m_port = port;
}

void ProxyMonitor::observe(DWORD pid)
{
    m_pid = pid;
    m_wasActive = false;
}

void ProxyMonitor::clearObservation()
{
    m_pid = 0;
    m_wasActive = false;
}

void ProxyMonitor::tick()
{
    ProxySnapshot snap;

    const ProxyTarget window = ProcessFinder::newestMatch(m_keyword);
    snap.windowFound = window.isValid();
    snap.windowTitle = window.windowTitle;
    snap.windowPid = window.pid;

    // Auto-adopt: the game is already injected (listener owned by its pid).
    if (m_pid == 0 && snap.windowFound)
    {
        if (PortProbe::isListening(snap.windowPid, m_port))
        {
            m_pid = snap.windowPid;
            m_wasActive = true;
        }
    }

    if (m_pid != 0)
    {
        snap.observing = true;
        snap.pid = m_pid;
        snap.alive = ProcessFinder::isProcessAlive(m_pid);
        snap.listening = snap.alive && PortProbe::isListening(m_pid, m_port);
        snap.bConnections = snap.listening ? PortProbe::establishedCount(m_pid, m_port) : 0;
        if (snap.listening)
            m_wasActive = true;
        snap.wasActive = m_wasActive;
    }

    emit snapshotUpdated(snap);
}
