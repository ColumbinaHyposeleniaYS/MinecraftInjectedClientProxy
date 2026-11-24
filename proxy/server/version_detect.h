#pragma once

#include "proxy.h"

namespace proxy_server {

enum class McVersion { Unknown, V1_20_1, V1_21_10 };

// Detects the vanilla protocol generation using SharedConstants when available,
// then falls back to structural probes on Connection (no SRG names required).
McVersion DetectMcVersion(JNIEnv* env, jobject mcLoader, jclass connectionCls);
const char* McVersionName(McVersion v);

} // namespace proxy_server
