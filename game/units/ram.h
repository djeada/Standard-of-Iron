#pragma once

#include <memory>

#include "unit.h"

namespace Engine::Core {
class World;
}

namespace Game::Units {

class Ram : public Unit {
public:
  static auto Create(Engine::Core::World& world,
                     const SpawnParams& params) -> std::unique_ptr<Ram>;

private:
  explicit Ram(Engine::Core::World& world);
  void init(const SpawnParams& params);
};

} // namespace Game::Units
