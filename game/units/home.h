#pragma once

#include "unit.h"

namespace Game::Units {

class Home : public Unit {
public:
  static auto Create(Engine::Core::World &world,
                     const SpawnParams &params) -> std::unique_ptr<Home>;

private:
  Home(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} 
