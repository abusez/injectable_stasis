#include "../pch.hpp"
#include "ClickGUI.hpp"
#include "../ModuleManager/ModuleManager.hpp"
#include "../ModuleManager/Module.hpp"
#include "../KeybindHandler/KeybindHandler.hpp"

// ---- Color palette -------------------------------------------------------
static constexpr ImU32 COL_BG       = IM_COL32( 14,  14,  14, 252);
static constexpr ImU32 COL_PANEL    = IM_COL32( 20,  20,  20, 255);
static constexpr ImU32 COL_SIDEBAR  = IM_COL32( 11,  11,  11, 255);
static constexpr ImU32 COL_SEP      = IM_COL32( 35,  35,  35, 255);
static constexpr ImU32 COL_HOVER    = IM_COL32( 30,  30,  30, 255);
static constexpr ImU32 COL_SELECTED = IM_COL32( 22,  22,  22, 255);
static constexpr ImU32 COL_ACCENT   = IM_COL32(  0, 230, 120, 255);
static constexpr ImU32 COL_TEXT     = IM_COL32(240, 240, 240, 255);
static constexpr ImU32 COL_TEXT_DIM = IM_COL32(110, 110, 110, 255);
static constexpr ImU32 COL_BORDER   = IM_COL32( 38,  38,  38, 255);

static constexpr float WIN_W  = 760.f;
static constexpr float WIN_H  = 450.f;
static constexpr float SIDE_W = 160.f;
static constexpr float HDR_H  =  44.f;
static constexpr float TAB_H  =  32.f;
static constexpr float CONT_Y = HDR_H + TAB_H;

struct CatInfo { Category cat; const char* icon; const char* label; };
static const CatInfo kCategories[] = {
    { Category::Combat,    "*", "Combat"    },
    { Category::Render,    "~", "Render"    },
    { Category::Movement,  "^", "Movement"  },
    { Category::Utility,   "+", "Utility"   },
    { Category::World,     "o", "World"     },
    { Category::Inventory, "=", "Inventory" },
    { Category::Legit,     "L", "Legit"     },
};
static constexpr int kCatCount = (int)(sizeof(kCategories)/sizeof(kCategories[0]));

// ---- Helpers -------------------------------------------------------------

static float lerp(float a, float b, float t) { return a + (b-a)*t; }

static const char* VKName(int vk) {
    if (vk <= 0) return "NONE";
    static char buf[16];
    if (vk >= 'A' && vk <= 'Z') { buf[0]=(char)vk; buf[1]=0; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0]=(char)vk; buf[1]=0; return buf; }
    switch (vk) {
        case VK_F1:      return "F1";    case VK_F2:  return "F2";
        case VK_F3:      return "F3";    case VK_F4:  return "F4";
        case VK_F5:      return "F5";    case VK_F6:  return "F6";
        case VK_F7:      return "F7";    case VK_F8:  return "F8";
        case VK_F9:      return "F9";    case VK_F10: return "F10";
        case VK_F11:     return "F11";   case VK_F12: return "F12";
        case VK_SPACE:   return "SPACE"; case VK_RETURN:  return "ENTER";
        case VK_TAB:     return "TAB";   case VK_CAPITAL: return "CAPS";
        case VK_SHIFT:   return "SHIFT"; case VK_CONTROL: return "CTRL";
        case VK_MENU:    return "ALT";   case VK_ESCAPE:  return "ESC";
        case VK_LSHIFT:  return "LSHIFT";case VK_RSHIFT:  return "RSHIFT";
        case VK_LCONTROL:return "LCTRL"; case VK_RCONTROL:return "RCTRL";
        case VK_INSERT:  return "INS";   case VK_DELETE:  return "DEL";
        case VK_HOME:    return "HOME";  case VK_END:     return "END";
        case VK_PRIOR:   return "PGUP";  case VK_NEXT:    return "PGDN";
        default: snprintf(buf, sizeof(buf), "0x%02X", vk); return buf;
    }
}

