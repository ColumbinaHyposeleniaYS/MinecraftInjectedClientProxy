#pragma once
// Hide-from-A packet filter ("void mode"): while B is active, packets that
// exist purely for A's local rendering (chunks, chat, titles, boss bars,
// sounds...) are dropped before they reach A's pipeline. B still receives the
// full stream via BServer_ForwardToB, so the shared session is unaffected.
// State-bearing packets (position, health, time, keep-alive, player info...)
// are always delivered to A to keep its protocol state consistent.

#include <jni.h>
#include <string>

namespace packet_filter {

// Returns true when the clientbound packet should NOT be handed to A.
// cls is the dotted packet class name (may be empty when lookup fails).
bool HideFromA(JNIEnv* env, jobject packet, const std::string& cls);

} // namespace packet_filter
