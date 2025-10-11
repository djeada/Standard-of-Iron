#pragma once

#include "../core/system.h"

namespace Game::Systems {

class PatrolSystem : public Engine::Core::System {
public:
  PatrolSystem() = default;
  ~PatrolSystem() override = default;

  void update(Engine::Core::World *world, float deltaTime) override;
};

} // namespace Game::Systems
