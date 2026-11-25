#pragma once

#include "unit.h"

namespace Game::Units {

class NationBuilding : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<NationBuilding>;

private:
  NationBuilding(Engine::Core::World& world, SpawnType type);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
