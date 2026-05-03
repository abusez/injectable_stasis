#pragma once
#include <unordered_map>
#include <string>

class Module;

class ClickGUI {
public:
    static ClickGUI& get() { static ClickGUI i; return i; }

    void init();
    void render();
    void toggle() { m_visible = !m_visible; }
    bool isVisible()  const { return m_visible; }
    bool isBinding()  const { return m_bindingModule != nullptr || m_bindingGuiKey; }

    // Called from WndProc when a key is pressed while GUI is open and in bind mode.
    void onKeyDown(int vk);

private:
    ClickGUI() = default;

    void applyTheme();
    void drawToggle(ImDrawList* dl, ImVec2 pos, bool enabled, float& anim);

    bool    m_visible       = false;
    int     m_selectedCat   = 0;
    char    m_search[128]   = {};
    bool    m_themeApplied  = false;

    // Keybind capture state
    Module* m_bindingModule = nullptr; // module being rebound (null = none)
    bool    m_bindingGuiKey = false;   // true when rebinding the GUI toggle key

    std::unordered_map<std::string, float> m_toggleAnim;
};
