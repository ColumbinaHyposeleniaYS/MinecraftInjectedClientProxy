#include "proxy.h"
#include "relay_handler.h"
#include "b_server.h"
#include "chat_notify.h"
#include "server/state.h" // ProxyPort() - actual bind port from GUI settings.ini
#include "runtime_gate.h"
#include "RuntimeControl.h" // resume-event contract shared with the GUI injector (src/core/injectcore)

#include <cstdio>

namespace {

bool deletePressed(DeleteKeyEdge& edge) {
    DWORD foregroundPid = 0;
    HWND window = GetForegroundWindow();
    if (window) GetWindowThreadProcessId(window, &foregroundPid);
    return edge.update((GetAsyncKeyState(VK_DELETE) & 0x8000) != 0,
                       foregroundPid == GetCurrentProcessId());
}

bool startSession(JNIEnv* env, DeleteKeyEdge& edge) {
    LogTo("START: enabling proxy");
    LogTo("BUILD: native-snapshot %s %s pid=%lu", __DATE__, __TIME__, GetCurrentProcessId());
    // Wait for the game class loader, then attach the Netty relay directly.
    // No class bytecode is rewritten: A is always captured mid-session via
    // JNI (Minecraft.getConnection().channel.pipeline()), so a hostile native
    // hook never sees a modified Connection.channelActive or a retransformed
    // InstanceKlass/ConstantPool. This loop also keeps DEL-cancel responsive.
    bool installed = false;
    for (int i = 0; i < 240; ++i) {
        if (deletePressed(edge)) {
            LogTo("START: cancelled by DEL");
            return false;
        }
        if (InstallRelayHandler(env)) { installed = true; break; }
        Sleep(100);
    }
    if (!installed) return false;
    g_runtimeGate.start();
    if (!InstallBServer(env)) return false;
    LiveConnectionState connection = LiveConnectionState::Failed;
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (deletePressed(edge)) return false;
        if (env->PushLocalFrame(128) != JNI_OK) { env->ExceptionClear(); return false; }
        connection = BServer_TryCaptureLiveConnection(env);
        env->PopLocalFrame(nullptr);
        if (connection != LiveConnectionState::Failed) break;
        Sleep(100);
    }
    if (connection == LiveConnectionState::Failed) {
        LogTo("START: failed to resolve A's connection; proxy will suspend");
        return false;
    }
    if (connection == LiveConnectionState::NotConnected) {
        // Pure mid-session design: A must already be in-game. Never block A's
        // render thread waiting for B (BServer_BlockAMainThreadUntilBConnected
        // was the cause of "A freezes when injected before joining"). Suspend
        // cleanly instead; re-inject after A has joined a world.
        LogTo("START: A is not connected yet; mid-session proxy needs A in-game. "
              "Suspending (join a world, then inject again)");
        return false;
    }
    proxy_server::ChatNotify_ResetPaused();
    LogTo("START: active on port %d; DEL in A suspends the proxy",
          (int)proxy_server::ProxyPort());
    return true;
}

bool stopSession(JNIEnv* env, bool userRequested = false) {
    if (userRequested)
        proxy_server::ChatNotify_NotifyPaused(env);
    // Retry throttle: the runtime thread retries stopSession every second
    // while callbacks are stuck; log the drain line once per stuck streak
    // instead of every retry (log spam previously grew the log to gigabytes).
    static bool drainLogged = false;
    if (!drainLogged) {
        LogTo("STOP: draining native callbacks");
        drainLogged = true;
    }
    // Release the render-thread gate before waiting for callbacks that may need it.
    BServer_RequestStop();
    if (!g_runtimeGate.stopFor(std::chrono::milliseconds(1000))) {
        LogTo("STOP: callbacks still running; cleanup will retry");
        return false;
    }
    bool stopped = StopBServer(env);
    bool detached = RelayHandler_DetachAll(env);
    stopped = stopped && detached;
    if (stopped) {
        LogTo("STOP: suspended; run the injector again to reactivate");
        drainLogged = false;
    }
    return stopped;
}

