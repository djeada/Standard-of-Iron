#pragma once

#include "unit.h"

namespace Game::Units {

class DefenseTower : public Unit {
public:
  static auto create(Engine::Core::World &world, const SpawnParams &params)
      -> std::unique_ptr<DefenseTower>;

private:
  DefenseTower(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} 
