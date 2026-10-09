#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "spawn_type.h"
#include "unit.h"

namespace Game::Units {

class UnitFactoryRegistry {
public:
  using Factory =
      std::function<std::unique_ptr<Unit>(Engine::Core::World&, const SpawnParams&)>;

  void register_factory(SpawnType type, Factory f) { m_factories[type] = std::move(f); }

  auto create(SpawnType type,
              Engine::Core::World& world,
              const SpawnParams& params) const -> std::unique_ptr<Unit>;

  auto create(TroopType type,
              Engine::Core::World& world,
              const SpawnParams& params) const -> std::unique_ptr<Unit> {
    const SpawnType spawn_type = spawn_typeFromTroopType(type);
    return create(spawn_type, world, params);
  }

private:
  static void
  apply_forest_passability(Engine::Core::World& world, Unit* unit, SpawnType type);
  static void apply_historical_commander(Engine::Core::World& world,
                                         Unit* unit,
                                         const SpawnParams& params);

  std::unordered_map<SpawnType, Factory> m_factories;
};

void register_built_in_units(UnitFactoryRegistry& reg);

} // namespace Game::Units
