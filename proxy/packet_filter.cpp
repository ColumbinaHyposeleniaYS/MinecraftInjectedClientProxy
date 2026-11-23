#include "packet_filter.h"

#include "proxy.h"
#include "b_server.h"
#include "jni_lookup.h"
#include "server/state.h"

#include <mutex>
#include <unordered_map>

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

// Allocation-free suffix test: compare the raw char pointers directly, no
// std::string temporaries. Called once per unseen packet class; results are
// cached below, so the hot path never reaches this for a known class.
bool EndsWithAny(const char* cls, size_t len) {
    if (!cls || len == 0)
        return false;
    for (const char* suffix : kHiddenSuffixes) {
        const size_t slen = strlen(suffix);
        if (len >= slen && memcmp(cls + len - slen, suffix, slen) == 0)
            return true;
    }
    return false;
}

// Per-class verdict cache. Packet classes are a tiny fixed set; each entry is
// resolved once, then every packet costs a single hash lookup. jclass values
// are local references from GetObjectClass, so the key is the class *name*
// (stable for the JVM's lifetime), not the jobject.
struct ClassVerdict {
    bool hidden;
};
std::mutex g_cacheMutex;
std::unordered_map<std::string, bool> g_cache;

bool HiddenByName(JNIEnv* env, jobject packet) {
    // Resolve the class name once; JniClassName is the only allocation on a
    // cache miss. On a hit this path is allocation-free.
    const jclass c = env->GetObjectClass(packet);
    if (!c)
        return false;
    char* sig = nullptr;
    const jvmtiError rc = g_jvmti ? g_jvmti->GetClassSignature(c, &sig, nullptr) : JVMTI_ERROR_INVALID_ENVIRONMENT;
    env->DeleteLocalRef(c);
    if (rc != JVMTI_ERROR_NONE || !sig)
        return false;
    std::string name;
    const char* p = sig;
    if (*p == 'L') {
        ++p;
        for (; *p && *p != ';'; ++p)
            name.push_back(*p == '/' ? '.' : *p);
    } else {
        name = sig;
    }
    g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    const auto it = g_cache.find(name);
    if (it != g_cache.end())
        return it->second;
    const bool hidden = EndsWithAny(name.c_str(), name.size());
    g_cache.emplace(std::move(name), hidden);
    return hidden;
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
            if (!HiddenByName(env, sub)) {
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
    if (!cls.empty())
        return EndsWithAny(cls.c_str(), cls.size()) || bundleFullyHidden(env, packet);
    return HiddenByName(env, packet) || bundleFullyHidden(env, packet);
}

} // namespace packet_filter
