#pragma once

#include "unit.h"

namespace Game::Units {

class WallGate : public Unit {
public:
  static auto create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<WallGate>;

private:
  WallGate(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
