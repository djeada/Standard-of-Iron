#pragma once

#include "unit.h"

namespace Game::Units {

class HorseArcher : public Unit {
public:
  static auto Create(Engine::Core::World &world, const SpawnParams &params)
      -> std::unique_ptr<HorseArcher>;

private:
  HorseArcher(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Game::Units
