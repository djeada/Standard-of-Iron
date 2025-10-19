#pragma once

#include "unit.h"

namespace Game {
namespace Units {

class Spearman : public Unit {
public:
  static std::unique_ptr<Spearman> Create(Engine::Core::World &world,
                                          const SpawnParams &params);

private:
  Spearman(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Units
} // namespace Game
