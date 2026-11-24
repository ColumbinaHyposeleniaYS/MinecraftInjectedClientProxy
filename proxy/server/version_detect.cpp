#include "version_detect.h"
#include "jni_lookup.h"
#include <cstring>
#include <string>

namespace proxy_server {

const char* McVersionName(McVersion v) {
    switch (v) {
    case McVersion::V1_20_1: return "1.20.1";
    case McVersion::V1_21_10: return "1.21.10";
    default: return "unknown";
    }
}

McVersion DetectMcVersion(JNIEnv* env, jobject mcLoader, jclass connectionCls) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    // Preferred: SharedConstants.getGameVersion() (obfuscated name, stable signature).
    jclass shared = loadOrFind(env, mcLoader, "net.minecraft.SharedConstants", "Lnet/minecraft/SharedConstants;");
    if (shared) {
        jmethodID getVersion = findMethodByDescriptor(shared, "()Ljava/lang/String;", true);
        if (getVersion) {
            jstring value = static_cast<jstring>(env->CallStaticObjectMethod(shared, getVersion));
            if (value && !env->ExceptionCheck()) {
                const char* chars = env->GetStringUTFChars(value, nullptr);
                std::string text = chars ? chars : "";
                env->ReleaseStringUTFChars(value, chars);
                if (text.compare(0, 4, "1.21") == 0) {
                    env->DeleteLocalRef(value);
                    env->DeleteLocalRef(shared);
                    return McVersion::V1_21_10;
                }
                if (text.compare(0, 4, "1.20") == 0) {
                    env->DeleteLocalRef(value);
                    env->DeleteLocalRef(shared);
                    return McVersion::V1_20_1;
                }
                env->DeleteLocalRef(value);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        env->DeleteLocalRef(shared);
    }
    // Fallback: structural probe on Connection.configureSerialization.
    if (connectionCls) {
        if (findMethodByDescriptor(connectionCls,
                "(Lio/netty/channel/ChannelPipeline;Lnet/minecraft/network/protocol/PacketFlow;ZLnet/minecraft/network/BandwidthDebugMonitor;)V", true))
            return McVersion::V1_21_10;
        if (findMethodByDescriptor(connectionCls,
                "(Lio/netty/channel/ChannelPipeline;Lnet/minecraft/network/protocol/PacketFlow;)V", true))
            return McVersion::V1_20_1;
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    return McVersion::Unknown;
}

} // namespace proxy_server
