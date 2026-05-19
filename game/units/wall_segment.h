#pragma once

#include "unit.h"

namespace Game::Units {

class WallSegment : public Unit {
public:
  static auto create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<WallSegment>;

private:
  WallSegment(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
