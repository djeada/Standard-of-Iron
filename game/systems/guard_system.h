#pragma once

#include "../core/system.h"
#include "../core/world.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class GuardSystem : public Engine::Core::System {
public:
  GuardSystem() = default;
  ~GuardSystem() override = default;

  void update(Engine::Core::World *world, float delta_time) override;
};

} // namespace Game::Systems
