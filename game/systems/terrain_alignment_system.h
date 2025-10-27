#pragma once

#include "../core/system.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class TerrainAlignmentSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  static void alignEntityToTerrain(Engine::Core::Entity *entity);
};

} // namespace Game::Systems
