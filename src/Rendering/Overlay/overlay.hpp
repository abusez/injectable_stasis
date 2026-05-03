#pragma once
#include <Windows.h>

namespace Overlay {
    void Init(HWND hwnd);
    void BeginFrame();
    void EndFrame();
    void Shutdown();
}
