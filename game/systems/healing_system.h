#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class HealingSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  void processHealing(Engine::Core::World *world, float deltaTime);
};

} // namespace Game::Systems