DWORD WINAPI RuntimeThread(LPVOID) {
    char eventName[96];
    std::snprintf(eventName, sizeof(eventName), MICP_RESUME_PREFIX "%lu", GetCurrentProcessId());
    HANDLE resume = CreateEventA(nullptr, FALSE, FALSE, eventName);
    if (!resume) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SetEvent(resume);
        CloseHandle(resume);
        return 0;
    }

    std::snprintf(eventName, sizeof(eventName), MICP_DISCONNECT_PREFIX "%lu", GetCurrentProcessId());
    HANDLE disconnect = CreateEventA(nullptr, FALSE, FALSE, eventName);
    if (!disconnect) {
        CloseHandle(resume);
        return 0;
    }

    LogTo("Runtime: resident controller started");
    HMODULE jvm = nullptr;
    for (int i = 0; i < 600 && !jvm; ++i) {
        jvm = GetModuleHandleA("jvm.dll");
        if (!jvm) Sleep(100);
    }
    using GetVms = jint (JNICALL*)(JavaVM**, jsize, jsize*);
    auto getVms = jvm ? reinterpret_cast<GetVms>(GetProcAddress(jvm, "JNI_GetCreatedJavaVMs")) : nullptr;
    if (getVms) {
        for (int i = 0; i < 600; ++i) {
            jsize count = 0;
            if (getVms(&g_vm, 1, &count) == JNI_OK && count && g_vm) break;
            g_vm = nullptr;
            Sleep(100);
        }
    }
    JNIEnv* env = nullptr;
    // Plain Netty-looking name, not "MICPRuntime": see env.cpp rationale.
    JavaVMAttachArgs args{JNI_VERSION_1_8, const_cast<char*>("nioEventLoopGroup-3-1"), nullptr};
    if (!g_vm || g_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), &args) != JNI_OK) {
        LogTo("Runtime: could not attach to JVM");
        CloseHandle(disconnect);
        CloseHandle(resume);
        return 0;
    }
    if (g_vm->GetEnv(reinterpret_cast<void**>(&g_jvmti), JVMTI_VERSION_1_2) != JNI_OK) {
        LogTo("Runtime: JVMTI unavailable");
        CloseHandle(disconnect);
        g_vm->DetachCurrentThread();
        CloseHandle(resume);
        return 0;
    }

    DeleteKeyEdge edge;
    bool active = false;
    bool clean = true;
    bool requested = true;
    ULONGLONG retryAt = 0;
    for (;;) {
        if (active) BServer_CheckLoginTimeout(env);
        if (!active && !clean && GetTickCount64() >= retryAt) {
            if (env->PushLocalFrame(512) == JNI_OK) {
                clean = stopSession(env);
                env->PopLocalFrame(nullptr);
            } else env->ExceptionClear();
            retryAt = GetTickCount64() + 1000;
        }
        if (requested && !active && clean) {
            requested = false;
            if (env->PushLocalFrame(512) == JNI_OK) {
                if (clean) {
                    active = startSession(env, edge);
                    if (!active) clean = stopSession(env);
                }
                env->PopLocalFrame(nullptr);
            } else {
                env->ExceptionClear();
                LogTo("Runtime: local frame allocation failed");
            }
        }
        if (deletePressed(edge) && (active || requested)) {
            // Ignore duplicate start requests made before this stop.
            ResetEvent(resume);
            requested = false;
            if (env->PushLocalFrame(512) == JNI_OK) {
                clean = stopSession(env, /*userRequested=*/true);
                active = false;
                env->PopLocalFrame(nullptr);
            } else env->ExceptionClear();
        }
        HANDLE waits[2] = { resume, disconnect };
        DWORD wait = WaitForMultipleObjects(2, waits, FALSE, active ? 25 : 250);
        if (wait == WAIT_OBJECT_0 && !active) requested = true;
        if (wait == WAIT_OBJECT_0 + 1 && active) {
            // GUI asked to drop A's connection to the remote server; the proxy stays resident.
            if (env->PushLocalFrame(128) == JNI_OK) {
                BServer_DisconnectARemote(env);
                env->PopLocalFrame(nullptr);
            } else env->ExceptionClear();
        }
        if (wait == WAIT_FAILED) break;
    }
    if (env->PushLocalFrame(512) == JNI_OK) {
        stopSession(env);
        env->PopLocalFrame(nullptr);
    } else env->ExceptionClear();
    CloseHandle(disconnect);
    g_vm->DetachCurrentThread();
    CloseHandle(resume);
    return 0;
}
}

void InitializeProxy(JNIEnv*) {
    HANDLE thread = CreateThread(nullptr, 0, RuntimeThread, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        InitializeProxy(nullptr);
    }
    return TRUE;
}