// Small pill-shaped label button used for keybind display.
// Returns true if clicked.
static bool KeybindButton(ImDrawList* dl, ImVec2 wpos, float x, float y,
                          const char* label, bool active) {
    const float BW = 52.f, BH = 18.f;
    ImVec2 bmin(wpos.x + x, wpos.y + y);
    ImVec2 bmax(bmin.x + BW, bmin.y + BH);

    ImU32 bg  = active ? COL_ACCENT : IM_COL32(18,18,18,255);
    ImU32 brd = active ? IM_COL32(0,255,140,255) : COL_BORDER;
    ImU32 fg  = active ? IM_COL32(0,0,0,255) : COL_TEXT;

    // Hover
    ImVec2 mp = ImGui::GetMousePos();
    bool hov  = mp.x >= bmin.x && mp.x <= bmax.x && mp.y >= bmin.y && mp.y <= bmax.y;
    if (hov && !active) bg = IM_COL32(30,30,30,255);

    dl->AddRectFilled(bmin, bmax, bg, 3.f);
    dl->AddRect(bmin, bmax, brd, 3.f, 0, 1.f);

    ImVec2 tsz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(bmin.x + (BW-tsz.x)*0.5f, bmin.y + (BH-tsz.y)*0.5f), fg, label);

    return hov && ImGui::IsMouseClicked(0);
}

// ---- onKeyDown -----------------------------------------------------------

void ClickGUI::onKeyDown(int vk) {
    if (vk == VK_ESCAPE) {
        m_bindingModule = nullptr;
        m_bindingGuiKey = false;
        return;
    }
    if (m_bindingModule) {
        m_bindingModule->keyBind = vk;
        m_bindingModule = nullptr;
        return;
    }
    if (m_bindingGuiKey) {
        KeybindHandler::g_guiKey = vk;
        m_bindingGuiKey = false;
    }
}

// ---- Toggle --------------------------------------------------------------

void ClickGUI::drawToggle(ImDrawList* dl, ImVec2 pos, bool enabled, float& anim) {
    const float W = 34.f, H = 18.f, R = H*0.5f;
    anim = lerp(anim, enabled ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 14.f);
    ImU32 bg = IM_COL32((int)lerp(55,0,anim),(int)lerp(55,230,anim),(int)lerp(55,120,anim),255);
    dl->AddRectFilled(pos, ImVec2(pos.x+W, pos.y+H), bg, R);
    float tx = pos.x + R + anim*(W-H);
    dl->AddCircleFilled(ImVec2(tx, pos.y+R), R-2.f, IM_COL32(255,255,255,255));
}

// ---- Theme ---------------------------------------------------------------

void ClickGUI::applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.05f,0.05f,0.05f,1.f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.05f,0.05f,0.05f,0.f);
    c[ImGuiCol_Text]              = ImVec4(0.94f,0.94f,0.94f,1.f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.43f,0.43f,0.43f,1.f);
    c[ImGuiCol_Border]            = ImVec4(0.15f,0.15f,0.15f,1.f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.10f,0.10f,0.10f,1.f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.15f,0.15f,0.15f,1.f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.20f,0.20f,0.20f,1.f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.00f,0.90f,0.47f,1.f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.00f,1.00f,0.55f,1.f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.00f,0.90f,0.47f,1.f);
    c[ImGuiCol_Button]            = ImVec4(0.12f,0.12f,0.12f,1.f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.18f,0.18f,0.18f,1.f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.24f,0.24f,0.24f,1.f);
    c[ImGuiCol_Header]            = ImVec4(0.15f,0.15f,0.15f,1.f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.20f,0.20f,0.20f,1.f);
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.06f,0.06f,0.06f,1.f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.20f,0.20f,0.20f,1.f);
    c[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.28f,0.28f,0.28f,1.f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.10f,0.10f,0.10f,1.f);
    s.WindowRounding    = 10.f; s.ChildRounding     =  6.f;
    s.FrameRounding     =  4.f; s.GrabRounding      =  4.f;
    s.ScrollbarRounding =  4.f; s.PopupRounding     =  6.f;
    s.WindowBorderSize  =  1.f; s.FrameBorderSize   =  0.f;
    s.ItemSpacing = ImVec2(8,5); s.FramePadding = ImVec2(8,4);
}

// ---- Init ----------------------------------------------------------------

void ClickGUI::init() { applyTheme(); m_themeApplied = true; }

// ---- Render --------------------------------------------------------------

