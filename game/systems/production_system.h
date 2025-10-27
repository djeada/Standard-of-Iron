#pragma once

#include "../core/system.h"

namespace Game::Systems {

class ProductionSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;
};

} // namespace Game::Systems
