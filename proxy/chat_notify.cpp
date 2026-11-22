#include "chat_notify.h"

#include "proxy.h"
#include "relay_handler.h"
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

const char* PickMessage(const std::string& lang) {
    if (!lang.empty()) {
        for (const Message& m : kMessages) {
            if (lang == m.lang)
                return m.text;
        }
        if (lang.rfind("zh_tw", 0) == 0 || lang.rfind("zh_hk", 0) == 0)
            return kMessages[2].text;
        if (lang.rfind("zh", 0) == 0)
            return kMessages[1].text;
        if (lang.rfind("pt", 0) == 0)
            return kMessages[7].text;
        if (lang.rfind("nl", 0) == 0)
            return kMessages[5].text;
    }
    return kMessages[0].text;
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

void ChatNotify_NotifyProxyReady(JNIEnv* env) {
    bool expected = false;
    if (!g_notified.compare_exchange_strong(expected, true))
        return;
    if (!server.refs.systemChatPacketCls || !server.refs.systemChatPacketCtor ||
        !server.refs.componentCls || !server.refs.componentLiteralMid) {
        g_notified.store(false); // retry when bindings appear
        return;
    }
    if (env->PushLocalFrame(8) != JNI_OK) {
        env->ExceptionClear();
        g_notified.store(false);
        return;
    }

    const jint port = kProxyPort;
    char buffer[160] = {0};
    std::snprintf(buffer, sizeof(buffer), PickMessage(ReadClientLanguage(env)), static_cast<unsigned>(port));

    bool sent = false;
    do {
        jstring text = env->NewStringUTF(buffer);
        if (!text || env->ExceptionCheck())
            break;
        jobject component =
            env->CallStaticObjectMethod(server.refs.componentCls, server.refs.componentLiteralMid, text);
        if (!component || env->ExceptionCheck())
            break;
        jobject packet = env->NewObject(server.refs.systemChatPacketCls, server.refs.systemChatPacketCtor,
                                        component, JNI_FALSE);
        if (!packet || env->ExceptionCheck())
            break;
        if (!sendToA(env, packet))
            break;
        sent = true;
        LogTo("chat-notify: ready message sent (port %u)", static_cast<unsigned>(port));
    } while (false);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->PopLocalFrame(nullptr);
    if (!sent)
        g_notified.store(false);
}

} // namespace proxy_server
