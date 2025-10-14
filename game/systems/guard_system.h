#pragma once

#include "../core/system.h"

namespace Game::Systems {

class GuardSystem : public Engine::Core::System {
public:
  GuardSystem() = default;
  ~GuardSystem() override = default;

  void update(Engine::Core::World *world, float deltaTime) override;
};

} // namespace Game::Systems
