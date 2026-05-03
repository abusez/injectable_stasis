#pragma once
#include <Windows.h>

namespace Hooking {
    bool Initialize();
    void Shutdown();

    // WndProc replacement — forward mouse/keyboard input to ImGui while GUI is open.
    extern LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    inline HWND   g_hwnd        = nullptr;
    inline WNDPROC g_origWndProc = nullptr;
}
