#pragma once

#include "unit.h"

namespace Game::Units {

class Marketplace : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<Marketplace>;

private:
  Marketplace(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
