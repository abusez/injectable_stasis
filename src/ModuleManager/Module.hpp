#pragma once
#include "Setting.hpp"
#include <string>
#include <vector>

enum class Category { Combat, Render, Movement, Utility, World, Inventory, Legit };

inline const char* CategoryName(Category c) {
    switch (c) {
        case Category::Combat:    return "Combat";
        case Category::Render:    return "Render";
        case Category::Movement:  return "Movement";
        case Category::Utility:   return "Utility";
        case Category::World:     return "World";
        case Category::Inventory: return "Inventory";
        case Category::Legit:     return "Legit";
        default:                  return "Unknown";
    }
}

class Module {
public:
    Module(const char* name, Category cat, int defaultKey = 0)
        : name(name), category(cat), keyBind(defaultKey) {}
    virtual ~Module() = default;

    void toggle() { enabled = !enabled; enabled ? onEnable() : onDisable(); }

    virtual void onEnable()  {}
    virtual void onDisable() {}
    virtual void onTick()    {}
    virtual void onRender()  {}

    // --- Settings helpers ---
    void addBool(const std::string& n, bool def = false)
        { settings.push_back(Setting::Bool(n, def)); }
    void addInt(const std::string& n, int mn, int mx, int def)
        { settings.push_back(Setting::Int(n, mn, mx, def)); }
    void addFloat(const std::string& n, float mn, float mx, float def)
        { settings.push_back(Setting::Float(n, mn, mx, def)); }
    void addEnum(const std::string& n, std::vector<std::string> opts, int def = 0)
        { settings.push_back(Setting::Enum(n, std::move(opts), def)); }

    Setting* getSetting(const std::string& n) {
        for (auto& s : settings) if (s.name == n) return &s;
        return nullptr;
    }

    bool  getBool(const std::string& n)  { auto* s = getSetting(n); return s ? s->boolVal  : false; }
    int   getInt(const std::string& n)   { auto* s = getSetting(n); return s ? s->intVal   : 0; }
    float getFloat(const std::string& n) { auto* s = getSetting(n); return s ? s->floatVal : 0.f; }
    int   getEnum(const std::string& n)  { auto* s = getSetting(n); return s ? s->enumIdx  : 0; }

    // Short description shown in the GUI below the module name
    std::string getDescription() const {
        std::string d;
        for (const auto& s : settings) {
            std::string part;
            switch (s.type) {
                case SettingType::BOOL:
                    if (!s.boolVal) continue;
                    part = s.name; break;
                case SettingType::INT: {
                    char buf[32]; snprintf(buf, sizeof(buf), "%d %s", s.intVal, s.name.c_str());
                    part = buf; break;
                }
                case SettingType::FLOAT: {
                    char buf[32]; snprintf(buf, sizeof(buf), "%.1f %s", s.floatVal, s.name.c_str());
                    part = buf; break;
                }
                case SettingType::ENUM:
                    if (!s.enumOpts.empty()) part = s.enumOpts[s.enumIdx]; break;
            }
            if (!part.empty()) { if (!d.empty()) d += ". "; d += part; }
        }
        return d;
    }

public:
    const char* name;
    Category    category;
    bool        enabled  = false;
    int         keyBind;
    bool        expanded = false; // GUI state

    std::vector<Setting> settings;
};
