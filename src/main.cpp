#include "pch.hpp"
#include "SDK/sdk.hpp"
#include "Rendering/Hooking/hooking.hpp"
#include "ModuleManager/ModuleManager.hpp"
#include "GUI/ClickGUI.hpp"
#include "KeybindHandler/KeybindHandler.hpp"

static DWORD WINAPI MainThread(LPVOID) {
    // Wait for Minecraft to finish loading OpenGL
    while (!GetModuleHandleA("opengl32.dll"))
        Sleep(100);
    Sleep(2000);

    // Attach to the JVM that is running Minecraft
    if (!SDK::Init()) {
        MessageBoxA(nullptr, "Stasis: failed to attach to JVM.\n"
                             "Make sure Minecraft is already running.",
                    "Stasis", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Register all modules
    ModuleManager::get().init();

    // ClickGUI theme is applied inside the first wglSwapBuffers call, after
    // the ImGui context is created. Do not call ClickGUI::get().init() here.

    // Hook wglSwapBuffers to inject rendering every frame
    if (!Hooking::Initialize()) {
        MessageBoxA(nullptr, "Stasis: failed to install rendering hook.",
                    "Stasis", MB_OK | MB_ICONERROR);
        SDK::Shutdown();
        return 1;
    }

    // Main loop:
    //   END          — unload the DLL
    //   Right Shift  — toggle click GUI  (handled inside KeybindHandler)
    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        ModuleManager::get().onTick();
        KeybindHandler::poll();
        Sleep(10);
    }

    Hooking::Shutdown();
    SDK::Shutdown();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
