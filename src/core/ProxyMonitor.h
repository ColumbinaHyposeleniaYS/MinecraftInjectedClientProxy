#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

/*
 * Everything the overview page needs to know about the world right now:
 * is there a matching game window, is the observed proxy alive / listening,
 * and how many B clients are attached.
 */
struct ProxySnapshot
{
    bool windowFound = false;   // a game window matching the keyword exists
    QString windowTitle;
    DWORD windowPid = 0;

    bool observing = false;     // we have a target pid under observation
    DWORD pid = 0;
    bool alive = false;
    bool listening = false;
    int bConnections = 0;
    bool wasActive = false;     // listener seen at least once since observe()
};

/*
 * Polls the machine once a second on the GUI thread. Also auto-adopts an
 * already-injected game (matching window + listener on the proxy port) so
 * the UI shows "running" immediately after the app starts.
 */
class ProxyMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ProxyMonitor(QObject* parent = nullptr);

    void setKeyword(const QString& keyword);
    void setPort(quint16 port);
    void observe(DWORD pid);    // focus on an explicit target (post injection)
    void clearObservation();

signals:
    void snapshotUpdated(const ProxySnapshot& snapshot);

private slots:
    void tick();

private:
    QTimer m_timer;
    QString m_keyword;
    quint16 m_port = 25565;
    DWORD m_pid = 0;
    bool m_wasActive = false;
};
