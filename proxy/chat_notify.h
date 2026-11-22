#pragma once
// Sends the "proxying on (port)" system-chat message to client A once, right
// after its login reaches PLAY. The message text is localized in the 12 GUI
// languages; A's client language is read from Minecraft.options.language when
// available (falls back to English).

#include <jni.h>

namespace proxy_server {

// Fires once per session (atomic guard). Safe to call at every PLAY store.
void ChatNotify_NotifyProxyReady(JNIEnv* env);

// Sends a "proxy suspended" system-chat message to client A. Call before
// teardown on a user-requested stop (Del key); only fires while a B session
// was actually running.
void ChatNotify_NotifyPaused(JNIEnv* env);

// Clears the once-per-session guard so the next pause gets a fresh message.
void ChatNotify_ResetPaused();

// Registers chat/component bindings; failure is non-fatal.
bool cacheChatNotifyBindings(JNIEnv* env, jobject mcLoader);

} // namespace proxy_server
