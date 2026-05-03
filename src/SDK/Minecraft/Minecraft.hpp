#pragma once
#include <jni.h>

// Thin wrapper around the Minecraft singleton (obfuscated class "ave", version 1.8.9).
// All obfuscated names are in comments — update them when targeting a different version.
class Minecraft {
public:
    static Minecraft* get();

    bool    isValid() const { return m_obj != nullptr; }
    jobject getObject() const { return m_obj; }

    jobject getPlayer() const;       // EntityClientPlayerMP (bip)
    jobject getWorld()  const;       // WorldClient (bkr)
    jobject getMouseOver() const;    // MovingObjectPosition (bmo) — what the crosshair targets
    bool    isInGui() const;         // true when any screen/menu is open
    void    leftClick();             // simulate left-click (attack / break)
    void    rightClick();            // simulate right-click (use / place)

private:
    Minecraft(JNIEnv* env, jobject obj) : m_env(env), m_obj(obj) {}
    void ensureCached();

    JNIEnv* m_env = nullptr;
    jobject m_obj = nullptr;

    static jmethodID s_leftClick;
    static jmethodID s_rightClick;
    static bool      s_cached;
};
