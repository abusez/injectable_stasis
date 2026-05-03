#pragma once
#include <Windows.h>
#include <jni.h>
#include <string>

namespace SDK {
    bool Init();
    void Shutdown();

    JavaVM* GetVM();
    JNIEnv* GetEnv();

    std::string JStringToStr(JNIEnv* env, jstring str);
    jstring     StrToJString(JNIEnv* env, const std::string& str);
    void        ClearException(JNIEnv* env);
    bool        CheckException(JNIEnv* env);

    // Returns the Minecraft singleton object (obfuscated class "ave" in 1.8.9)
    jobject GetMinecraftObject(JNIEnv* env);

    // Print a client-side message to Minecraft's chat (does NOT send to server).
    // Tries several obfuscated class/method name combos; logs every attempt to
    // C:\stasis_debug.log so you can see exactly which names work on your jar.
    void PrintChat(const char* message);
}
