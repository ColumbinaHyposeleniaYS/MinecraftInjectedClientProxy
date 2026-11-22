#pragma once
// Sends the "proxying on (port)" system-chat message to client A once, right
// after its login reaches PLAY. The message text is localized in the 12 GUI
// languages; A's client language is read from Minecraft.options.language when
// available (falls back to English).

#include <jni.h>

namespace proxy_server {

// Fires once per session (atomic guard). Safe to call at every PLAY store.
void ChatNotify_NotifyProxyReady(JNIEnv* env);

// Registers chat/component bindings; failure is non-fatal.
bool cacheChatNotifyBindings(JNIEnv* env, jobject mcLoader);

} // namespace proxy_server
