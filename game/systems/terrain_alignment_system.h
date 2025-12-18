#pragma once

#include "../core/system.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class TerrainAlignmentSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static void align_entity_to_terrain(Engine::Core::Entity *entity);
};

} 
