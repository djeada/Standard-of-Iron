#pragma once

#include "unit.h"

namespace Game::Units {

// The buildings only one nation raises (NATION_ASYMMETRY.md): Carthage's
// Mercenary Camp and Elephant Stables, Rome's Fabrica and Castra. The two
// hiring buildings carry a ProductionComponent with no reserve of men: they
// hire for gold (and food) alone.
class NationBuilding : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<NationBuilding>;

private:
  NationBuilding(Engine::Core::World& world, SpawnType type);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
