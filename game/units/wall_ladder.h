#pragma once

#include "unit.h"

namespace Game::Units {

// A timber ladder a builder raises against the town face of its own palisade.
// It gives troops a way up onto the balcony wherever the wall has no stair.
class WallLadder : public Unit {
public:
  static auto create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<WallLadder>;

private:
  WallLadder(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
