#pragma once

#include "unit.h"

namespace Game::Units {

class GravePriest : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<GravePriest>;

private:
  GravePriest(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
