#pragma once

#include "proxy.h"

// Descriptor fallbacks for classes whose mapped names are unavailable.
// Class lookup results are local references owned by the caller.
namespace proxy_server {
jmethodID findMethodByDescriptor(jclass klass, const char* desc, bool wantStatic);
// Matches only the argument part of fullDesc; the return type may change between versions.
jmethodID findMethodByArgDescriptor(jclass klass, const char* fullDesc, bool wantStatic);
int findMethodsByDescriptor(jclass klass, const char* desc, bool wantStatic, jmethodID* out, int maxOut);
jmethodID findMethodByDescriptorExcept(jclass klass, const char* desc, bool wantStatic, const char* const* excl,
                                       int nExcl);
jfieldID findFieldByDescriptor(jclass klass, const char* desc, bool wantStatic);
jclass findLoadedBySig(JNIEnv* env, const char* sig);
jclass loadOrFind(JNIEnv* env, jobject mcLoader, const char* dot, const char* sig);
} // namespace proxy_server
std::string JniClassName(JNIEnv* env, jobject object);
