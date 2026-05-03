#include "../pch.hpp"
#include "ModuleManager.hpp"
#include "Modules/Movement/Stasis.hpp"

void ModuleManager::init() {
    m_modules.push_back(std::make_unique<Stasis>());
}

void ModuleManager::onTick() {
    for (auto& m : m_modules)
        if (m->enabled) m->onTick();
}

void ModuleManager::onRender() {
    for (auto& m : m_modules)
        if (m->enabled) m->onRender();
}

Module* ModuleManager::getModule(const std::string& name) const {
    for (auto& m : m_modules)
        if (m->name == name) return m.get();
    return nullptr;
}
