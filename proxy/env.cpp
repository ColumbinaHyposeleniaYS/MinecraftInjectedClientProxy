#include "proxy.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Debug-log policy. The GUI writes %APPDATA%\MinecraftInjectedClientProxy\
// settings.ini; the [logs] section carries two switches:
//   debug=1        -> LogTo output is written at all (default: off)
//   clearOnStart=1 -> when debug is on, the previous log is deleted at start
// Read once, on first use, via the same ANSI API the GUI writes with.
namespace {

struct DebugLogConfig {
    bool enabled = false;
    bool clearOnStart = true;
};

DebugLogConfig LoadDebugLogConfig() {
    DebugLogConfig cfg;
    char ini[MAX_PATH] = {0};
    const char* appdata = getenv("APPDATA");
    if (!appdata || strlen(appdata) + 48 >= MAX_PATH) return cfg;
    lstrcpynA(ini, appdata, MAX_PATH);
    lstrcatA(ini, "\\MinecraftInjectedClientProxy\\settings.ini");
    cfg.enabled = GetPrivateProfileIntA("logs", "debug", 0, ini) != 0;
    cfg.clearOnStart = GetPrivateProfileIntA("logs", "clearOnStart", 1, ini) != 0;
    return cfg;
}

} // namespace

JavaVM*   g_vm    = nullptr;
jvmtiEnv* g_jvmti = nullptr;

void Dbg(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    MessageBoxA(nullptr, buf, "MinecraftProxy", MB_OK);
    LogTo("[DBG] %s", buf);
}

void LogTo(const char* fmt, ...) {
    static SRWLOCK lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&lock);

    static HANDLE h = INVALID_HANDLE_VALUE;
    static char   activePath[MAX_PATH] = {0};
    static bool   tried = false;

    // Debug switch: with logging disabled nothing is written at all, so the
    // steady-state cost is a single one-shot INI read on the first call.
    static const DebugLogConfig s_cfg = LoadDebugLogConfig();
    if (!tried && s_cfg.enabled && s_cfg.clearOnStart) {
        // Wipe the previous log before the first open. Done under the lock,
        // before tryOpen below can create/append to it.
        char buf[MAX_PATH];
        const char* appdata = getenv("APPDATA");
        if (appdata && strlen(appdata) + 32 < MAX_PATH) {
            lstrcpynA(buf, appdata, MAX_PATH);
            lstrcatA(buf, "\\.minecraft\\proxy.log");
            DeleteFileA(buf);
        }
        char tmp[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, tmp);
        if (n > 0 && n < MAX_PATH - 24) {
            lstrcatA(tmp, "MinecraftProxy.log");
            DeleteFileA(tmp);
        }
        DeleteFileA("C:\\MinecraftProxy.log");
    }
    if (!s_cfg.enabled) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }

    // Rotate at kLogMaxBytes: move the current file to <name>.old (previous
    // .old is discarded) and reopen fresh. Checked per write, so even one
    // long-lived session cannot grow the log without bound.
    constexpr long long kLogMaxBytes = 32LL * 1024 * 1024;
    auto rotateIfNeeded = [&]() {
        if (h == INVALID_HANDLE_VALUE || !activePath[0])
            return;
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= kLogMaxBytes)
            return;
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
        char oldPath[MAX_PATH];
        lstrcpynA(oldPath, activePath, MAX_PATH);
        lstrcatA(oldPath, ".old");
        DeleteFileA(oldPath);
        MoveFileA(activePath, oldPath);
        h = CreateFileA(activePath, FILE_APPEND_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    };
    rotateIfNeeded();

    if (!tried) {
        tried = true;

        auto tryOpen = [](const char* path) -> HANDLE {
            if (!path || !*path) return INVALID_HANDLE_VALUE;
            return CreateFileA(path, FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        };
        char buf[MAX_PATH];

        if (h == INVALID_HANDLE_VALUE) {
            const char* appdata = getenv("APPDATA");
            if (appdata && strlen(appdata) + 32 < MAX_PATH) {
                lstrcpynA(buf, appdata, MAX_PATH);
                lstrcatA(buf, "\\.minecraft\\proxy.log");
                h = tryOpen(buf);
                if (h != INVALID_HANDLE_VALUE) lstrcpynA(activePath, buf, MAX_PATH);
            }
        }



        if (h == INVALID_HANDLE_VALUE) {
            char tmp[MAX_PATH];
            DWORD n = GetTempPathA(MAX_PATH, tmp);
            if (n > 0 && n < MAX_PATH - 24) {
                lstrcatA(tmp, "MinecraftProxy.log");
                h = tryOpen(tmp);
                if (h != INVALID_HANDLE_VALUE) lstrcpynA(activePath, tmp, MAX_PATH);
            }
        }

        if (h == INVALID_HANDLE_VALUE) {
            h = tryOpen("C:\\MinecraftProxy.log");
            if (h != INVALID_HANDLE_VALUE)
                lstrcpynA(activePath, "C:\\MinecraftProxy.log", MAX_PATH);
        }
    }

    if (h != INVALID_HANDLE_VALUE) {
        char msg[1200];
        va_list ap;
        va_start(ap, fmt);
        int base = snprintf(msg, sizeof(msg),
                            "[%08lu] ", (unsigned long)GetTickCount());
        vsnprintf(msg + base, sizeof(msg) - base - 2, fmt, ap);
        va_end(ap);
        size_t len = strlen(msg);
        if (len < sizeof(msg) - 2) {
            msg[len++] = '\r';
            msg[len++] = '\n';
        }

        DWORD wrote = 0;
        WriteFile(h, msg, (DWORD)len, &wrote, NULL);
    }
    ReleaseSRWLockExclusive(&lock);
}

