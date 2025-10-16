#pragma once

#include "unit.h"

namespace Game {
namespace Units {

class Knight : public Unit {
public:
  static std::unique_ptr<Knight> Create(Engine::Core::World &world,
                                        const SpawnParams &params);

private:
  Knight(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Units
} // namespace Game
