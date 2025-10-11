#pragma once

#include "unit.h"

namespace Game {
namespace Units {

class Barracks : public Unit {
public:
  static std::unique_ptr<Barracks> Create(Engine::Core::World &world,
                                          const SpawnParams &params);

private:
  Barracks(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Units
} // namespace Game