void LogAndClearException(JNIEnv* env, const char* where) {
    if (!env || !env->ExceptionCheck()) return;
    jthrowable t = env->ExceptionOccurred();
    env->ExceptionClear();

    jclass thrCls = env->FindClass("java/lang/Throwable");
    jmethodID toStringMid =
        env->GetMethodID(thrCls, "toString", "()Ljava/lang/String;");
    jstring msg = (jstring)env->CallObjectMethod(t, toStringMid);

    const char* c = msg ? env->GetStringUTFChars(msg, nullptr) : "<unknown>";
    Dbg("[%s] %s", where, c);
    if (msg) env->ReleaseStringUTFChars(msg, c);

    jmethodID pst = env->GetMethodID(thrCls, "printStackTrace", "()V");
    if (pst) env->CallVoidMethod(t, pst);
    if (env->ExceptionCheck()) env->ExceptionClear();

    env->DeleteLocalRef(t);
    env->DeleteLocalRef(thrCls);
    if (msg) env->DeleteLocalRef(msg);
}

JniAttach::JniAttach() {
    if (!g_vm) return;
    jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (rc == JNI_EDETACHED) {
        JavaVMAttachArgs args{};
        args.version = JNI_VERSION_1_8;
        // Plain Netty-looking name: avoid proxy/inject/runtime semantics that
        // a hostile native hook can scan for in JavaThread/_thread_list.
        args.name = const_cast<char*>("nioEventLoopGroup-9-2");
        args.group = nullptr;
        if (g_vm->AttachCurrentThreadAsDaemon(
                reinterpret_cast<void**>(&env), &args) == JNI_OK) {
            attached = true;
        } else {
            env = nullptr;
        }
    } else if (rc != JNI_OK) {
        env = nullptr;
    }
}

JniAttach::~JniAttach() {
    if (attached && g_vm) g_vm->DetachCurrentThread();
}

jobject GetMinecraftClassLoader(JNIEnv* env, jvmtiEnv* jvmti) {
    if (!env || !jvmti) return nullptr;

    jint count = 0;
    jthread* threads = nullptr;
    if (jvmti->GetAllThreads(&count, &threads) != JVMTI_ERROR_NONE) {
        return nullptr;
    }

    static int s_dump_count = 0;
    bool dump = (s_dump_count++ < 3);
    if (dump) LogTo("GetMinecraftClassLoader: enumerating %d threads", (int)count);

    jobject result = nullptr;
    for (jint i = 0; i < count; ++i) {
        jvmtiThreadInfo info{};
        if (jvmti->GetThreadInfo(threads[i], &info) != JVMTI_ERROR_NONE) continue;

        bool hasLoader = (info.context_class_loader != nullptr);
        if (dump) {
            LogTo("  thread[%d] name='%s' has_loader=%d",
                  (int)i,
                  info.name ? info.name : "(null)",
                  hasLoader ? 1 : 0);
        }

        bool hit = info.name && hasLoader &&
                   std::strcmp(info.name, "Render thread") == 0;

        if (!hit && info.name && hasLoader) {
            jclass conn = LoadClassInLoader(env, info.context_class_loader,
                                            "net.minecraft.network.Connection");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (conn) {
                if (dump) LogTo("     -> can load MC.Connection, PICKED");
                hit = true;
                env->DeleteLocalRef(conn);
            }
        }

        if (hit) result = env->NewGlobalRef(info.context_class_loader);

        if (info.name) jvmti->Deallocate(reinterpret_cast<unsigned char*>(info.name));
        if (result) break;
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char*>(threads));
    return result;
}

jclass LoadClassInLoader(JNIEnv* env, jobject classLoader, const char* dotName) {
    if (!env || !classLoader || !dotName) return nullptr;

    jclass clCls = env->GetObjectClass(classLoader);
    jmethodID mid =
        env->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!mid) {
        env->ExceptionClear();
        env->DeleteLocalRef(clCls);
        return nullptr;
    }

    jstring jname = env->NewStringUTF(dotName);
    jclass  result = (jclass)env->CallObjectMethod(classLoader, mid, jname);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = nullptr;
    }

    env->DeleteLocalRef(jname);
    env->DeleteLocalRef(clCls);
    return result;
}
