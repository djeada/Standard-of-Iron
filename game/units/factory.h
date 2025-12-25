#pragma once

#include "spawn_type.h"
#include "unit.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Game::Units {

class UnitFactoryRegistry {
public:
  using Factory = std::function<std::unique_ptr<Unit>(Engine::Core::World &,
                                                      const SpawnParams &)>;

  void registerFactory(SpawnType type, Factory f) {
    m_factories[type] = std::move(f);
  }

  auto create(SpawnType type, Engine::Core::World &world,
              const SpawnParams &params) const -> std::unique_ptr<Unit> {
    auto it = m_factories.find(type);
    if (it == m_factories.end()) {
      return nullptr;
    }
    return it->second(world, params);
  }

  auto create(TroopType type, Engine::Core::World &world,
              const SpawnParams &params) const -> std::unique_ptr<Unit> {
    const SpawnType spawn_type = spawn_typeFromTroopType(type);
    return create(spawn_type, world, params);
  }

private:
  std::unordered_map<SpawnType, Factory> m_factories;
};

void registerBuiltInUnits(UnitFactoryRegistry &reg);

} // namespace Game::Units
