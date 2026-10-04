#pragma once

#include "unit.h"

namespace Game::Units {

class WallLadder : public Unit {
public:
  static auto create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<WallLadder>;

private:
  WallLadder(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
