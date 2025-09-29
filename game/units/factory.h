#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "unit.h"

namespace Game { namespace Units {

class UnitFactoryRegistry {
public:
    using Factory = std::function<std::unique_ptr<Unit>(Engine::Core::World&, const SpawnParams&)>;

    void registerFactory(const std::string& type, Factory f) { m_map[type] = std::move(f); }
    std::unique_ptr<Unit> create(const std::string& type, Engine::Core::World& world, const SpawnParams& params) const {
        auto it = m_map.find(type);
        if (it == m_map.end()) return nullptr;
        return it->second(world, params);
    }
private:
    std::unordered_map<std::string, Factory> m_map;
};

// Install built-in unit factories (archer, etc.)
void registerBuiltInUnits(UnitFactoryRegistry& reg);

} } // namespace Game::Units
