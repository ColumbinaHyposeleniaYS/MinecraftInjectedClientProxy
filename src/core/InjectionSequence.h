#pragma once
// Drives one complete injection run off the UI thread:
//
//   WaitWindow -> Injecting -> WaitPort -> Done / Failed / Cancelled
//
// The worker is a plain Win32 thread; progress is published into a small
// snapshot guarded by an SRWLOCK and the UI is notified with PostMessage.

#include "common.h"
#include <atomic>

namespace micp {

class InjectionSequence
{
public:
    enum class State { Idle, WaitWindow, Injecting, WaitPort, Done, Failed, Cancelled };

    struct Config
    {
        std::wstring keyword;
        uint16_t port = 1314;
    };

    struct Progress
    {
        State state = State::Idle;
        DWORD pid = 0;
        std::wstring windowTitle;
        int waitMs = 0;
        int waitTotal = 30000;
        std::wstring message;
        bool ok = false;
        bool resumed = false;
    };

    // Returns false when a run is already in progress.
    bool Start(HWND notify, const Config& config);
    void Cancel();
    void Shutdown(); // cancel + brief join, called on app quit

    bool IsRunning() const { return m_running.load(); }
    Progress Snapshot() const;

    static constexpr UINT kMsgProgress = WM_APP + 1; // posted on every change
    static constexpr UINT kMsgFinished = WM_APP + 2; // posted once at the end

private:
    static DWORD WINAPI ThreadProc(LPVOID param);
    void Run(Config config);
    void Publish(const Progress& p, bool finished);

    HWND m_hwnd = nullptr;
    HANDLE m_thread = nullptr;
    Config m_cfg; // copied in Start() before the thread is created
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancel{false};
    mutable SRWLOCK m_lock = SRWLOCK_INIT;
    Progress m_progress;
};

} // namespace micp
