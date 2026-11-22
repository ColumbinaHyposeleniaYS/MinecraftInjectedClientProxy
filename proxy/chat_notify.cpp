#include "chat_notify.h"

#include "proxy.h"
#include "relay_handler.h"
#include "b_server.h"
#include "server/internal.h"
#include "server/state.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

namespace proxy_server {

namespace {

// Direct A-send that intentionally bypasses the void-mode filter: the proxy
// itself owns this packet, and routeToA would drop SystemChatPacket from the
// hide list. Marks bypass so RelayWrite cannot eat it either.
bool sendToA(JNIEnv* env, jobject packet)
{
    jobject target;
    {
        std::lock_guard<std::mutex> l(server.targetMutex);
        target = server.targetConnection ? env->NewLocalRef(server.targetConnection) : nullptr;
    }
    if (!target)
        return false;
    if (!server.refs.connectionSendMid) {
        env->DeleteLocalRef(target);
        return false;
    }
    RelayFilter_MarkBypass(env, packet);
    env->CallVoidMethod(target, server.refs.connectionSendMid, packet);
    const bool ok = !env->ExceptionCheck();
    if (!ok)
        LogAndClearException(env, "chat-notify/sendToA");
    env->DeleteLocalRef(target);
    return ok;
}


std::atomic<bool> g_notified{false};
std::atomic<bool> g_pausedNotified{false};

struct Message {
    const char* lang; // Minecraft language code (lowercase)
    const char* text; // %u placeholder for the port
};

// Matches the GUI's 12-language set (lowercased Minecraft codes). Exact match
// wins; zh_*/pt_*/nl_* fall back to their primary locale, otherwise English.
const Message kMessages[] = {
    {"en_us", "Proxying on port %u"},
    {"zh_cn", "\u5df2\u5728\u7aef\u53e3 %u \u4ee3\u7406"},
    {"zh_tw", "\u5df2\u5728\u9023\u63a5\u57e0 %u \u4ee3\u7406"},
    {"ja_jp", "\u30dd\u30fc\u30c8 %u \u3067\u30d7\u30ed\u30ad\u30b7\u4e2d"},
    {"de_de", "Proxy aktiv auf Port %u"},
    {"nl_nl", "Proxy actief via poort %u"},
    {"nl_be", "Proxy actief via poort %u"},
    {"pt_br", "Usando proxy na porta %u"},
    {"pt_pt", "A usar proxy na porta %u"},
    {"ru_ru", "\u041f\u0440\u043e\u043a\u0441\u0438 \u0440\u0430\u0431\u043e\u0442\u0430\u0435\u0442 \u043d\u0430 \u043f\u043e\u0440\u0442\u0443 %u"},
    {"tr_tr", "%u \u00fczerinden proxy aktif"},
    {"uk_ua", "\u041f\u0440\u043e\u043a\u0441\u0456 \u043f\u0440\u0430\u0446\u044e\u0454 \u043d\u0430 \u043f\u043e\u0440\u0442\u0443 %u"},
};

// "Proxy paused" messages (same language set; no placeholder).
const Message kPausedMessages[] = {
    {"en_us", "Proxy paused"},
    {"zh_cn", "\u4ee3\u7406\u5df2\u6682\u505c"},
    {"zh_tw", "\u4ee3\u7406\u5df2\u66ab\u505c"},
    {"ja_jp", "\u30d7\u30ed\u30ad\u30b7\u4e00\u6642\u505c\u6b62\u4e2d"},
    {"de_de", "Proxy pausiert"},
    {"nl_nl", "Proxy gepauzeerd"},
    {"nl_be", "Proxy gepauzeerd"},
    {"pt_br", "Proxy pausado"},
    {"pt_pt", "Proxy em pausa"},
    {"ru_ru", "\u041f\u0440\u043e\u043a\u0441\u0438 \u043f\u0440\u0438\u043e\u0441\u0442\u0430\u043d\u043e\u0432\u043b\u0435\u043d"},
    {"tr_tr", "Proxy duraklatildi"},
    {"uk_ua", "\u041f\u0440\u043e\u043a\u0441\u0456 \u043f\u0440\u0438\u0437\u0443\u043f\u0438\u043d\u0435\u043d\u043e"},
};

const char* PickFrom(const Message* table, int count, const std::string& lang,
                     int zhTwIdx, int zhIdx, int ptIdx, int nlIdx) {
    if (!lang.empty()) {
        for (int i = 0; i < count; ++i) {
            if (lang == table[i].lang)
                return table[i].text;
        }
        if (lang.rfind("zh_tw", 0) == 0 || lang.rfind("zh_hk", 0) == 0)
            return table[zhTwIdx].text;
        if (lang.rfind("zh", 0) == 0)
            return table[zhIdx].text;
        if (lang.rfind("pt", 0) == 0)
            return table[ptIdx].text;
        if (lang.rfind("nl", 0) == 0)
            return table[nlIdx].text;
    }
    return table[0].text;
}

const char* PickMessage(const std::string& lang) {
    return PickFrom(kMessages, 12, lang, 2, 1, 7, 5);
}

const char* PickPausedMessage(const std::string& lang) {
    return PickFrom(kPausedMessages, 12, lang, 2, 1, 7, 5);
}

std::string ReadClientLanguage(JNIEnv* env) {
    if (!server.refs.minecraftCls || !server.refs.mcGetInstanceMid || !server.refs.mcOptionsFid ||
        !server.refs.optionsCls || !server.refs.optionsLanguageFid)
        return {};
    if (env->PushLocalFrame(8) != JNI_OK) {
        env->ExceptionClear();
        return {};
    }
    std::string result;
    do {
        jobject mc = env->CallStaticObjectMethod(server.refs.minecraftCls, server.refs.mcGetInstanceMid);
        if (!mc || env->ExceptionCheck())
            break;
        jobject options = env->GetObjectField(mc, server.refs.mcOptionsFid);
        if (!options || env->ExceptionCheck())
            break;
        jstring lang = static_cast<jstring>(env->GetObjectField(options, server.refs.optionsLanguageFid));
        if (!lang || env->ExceptionCheck())
            break;
        const char* utf = env->GetStringUTFChars(lang, nullptr);
        if (utf) {
            result = utf;
            env->ReleaseStringUTFChars(lang, utf);
        }
    } while (false);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->PopLocalFrame(nullptr);
    for (char& ch : result)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    return result;
}

} // namespace

bool cacheChatNotifyBindings(JNIEnv* env, jobject mcLoader) {
    auto ref = [&](jobject local) { return local ? env->NewGlobalRef(local) : nullptr; };

    jclass systemChat =
        LoadClassInLoader(env, mcLoader, "net.minecraft.network.protocol.game.ClientboundSystemChatPacket");
    if (!systemChat) {
        LogAndClearException(env, "chat-notify/system chat class");
        return false;
    }
    server.refs.systemChatPacketCls = static_cast<jclass>(ref(systemChat));
    server.refs.systemChatPacketCtor =
        env->GetMethodID(systemChat, "<init>", "(Lnet/minecraft/network/chat/Component;Z)V");
    env->DeleteLocalRef(systemChat);

    jclass component = LoadClassInLoader(env, mcLoader, "net.minecraft.network.chat.Component");
    if (component) {
        server.refs.componentCls = static_cast<jclass>(ref(component));
        server.refs.componentLiteralMid = env->GetStaticMethodID(
            component, "literal", "(Ljava/lang/String;)Lnet/minecraft/network/chat/MutableComponent;");
        env->DeleteLocalRef(component);
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();

    // Client language: Minecraft.options -> Options.language
    if (server.refs.minecraftCls) {
        jclass mcClass = static_cast<jclass>(env->NewLocalRef(server.refs.minecraftCls));
        server.refs.mcOptionsFid = env->GetFieldID(mcClass, "options", "Lnet/minecraft/client/Options;");
        env->DeleteLocalRef(mcClass);
        jclass options = LoadClassInLoader(env, mcLoader, "net.minecraft.client.Options");
        if (options) {
            server.refs.optionsCls = static_cast<jclass>(ref(options));
            server.refs.optionsLanguageFid = env->GetFieldID(options, "language", "Ljava/lang/String;");
            env->DeleteLocalRef(options);
        }
        if (env->ExceptionCheck())
            env->ExceptionClear();
    }

    const bool ok = server.refs.systemChatPacketCls && server.refs.systemChatPacketCtor &&
                    server.refs.componentCls && server.refs.componentLiteralMid;
    if (!ok)
        LogTo("chat-notify: bindings unavailable; ready message disabled");
    return ok;
}

// Builds and sends a one-literal system chat packet. Returns false on any
// failure so callers can retry later.
bool SendSystemChat(JNIEnv* env, const char* text)
{
    if (!server.refs.systemChatPacketCls || !server.refs.systemChatPacketCtor ||
        !server.refs.componentCls || !server.refs.componentLiteralMid)
        return false;
    if (env->PushLocalFrame(8) != JNI_OK) {
        env->ExceptionClear();
        return false;
    }
    bool sent = false;
    do {
        jstring str = env->NewStringUTF(text);
        if (!str || env->ExceptionCheck())
            break;
        jobject component =
            env->CallStaticObjectMethod(server.refs.componentCls, server.refs.componentLiteralMid, str);
        if (!component || env->ExceptionCheck())
            break;
        jobject packet = env->NewObject(server.refs.systemChatPacketCls, server.refs.systemChatPacketCtor,
                                        component, JNI_FALSE);
        if (!packet || env->ExceptionCheck())
            break;
        sent = sendToA(env, packet);
    } while (false);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->PopLocalFrame(nullptr);
    return sent;
}

void ChatNotify_NotifyProxyReady(JNIEnv* env) {
    bool expected = false;
    if (!g_notified.compare_exchange_strong(expected, true))
        return;

    const jint port = kProxyPort;
    char buffer[160] = {0};
    std::snprintf(buffer, sizeof(buffer), PickMessage(ReadClientLanguage(env)), static_cast<unsigned>(port));

    if (SendSystemChat(env, buffer)) {
        LogTo("chat-notify: ready message sent (port %u)", static_cast<unsigned>(port));
        return;
    }
    g_notified.store(false); // retry when bindings / connection appear
}

void ChatNotify_ResetPaused() {
    g_pausedNotified.store(false);
}

void ChatNotify_NotifyPaused(JNIEnv* env) {
    // Only meaningful while a B session was actually running.
    if (!BServer_IsBActive())
        return;
    bool expected = false;
    if (!g_pausedNotified.compare_exchange_strong(expected, true))
        return;
    char buffer[64] = {0};
    std::snprintf(buffer, sizeof(buffer), "%s", PickPausedMessage(ReadClientLanguage(env)));
    if (SendSystemChat(env, buffer))
        LogTo("chat-notify: paused message sent");
    else
        g_pausedNotified.store(false);
}

} // namespace proxy_server
