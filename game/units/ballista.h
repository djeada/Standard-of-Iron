#pragma once

#include "unit.h"
#include <memory>

namespace Engine::Core {
class World;
}

namespace Game::Units {

class Ballista : public Unit {
public:
  static auto Create(Engine::Core::World &world,
                     const SpawnParams &params) -> std::unique_ptr<Ballista>;

private:
  explicit Ballista(Engine::Core::World &world);
  void init(const SpawnParams &params);
};

} // namespace Game::Units
