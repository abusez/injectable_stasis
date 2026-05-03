#include "../../pch.hpp"
#include "overlay.hpp"

namespace Overlay {

static bool s_init = false;

void Init(HWND hwnd) {
    if (s_init) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // we manage cursor manually
    io.MouseDrawCursor = false; // no software cursor — we show the Windows cursor instead
    io.IniFilename     = nullptr;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 110");

    s_init = true;
}

void BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Shutdown() {
    if (!s_init) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_init = false;
}

} // namespace Overlay
