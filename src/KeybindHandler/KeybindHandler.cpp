#include "../pch.hpp"
#include "KeybindHandler.hpp"
#include "../ModuleManager/ModuleManager.hpp"
#include "../GUI/ClickGUI.hpp"
#include "../Rendering/Hooking/hooking.hpp"
#include "../SDK/sdk.hpp"

namespace KeybindHandler {

int g_guiKey = 'M';

static bool s_prevStates[256] = {};

static jclass    s_mouseClass = nullptr;
static jmethodID s_isGrabbed  = nullptr;

static bool IsInGame() {
    JNIEnv* env = SDK::GetEnv();
    if (!env) return false;
    if (!s_mouseClass) {
        jclass local = env->FindClass("org/lwjgl/input/Mouse");
        if (!local) { SDK::ClearException(env); return false; }
        s_mouseClass = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
    }
    if (!s_isGrabbed) {
        s_isGrabbed = env->GetStaticMethodID(s_mouseClass, "isGrabbed", "()Z");
        if (!s_isGrabbed) { SDK::ClearException(env); return false; }
    }
    jboolean grabbed = env->CallStaticBooleanMethod(s_mouseClass, s_isGrabbed);
    SDK::ClearException(env);
    return grabbed == JNI_TRUE;
}

void poll() {
    if (Hooking::g_hwnd && GetForegroundWindow() != Hooking::g_hwnd)
        return;

    // GUI toggle — only when actually in-game (mouse grabbed by LWJGL)
    if (g_guiKey > 0 && g_guiKey < 256) {
        bool down = (GetAsyncKeyState(g_guiKey) & 0x8000) != 0;
        if (down && !s_prevStates[g_guiKey] && IsInGame())
            ClickGUI::get().toggle();
        s_prevStates[g_guiKey] = down;
    }

    // Module keybinds
    for (auto& mod : ModuleManager::get().getModules()) {
        int vk = mod->keyBind;
        if (vk <= 0 || vk >= 256) continue;
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !s_prevStates[vk])
            mod->toggle();
        s_prevStates[vk] = down;
    }
}

} // namespace KeybindHandler
