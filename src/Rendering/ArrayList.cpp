#include "../pch.hpp"
#include "ArrayList.hpp"
#include "../ModuleManager/ModuleManager.hpp"

namespace ArrayList {

void render() {
    ImGuiIO&    io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const float PADDING  = 4.f;
    const float LINE_H   = 16.f;
    const float BAR_W    = 2.f;   // right accent bar thickness
    const float RIGHT_X  = io.DisplaySize.x - PADDING;

    float y = PADDING;

    for (const auto& mod : ModuleManager::get().getModules()) {
        if (!mod->enabled) continue;

        ImVec2 tsz = ImGui::CalcTextSize(mod->name);
        float textX = RIGHT_X - BAR_W - PADDING - tsz.x;

        // Dark background slab
        dl->AddRectFilled(
            ImVec2(textX - 3.f, y),
            ImVec2(RIGHT_X,     y + LINE_H),
            IM_COL32(14, 14, 14, 180), 2.f
        );

        // Green accent bar on far right
        dl->AddRectFilled(
            ImVec2(RIGHT_X - BAR_W, y),
            ImVec2(RIGHT_X,          y + LINE_H),
            IM_COL32(0, 230, 120, 255)
        );

        // Module name
        dl->AddText(
            ImVec2(textX, y + (LINE_H - ImGui::GetTextLineHeight()) * 0.5f),
            IM_COL32(240, 240, 240, 255), mod->name
        );

        y += LINE_H + 2.f;
    }
}

} // namespace ArrayList
