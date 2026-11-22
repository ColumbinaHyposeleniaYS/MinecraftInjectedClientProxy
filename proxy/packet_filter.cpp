#include "packet_filter.h"

#include "b_server.h"
#include "jni_lookup.h"
#include "server/state.h"

namespace packet_filter {
namespace {

// 1.20.1 clientbound packets that only feed A's local rendering. Dropping
// them removes the render/network load from A while B keeps a full view.
const char* const kHiddenSuffixes[] = {
    "LevelChunkWithLightPacket",     // chunk data + light
    "LevelChunkPacket",              // pre-1.18 chunk data
    "ForgetLevelChunkPacket",        // chunk unload
    "LightUpdatePacket",             // light deltas
    "SystemChatPacket",              // chat overlay / system messages
    "PlayerChatPacket",              // signed player chat
    "PlayerChatHeaderPacket",        // chat header update
    "DisguisedChatPacket",           // unsigned player chat
    "SetActionBarTextPacket",        // action bar
    "SetTitleTextPacket",            // title
    "SetSubtitleTextPacket",         // subtitle
    "SetTitleTextAnimationPacket",   // title timings
    "ClearTitlesPacket",             // title clear
    "SetObjectivePacket",            // scoreboard objectives
    "SetDisplayObjectivePacket",     // scoreboard slots
    "SetScorePacket",                // scoreboard scores
    "SetPlayerTeamPacket",           // team list UI
    "BossEventPacket",               // boss bars
    "SoundPacket",                   // ClientboundSoundPacket (global sound)
    "SoundEntityPacket",             // ClientboundSoundEntityPacket (positional)
    "StopSoundPacket",               // stop custom sound
    "LevelEventPacket",              // generic effect events (break sounds, smoke)
};

bool EndsWithAny(const std::string& cls) {
    if (cls.empty())
        return false;
    for (const char* suffix : kHiddenSuffixes) {
        const std::string s(suffix);
        if (cls.size() >= s.size() && cls.compare(cls.size() - s.size(), s.size(), s) == 0)
            return true;
    }
    return false;
}

// A ClientboundBundlePacket can wrap chunk/chat packets; the container itself
// is not on the suffix list. Drop the bundle only when EVERY sub-packet is
// render-only - a mixed bundle must still reach A so its protocol state
// (reordering of bundled packets) stays intact.
bool bundleFullyHidden(JNIEnv* env, jobject packet) {
    const auto& refs = proxy_server::server.refs;
    if (!refs.bundlePacketCls || !refs.bundleSubPacketsMid || !refs.iterableIteratorMid ||
        !refs.iteratorHasNextMid || !refs.iteratorNextMid)
        return false;
    if (!env->IsInstanceOf(packet, refs.bundlePacketCls))
        return false;
    if (env->PushLocalFrame(8) != JNI_OK) {
        env->ExceptionClear();
        return false;
    }
    bool verdict = false;
    do {
        jobject iterable = env->CallObjectMethod(packet, refs.bundleSubPacketsMid);
        if (env->ExceptionCheck() || !iterable)
            break;
        jobject it = env->CallObjectMethod(iterable, refs.iterableIteratorMid);
        if (env->ExceptionCheck() || !it)
            break;
        int seen = 0;
        while (true) {
            const jboolean more = env->CallBooleanMethod(it, refs.iteratorHasNextMid);
            if (env->ExceptionCheck())
                break;
            if (!more)
                break;
            jobject sub = env->CallObjectMethod(it, refs.iteratorNextMid);
            if (env->ExceptionCheck())
                break;
            if (!sub)
                continue;
            ++seen;
            const std::string cls = JniClassName(env, sub);
            env->DeleteLocalRef(sub);
            if (!EndsWithAny(cls)) {
                seen = -1; // a state-bearing packet: keep the whole bundle
                break;
            }
        }
        if (seen > 0)
            verdict = true;
    } while (false);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->PopLocalFrame(nullptr);
    return verdict;
}

} // namespace

bool HideFromA(JNIEnv* env, jobject packet, const std::string& cls) {
    if (!BServer_IsBActive())
        return false; // no B session: A plays normally
    std::string name = cls;
    if (name.empty())
        name = JniClassName(env, packet);
    return EndsWithAny(name) || bundleFullyHidden(env, packet);
}

} // namespace packet_filter
