#include "core/InjectionSequence.h"

#include "core/Injector.h"
#include "core/PortProbe.h"
#include "core/ProcessFinder.h"

namespace micp {
namespace {
constexpr int kWaitPortTimeoutMs = 30000;
constexpr int kPollIntervalMs = 50;
} // namespace

bool InjectionSequence::Start(HWND notify, const Config& config)
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return false; // a run is already in progress

    m_cancel.store(false);
    m_hwnd = notify;
    m_cfg = config;
    {
        Progress fresh;
        fresh.state = State::WaitWindow;
        AcquireSRWLockExclusive(&m_lock);
        m_progress = fresh;
        ReleaseSRWLockExclusive(&m_lock);
    }

    m_thread = CreateThread(nullptr, 0, &InjectionSequence::ThreadProc, this, 0, nullptr);
    if (!m_thread)
    {
        m_running.store(false);
        return false;
    }
    CloseHandle(m_thread); // the thread keeps running; we never join it
    m_thread = nullptr;
    return true;
}

void InjectionSequence::Cancel()
{
    if (m_running.load())
        m_cancel.store(true);
}

void InjectionSequence::Shutdown()
{
    m_cancel.store(true);
    int waited = 0;
    while (m_running.load() && waited < 2000)
    {
        Sleep(25);
        waited += 25;
    }
}

InjectionSequence::Progress InjectionSequence::Snapshot() const
{
    Progress copy;
    AcquireSRWLockShared(&m_lock);
    copy = m_progress;
    ReleaseSRWLockShared(&m_lock);
    return copy;
}

void InjectionSequence::Publish(const Progress& p, bool finished)
{
    {
        AcquireSRWLockExclusive(&m_lock);
        m_progress = p;
        ReleaseSRWLockExclusive(&m_lock);
    }
    if (m_hwnd)
        PostMessageW(m_hwnd, finished ? kMsgFinished : kMsgProgress, 0, 0);
}

DWORD WINAPI InjectionSequence::ThreadProc(LPVOID param)
{
    auto* self = static_cast<InjectionSequence*>(param);
    self->Run(self->m_cfg);
    return 0;
}

void InjectionSequence::Run(Config config)
{
    // ---- Phase 1: wait for a matching game window ------------------------
    {
        Progress p;
        p.state = State::WaitWindow;
        p.message = L"正在等待目标窗口出现…";
        Publish(p, false);
    }

    ProxyTarget target;
    for (;;)
    {
        if (m_cancel.load())
        {
            Progress p;
            p.state = State::Cancelled;
            p.ok = false;
            p.message = L"已取消注入";
            m_running.store(false);
            Publish(p, true);
            return;
        }
        target = ProcessFinder::NewestMatch(config.keyword);
        if (target.valid())
            break;
        Sleep(kPollIntervalMs);
    }

    // ---- Phase 2: inject / signal resident proxy -------------------------
    {
        Progress p;
        p.state = State::Injecting;
        p.pid = target.pid;
        p.windowTitle = target.windowTitle;
        p.message = L"正在注入代理 DLL…";
        Publish(p, false);
    }

    const InjectionResult result = Injector::Run(target.pid, config.dllPath);

    if (!result.ok)
    {
        Progress p;
        p.state = State::Failed;
        p.pid = target.pid;
        p.windowTitle = target.windowTitle;
        p.ok = false;
        p.message = result.message;
        m_running.store(false);
        Publish(p, true);
        return;
    }

    // ---- Phase 3: wait for the proxy listener (max 30 s) ------------------
    int elapsed = 0;
    for (;;)
    {
        if (PortProbe::IsListening(target.pid, config.port))
        {
            Progress p;
            p.state = State::Done;
            p.pid = target.pid;
            p.windowTitle = target.windowTitle;
            p.ok = true;
            p.resumed = result.resumed;
            p.message = result.resumed
                ? FormatStr(L"驻留代理已恢复，端口 %u 就绪", unsigned(config.port))
                : FormatStr(L"注入完成，代理已监听 %u", unsigned(config.port));
            m_running.store(false);
            Publish(p, true);
            return;
        }
        if (m_cancel.load() || elapsed >= kWaitPortTimeoutMs)
            break;
        Sleep(kPollIntervalMs);
        elapsed += kPollIntervalMs;
        if (elapsed % 250 == 0)
        {
            Progress p;
            p.state = State::WaitPort;
            p.pid = target.pid;
            p.windowTitle = target.windowTitle;
            p.waitMs = elapsed;
            p.waitTotal = kWaitPortTimeoutMs;
            p.message = FormatStr(L"等待代理开始监听 %u…", unsigned(config.port));
            Publish(p, false);
        }
    }

    if (m_cancel.load())
    {
        Progress p;
        p.state = State::Cancelled;
        p.pid = target.pid;
        p.windowTitle = target.windowTitle;
        p.ok = false;
        p.message = L"已取消注入";
        m_running.store(false);
        Publish(p, true);
        return;
    }

    Progress p;
    p.state = State::Failed;
    p.pid = target.pid;
    p.windowTitle = target.windowTitle;
    p.ok = false;
    p.resumed = result.resumed;
    p.message = FormatStr(L"注入完成，但 %d 秒内代理未开始监听 %u，请查看日志排查",
                          kWaitPortTimeoutMs / 1000, unsigned(config.port));
    m_running.store(false);
    Publish(p, true);
}

} // namespace micp
