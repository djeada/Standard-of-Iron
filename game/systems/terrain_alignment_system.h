#pragma once

#include "../core/system.h"

namespace Engine::Core {
class TransformComponent;
}

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class TerrainAlignmentSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  static void align_transform_to_terrain(Engine::Core::TransformComponent& transform,
                                         Game::Map::TerrainService& terrain_service);
};

} // namespace Game::Systems
