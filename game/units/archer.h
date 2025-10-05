#pragma once

#include "unit.h"

namespace Game {
namespace Units {

class Archer : public Unit {
public:
  static std::unique_ptr<Archer> Create(Engine::Core::World &world,
                                        const SpawnParams &params);

private:
  Archer(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Units
} // namespace Game
