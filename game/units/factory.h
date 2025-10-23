#pragma once

#include "unit.h"
#include "spawn_type.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Game {
namespace Units {

class UnitFactoryRegistry {
public:
  using Factory = std::function<std::unique_ptr<Unit>(Engine::Core::World &,
                                                      const SpawnParams &)>;

  void registerFactory(SpawnType type, Factory f) {
    m_factories[type] = std::move(f);
  }

  std::unique_ptr<Unit> create(SpawnType type, Engine::Core::World &world,
                               const SpawnParams &params) const {
    auto it = m_factories.find(type);
    if (it == m_factories.end())
      return nullptr;
    return it->second(world, params);
  }

  std::unique_ptr<Unit> create(TroopType type, Engine::Core::World &world,
                               const SpawnParams &params) const {
    const SpawnType spawnType = spawnTypeFromTroopType(type);
    return create(spawnType, world, params);
  }

private:
  std::unordered_map<SpawnType, Factory> m_factories;
};

void registerBuiltInUnits(UnitFactoryRegistry &reg);

} // namespace Units
} // namespace Game
