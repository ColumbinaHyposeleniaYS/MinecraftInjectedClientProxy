#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>

#include <atomic>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

class QThread;

/*
 * Drives one complete injection run off the UI thread:
 *
 *   WaitWindow -> Injecting -> WaitPort -> Done
 *
 * Each phase can be cancelled; progress is reported through queued signals.
 * The sequence is a one-shot object driven by start(); it owns a worker
 * thread that is created per run.
 */
class InjectionSequence : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, WaitWindow, Injecting, WaitPort, Done, Failed, Cancelled };
    Q_ENUM(State)

    struct Config
    {
        QString dllPath;
        QString keyword;
        quint16 port = 25565;
    };

    explicit InjectionSequence(QObject* parent = nullptr);

    bool isRunning() const { return m_running; }
    State state() const { return static_cast<State>(m_state.load()); }

    void start(const Config& config);
    void cancel();
    void shutdown(); // cancel + brief join, called on app quit

signals:
    void stateChanged(InjectionSequence::State state);
    void targetFound(quint32 pid, const QString& windowTitle);
    void waitPortProgress(int elapsedMs, int totalMs);
    void finished(bool ok, bool resumed, const QString& message);

private:
    void run(const Config& config);
    void setState(State state);
    void finish(State finalState, bool ok, bool resumed, const QString& message);

    std::atomic_bool m_running{false};
    std::atomic_bool m_cancel{false};
    std::atomic<int> m_state{int(State::Idle)};
    QThread* m_thread = nullptr; // only touched from the GUI thread
};

Q_DECLARE_METATYPE(InjectionSequence::State)
