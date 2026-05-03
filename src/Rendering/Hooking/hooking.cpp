#include "../../pch.hpp"
#include "hooking.hpp"
#include "../Overlay/overlay.hpp"
#include "../../ModuleManager/ModuleManager.hpp"
#include "../../GUI/ClickGUI.hpp"
#include "../../SDK/sdk.hpp"
#include "../ArrayList.hpp"

#include <MinHook.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace Hooking {

using tWglSwapBuffers = BOOL(WINAPI*)(HDC);
static tWglSwapBuffers g_origSwapBuffers = nullptr;
static bool            g_initDone        = false;
static bool            g_guiWasVisible   = false;

// Cached JNI refs for org.lwjgl.input.Mouse.setGrabbed(boolean)
static jclass    s_mouseClass  = nullptr;
static jmethodID s_setGrabbed  = nullptr;

// Calls LWJGL's Mouse.setGrabbed() — the only reliable way to release
// Minecraft's raw mouse capture. LWJGL polls GetCursorPos directly so
// intercepting WM_MOUSEMOVE has no effect on head rotation.
static void LWJGLSetGrabbed(bool grabbed) {
    JNIEnv* env = SDK::GetEnv();
    if (!env) return;

    if (!s_mouseClass) {
        jclass local = env->FindClass("org/lwjgl/input/Mouse");
        if (!local) { SDK::ClearException(env); return; }
        s_mouseClass = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
    }

    if (!s_setGrabbed) {
        s_setGrabbed = env->GetStaticMethodID(s_mouseClass, "setGrabbed", "(Z)V");
        if (!s_setGrabbed) { SDK::ClearException(env); return; }
    }

    env->CallStaticVoidMethod(s_mouseClass, s_setGrabbed,
                              grabbed ? JNI_TRUE : JNI_FALSE);
    SDK::ClearException(env);
}

static void ForceShowCursor() {
    int n; do { n = ShowCursor(TRUE); } while (n < 0);
}

static void ReleaseCursorToGame() {
    CURSORINFO ci{ sizeof(ci) };
    GetCursorInfo(&ci);
    if (ci.flags & CURSOR_SHOWING) ShowCursor(FALSE);
}

static BOOL WINAPI hkWglSwapBuffers(HDC hdc) {
    if (!g_initDone) {
        g_hwnd = WindowFromDC(hdc);
        Overlay::Init(g_hwnd);
        ClickGUI::get().init();
        g_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(WndProc)));
        g_initDone = true;
    }

    bool guiVisible = ClickGUI::get().isVisible();

    if (guiVisible) {
        // Every frame: tell LWJGL the mouse is ungrabbed so it stops
        // centering the cursor and feeding deltas to head rotation.
        LWJGLSetGrabbed(false);
        ClipCursor(nullptr); // belt-and-suspenders at Win32 level
        if (!g_guiWasVisible)
            ForceShowCursor(); // make cursor visible on first open frame
    } else if (g_guiWasVisible) {
        // GUI just closed — explicitly re-grab. Minecraft won't do this on its
        // own unless the user clicks the window or a specific trigger fires.
        LWJGLSetGrabbed(true);
        ReleaseCursorToGame();
    }

    g_guiWasVisible = guiVisible;

    Overlay::BeginFrame();
    ArrayList::render();
    ModuleManager::get().onRender();
    ClickGUI::get().render();
    Overlay::EndFrame();

    return g_origSwapBuffers(hdc);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool guiOpen = ClickGUI::get().isVisible();

    if (guiOpen) {
        // Only feed ImGui when the GUI is visible. ImGui_ImplWin32_WndProcHandler
        // calls SetCapture() on mouse-button-down, which hijacks Minecraft's input
        // routing — calling it while the GUI is closed breaks right-click, etc.
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);

        switch (msg) {
        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
            if (ClickGUI::get().isBinding())
                ClickGUI::get().onKeyDown((int)wp);
            return 0;
        case WM_KEYUP: case WM_CHAR:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            return 0;
        }
    }

    // Pass everything else straight through using the W variant — LWJGL
    // creates a Unicode window, so CallWindowProcA would mangle messages.
    return CallWindowProcW(g_origWndProc, hwnd, msg, wp, lp);
}

bool Initialize() {
    HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
    if (!hOpenGL) return false;

    void* pSwap = reinterpret_cast<void*>(GetProcAddress(hOpenGL, "wglSwapBuffers"));
    if (!pSwap) return false;

    if (MH_Initialize() != MH_OK) return false;

    if (MH_CreateHook(pSwap, &hkWglSwapBuffers,
                      reinterpret_cast<void**>(&g_origSwapBuffers)) != MH_OK)
        return false;

    return MH_EnableHook(pSwap) == MH_OK;
}

void Shutdown() {
    if (s_mouseClass) {
        JNIEnv* env = SDK::GetEnv();
        if (env) env->DeleteGlobalRef(s_mouseClass);
        s_mouseClass = nullptr;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_hwnd && g_origWndProc)
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_origWndProc));

    Overlay::Shutdown();
}

} // namespace Hooking
