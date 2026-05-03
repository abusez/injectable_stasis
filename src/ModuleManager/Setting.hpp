#pragma once
#include <string>
#include <vector>

enum class SettingType { BOOL, INT, FLOAT, ENUM };

struct Setting {
    std::string name;
    SettingType type = SettingType::BOOL;

    bool  boolVal  = false;
    int   intVal   = 0,   intMin   = 0,   intMax   = 100;
    float floatVal = 0.f, floatMin = 0.f, floatMax = 1.f;
    int   enumIdx  = 0;
    std::vector<std::string> enumOpts;

    static Setting Bool(const std::string& n, bool def) {
        Setting s; s.name = n; s.type = SettingType::BOOL; s.boolVal = def; return s;
    }
    static Setting Int(const std::string& n, int mn, int mx, int def) {
        Setting s; s.name = n; s.type = SettingType::INT;
        s.intMin = mn; s.intMax = mx; s.intVal = def; return s;
    }
    static Setting Float(const std::string& n, float mn, float mx, float def) {
        Setting s; s.name = n; s.type = SettingType::FLOAT;
        s.floatMin = mn; s.floatMax = mx; s.floatVal = def; return s;
    }
    static Setting Enum(const std::string& n, std::vector<std::string> opts, int def = 0) {
        Setting s; s.name = n; s.type = SettingType::ENUM;
        s.enumOpts = std::move(opts); s.enumIdx = def; return s;
    }
};
