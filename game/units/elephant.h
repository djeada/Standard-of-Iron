#pragma once

#include "unit.h"

namespace Game::Units {

class Elephant : public Unit {
public:
  static auto Create(Engine::Core::World &world,
                     const SpawnParams &params) -> std::unique_ptr<Elephant>;

private:
  Elephant(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} 