void ClickGUI::render() {
    if (!m_visible) return;
    if (!m_themeApplied) { applyTheme(); m_themeApplied = true; }

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x*0.5f - WIN_W*0.5f, io.DisplaySize.y*0.5f - WIN_H*0.5f),
        ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(WIN_W, WIN_H), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##stasis_gui", nullptr, wf);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Main background
    dl->AddRectFilled(wp, ImVec2(wp.x+WIN_W, wp.y+WIN_H), COL_BG, 10.f);
    dl->AddRect(wp, ImVec2(wp.x+WIN_W, wp.y+WIN_H), COL_BORDER, 10.f, 0, 1.f);

    // ---- Header ----------------------------------------------------------
    {
        dl->AddRectFilled(wp, ImVec2(wp.x+WIN_W, wp.y+HDR_H), COL_PANEL, 10.f,
                          ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(wp.x, wp.y+HDR_H), ImVec2(wp.x+WIN_W, wp.y+HDR_H), COL_SEP);

        ImGui::SetCursorPos(ImVec2(16.f, 12.f));
        ImGui::TextColored(ImVec4(0.f,0.9f,0.47f,1.f), "STASIS");
        ImGui::SameLine(0,6);
        ImGui::TextColored(ImVec4(0.4f,0.4f,0.4f,1.f), "v1.0");

        // GUI rebind button — shows current key, click to rebind
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "GUI: %s", VKName(KeybindHandler::g_guiKey));
            bool bindingGui = m_bindingGuiKey;
            const char* label = bindingGui ? "PRESS KEY" : buf;
            if (KeybindButton(dl, wp, WIN_W - 170.f, (HDR_H - 18.f)*0.5f, label, bindingGui)) {
                m_bindingGuiKey  = !m_bindingGuiKey;
                m_bindingModule  = nullptr;
            }
        }

        // Close button
        ImGui::SetCursorPos(ImVec2(WIN_W-26.f, 13.f));
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f,0.25f,0.25f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f,0.35f,0.35f,1));
        if (ImGui::Button("X##close", ImVec2(18,18))) m_visible = false;
        ImGui::PopStyleColor(3);
    }

    // ---- Tab bar ---------------------------------------------------------
    {
        float tabY = HDR_H;
        dl->AddRectFilled(ImVec2(wp.x, wp.y+tabY), ImVec2(wp.x+WIN_W, wp.y+tabY+TAB_H), COL_PANEL);
        dl->AddLine(ImVec2(wp.x, wp.y+tabY+TAB_H), ImVec2(wp.x+WIN_W, wp.y+tabY+TAB_H), COL_SEP);

        const char* tabs[] = {"Modules","Friends","Profiles"};
        float tabX = SIDE_W + 16.f;
        for (int i = 0; i < 3; i++) {
            ImGui::SetCursorPos(ImVec2(tabX, tabY+7.f));
            bool sel = (i == 0);
            ImGui::TextColored(sel ? ImVec4(0.f,0.9f,0.47f,1.f) : ImVec4(0.45f,0.45f,0.45f,1.f), tabs[i]);
            if (sel) {
                ImVec2 tsz = ImGui::CalcTextSize(tabs[i]);
                float lx = wp.x+tabX, ly = wp.y+tabY+TAB_H-2.f;
                dl->AddLine(ImVec2(lx,ly), ImVec2(lx+tsz.x,ly), COL_ACCENT, 2.f);
            }
            tabX += ImGui::CalcTextSize(tabs[i]).x + 24.f;
        }
    }

    // ---- Sidebar ---------------------------------------------------------
    {
        float sideY = CONT_Y;
        dl->AddRectFilled(ImVec2(wp.x, wp.y+sideY), ImVec2(wp.x+SIDE_W, wp.y+WIN_H),
                          COL_SIDEBAR, 10.f, ImDrawFlags_RoundCornersBottomLeft);
        dl->AddLine(ImVec2(wp.x+SIDE_W, wp.y+sideY), ImVec2(wp.x+SIDE_W, wp.y+WIN_H), COL_SEP);

        float itemH = 38.f;
        for (int i = 0; i < kCatCount; i++) {
            float iy = sideY + i*itemH;
            bool sel = (m_selectedCat == i);
            ImGui::SetCursorPos(ImVec2(0.f, iy));
            ImGui::InvisibleButton(kCategories[i].label, ImVec2(SIDE_W, itemH));
            bool hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) m_selectedCat = i;

            if (sel) {
                dl->AddRectFilled(ImVec2(wp.x, wp.y+iy), ImVec2(wp.x+SIDE_W, wp.y+iy+itemH), COL_SELECTED);
                dl->AddRectFilled(ImVec2(wp.x, wp.y+iy+6.f), ImVec2(wp.x+3.f, wp.y+iy+itemH-6.f), COL_ACCENT, 2.f);
            } else if (hov) {
                dl->AddRectFilled(ImVec2(wp.x, wp.y+iy), ImVec2(wp.x+SIDE_W, wp.y+iy+itemH), COL_HOVER);
            }
            dl->AddText(ImVec2(wp.x+14.f, wp.y+iy+11.f), sel ? COL_ACCENT : COL_TEXT_DIM, kCategories[i].icon);
            dl->AddText(ImVec2(wp.x+32.f, wp.y+iy+11.f), sel ? COL_TEXT  : COL_TEXT_DIM, kCategories[i].label);
            if (i < kCatCount-1)
                dl->AddLine(ImVec2(wp.x+12.f, wp.y+iy+itemH), ImVec2(wp.x+SIDE_W-12.f, wp.y+iy+itemH), IM_COL32(30,30,30,200));
        }
    }

    // ---- Module list -----------------------------------------------------
    {
        float listX = SIDE_W, listY = CONT_Y;
        float listW = WIN_W - SIDE_W, listH = WIN_H - CONT_Y;
        const float SEARCH_H = 34.f;

        // Search bar
        dl->AddRectFilled(ImVec2(wp.x+listX, wp.y+listY), ImVec2(wp.x+WIN_W, wp.y+listY+SEARCH_H), COL_PANEL);
        dl->AddLine(ImVec2(wp.x+listX, wp.y+listY+SEARCH_H), ImVec2(wp.x+WIN_W, wp.y+listY+SEARCH_H), COL_SEP);
        dl->AddText(ImVec2(wp.x+listX+10.f, wp.y+listY+9.f), COL_TEXT_DIM, "?");
        ImGui::SetCursorPos(ImVec2(listX+28.f, listY+7.f));
        ImGui::SetNextItemWidth(listW-42.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
        ImGui::InputText("##search", m_search, sizeof(m_search));
        ImGui::PopStyleColor();
        if (m_search[0]=='\0') {
            ImVec2 pos = ImGui::GetItemRectMin();
            dl->AddText(ImVec2(pos.x, pos.y+1.f), COL_TEXT_DIM, "Search modules...");
        }

        // Module rows (scrollable child)
        ImGui::SetCursorPos(ImVec2(listX, listY+SEARCH_H));
        ImGui::BeginChild("##modlist", ImVec2(listW, listH-SEARCH_H), false, ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 cwp = ImGui::GetWindowPos();

        Category selCat   = kCategories[m_selectedCat].cat;
        bool     searching = m_search[0] != '\0';
        float    rowH     = 52.f;
        float    curY     = 0.f;

        for (auto& mod : ModuleManager::get().getModules()) {
            if (!searching && mod->category != selCat) continue;
            if (searching) {
                std::string n(mod->name), q(m_search);
                auto it = std::search(n.begin(), n.end(), q.begin(), q.end(),
                    [](char a,char b){ return std::tolower(a)==std::tolower(b); });
                if (it == n.end()) continue;
            }

            // Expanded section: settings rows + 1 keybind row (always shown when expanded)
            float expandH = 0.f;
            if (mod->expanded) {
                for (auto& s : mod->settings)
                    expandH += (s.type == SettingType::BOOL ? 22.f : 28.f);
                expandH += 28.f; // keybind row
                expandH += 8.f;  // bottom padding
            }
            float totalH = rowH + expandH;

            // Row background
            ImVec2 rowMin(cwp.x, cwp.y+curY), rowMax(cwp.x+listW, cwp.y+curY+totalH);
            if (ImGui::IsMouseHoveringRect(rowMin, rowMax))
                cdl->AddRectFilled(rowMin, rowMax, COL_HOVER);

            // Module name
            cdl->AddText(ImVec2(cwp.x+14.f, cwp.y+curY+12.f),
                         mod->enabled ? COL_ACCENT : COL_TEXT, mod->name);

            // Description
            std::string desc = mod->getDescription();
            if (!desc.empty()) {
                if (desc.size() > 55) desc = desc.substr(0,52)+"...";
                cdl->AddText(ImVec2(cwp.x+14.f, cwp.y+curY+30.f), COL_TEXT_DIM, desc.c_str());
            }

            // Toggle (right edge)
            {
                const float TW=34.f, TH=18.f;
                float tx = listW-TW-14.f;
                float ty = curY+(rowH-TH)*0.5f;
                auto& anim = m_toggleAnim[mod->name];
                ImGui::SetCursorPos(ImVec2(tx-4.f, ty-4.f));
                ImGui::InvisibleButton(("##tog_"+std::string(mod->name)).c_str(), ImVec2(TW+8.f,TH+8.f));
                if (ImGui::IsItemClicked()) mod->toggle();
                drawToggle(cdl, ImVec2(cwp.x+tx, cwp.y+ty), mod->enabled, anim);
            }

            // Row click to expand/collapse (right-click)
            {
                ImGui::SetCursorPos(ImVec2(0.f, curY));
                ImGui::InvisibleButton(("##row_"+std::string(mod->name)).c_str(), ImVec2(listW-80.f, rowH));
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    mod->expanded = !mod->expanded;
                    if (!mod->expanded && m_bindingModule == mod.get())
                        m_bindingModule = nullptr;
                }
            }

            // Expanded settings + keybind
            if (mod->expanded) {
                float sy = curY + rowH;

                // Settings
                for (auto& s : mod->settings) {
                    float sh = (s.type == SettingType::BOOL) ? 22.f : 28.f;
                    ImGui::SetCursorPos(ImVec2(20.f, sy));
                    std::string id = "##set_" + s.name + mod->name;
                    switch (s.type) {
                        case SettingType::BOOL: {
                            bool v = s.boolVal;
                            ImGui::Checkbox((s.name+id).c_str(), &v);
                            s.boolVal = v; break;
                        }
                        case SettingType::INT:
                            ImGui::SetNextItemWidth(listW-100.f);
                            ImGui::SliderInt((s.name+id).c_str(), &s.intVal, s.intMin, s.intMax); break;
                        case SettingType::FLOAT:
                            ImGui::SetNextItemWidth(listW-100.f);
                            ImGui::SliderFloat((s.name+id).c_str(), &s.floatVal, s.floatMin, s.floatMax, "%.2f"); break;
                        case SettingType::ENUM: {
                            ImGui::SetNextItemWidth(listW-100.f);
                            const char* prev = s.enumOpts.empty() ? "" : s.enumOpts[s.enumIdx].c_str();
                            if (ImGui::BeginCombo((s.name+id).c_str(), prev)) {
                                for (int ei=0; ei<(int)s.enumOpts.size(); ei++) {
                                    bool sel = (s.enumIdx==ei);
                                    if (ImGui::Selectable(s.enumOpts[ei].c_str(), sel)) s.enumIdx=ei;
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            break;
                        }
                    }
                    sy += sh;
                }

                // Keybind row
                {
                    cdl->AddText(ImVec2(cwp.x+20.f, cwp.y+sy+5.f), COL_TEXT_DIM, "Keybind");

                    bool isBinding = (m_bindingModule == mod.get());
                    const char* kLabel = isBinding ? "PRESS KEY" : VKName(mod->keyBind);

                    if (KeybindButton(cdl, cwp, 80.f, sy+2.f, kLabel, isBinding)) {
                        if (isBinding)
                            m_bindingModule = nullptr;
                        else {
                            m_bindingModule = mod.get();
                            m_bindingGuiKey = false;
                        }
                    }

                    // Clear bind button ("X")
                    if (!isBinding) {
                        if (KeybindButton(cdl, cwp, 140.f, sy+2.f, "CLR", false)) {
                            mod->keyBind = 0;
                            m_bindingModule = nullptr;
                        }
                    }

                    sy += 28.f;
                }
            }

            curY += totalH;
            cdl->AddLine(ImVec2(cwp.x+10.f, cwp.y+curY), ImVec2(cwp.x+listW-10.f, cwp.y+curY), IM_COL32(30,30,30,180));
        }

        ImGui::Dummy(ImVec2(0, curY));
        ImGui::EndChild();
    }

    ImGui::End();
}
