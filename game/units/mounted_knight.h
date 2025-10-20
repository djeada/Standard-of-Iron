#pragma once

#include "unit.h"

namespace Game {
namespace Units {

class MountedKnight : public Unit {
public:
  static std::unique_ptr<MountedKnight> Create(Engine::Core::World &world,
                                               const SpawnParams &params);

private:
  MountedKnight(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Units
} // namespace Game
