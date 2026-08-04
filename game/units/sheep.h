#pragma once

#include "unit.h"

namespace Game::Units {

class Sheep : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<Sheep>;

private:
  explicit Sheep(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
