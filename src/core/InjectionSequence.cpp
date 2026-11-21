#include "core/InjectionSequence.h"

#include "core/Injector.h"
#include "core/PortProbe.h"
#include "core/ProcessFinder.h"

#include <QThread>
#include <QTimer>

#include <atomic>

namespace {
constexpr int kWaitPortTimeoutMs = 30000;
constexpr int kPollIntervalMs = 50;
} // namespace

InjectionSequence::InjectionSequence(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<InjectionSequence::State>("InjectionSequence::State");
}

void InjectionSequence::setState(State state)
{
    m_state.store(int(state));
    emit stateChanged(state);
}

void InjectionSequence::start(const Config& config)
{
    if (m_running.exchange(true))
        return; // a run is already in progress

    m_cancel.store(false);
    m_thread = QThread::create([this, config] { run(config); });
    connect(m_thread, &QThread::finished, this, [this] {
        m_running.store(false);
        m_thread = nullptr;
    });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();
}

void InjectionSequence::cancel()
{
    if (m_running.load())
        m_cancel.store(true);
}

void InjectionSequence::shutdown()
{
    m_cancel.store(true);
    if (m_thread && m_thread->isRunning())
        m_thread->wait(5000);
}

void InjectionSequence::finish(State finalState, bool ok, bool resumed, const QString& message)
{
    setState(finalState);
    emit finished(ok, resumed, message);
}

void InjectionSequence::run(const Config& config)
{
    // ---- Phase 1: wait for a matching game window ------------------------
    setState(State::WaitWindow);
    ProxyTarget target;
    for (;;)
    {
        if (m_cancel.load())
        {
            finish(State::Cancelled, false, false, QStringLiteral("已取消注入"));
            return;
        }
        target = ProcessFinder::newestMatch(config.keyword);
        if (target.isValid())
            break;
        Sleep(kPollIntervalMs);
    }

    emit targetFound(target.pid, target.windowTitle);

    // ---- Phase 2: inject / signal resident proxy -------------------------
    setState(State::Injecting);
    const InjectionResult result = Injector::run(target.pid, config.dllPath);

    if (!result.ok)
    {
        finish(State::Failed, false, false, result.message);
        return;
    }

    // ---- Phase 3: wait for the proxy listener (max 30 s) ------------------
    setState(State::WaitPort);
    int elapsed = 0;
    while (elapsed < kWaitPortTimeoutMs)
    {
        if (m_cancel.load())
        {
            finish(State::Cancelled, false, false, QStringLiteral("已取消注入"));
            return;
        }
        if (PortProbe::isListening(target.pid, config.port))
        {
            const QString msg = result.resumed
                ? QStringLiteral("驻留代理已恢复，端口 %1 就绪").arg(config.port)
                : QStringLiteral("注入完成，代理已监听 %1").arg(config.port);
            finish(State::Done, true, result.resumed, msg);
            return;
        }
        Sleep(kPollIntervalMs);
        elapsed += kPollIntervalMs;
        if (elapsed % 250 == 0)
            emit waitPortProgress(elapsed, kWaitPortTimeoutMs);
    }

    finish(State::Failed, false, result.resumed,
           QStringLiteral("注入完成，但 %1 秒内代理未开始监听 %2，请查看日志排查")
               .arg(kWaitPortTimeoutMs / 1000)
               .arg(config.port));
}
