#pragma once
#include "Module.hpp"
#include <vector>
#include <memory>
#include <string>

class ModuleManager {
public:
    static ModuleManager& get() { static ModuleManager i; return i; }

    void init();
    void onTick();
    void onRender();

    Module* getModule(const std::string& name) const;
    const std::vector<std::unique_ptr<Module>>& getModules() const { return m_modules; }

private:
    ModuleManager() = default;
    std::vector<std::unique_ptr<Module>> m_modules;
};
