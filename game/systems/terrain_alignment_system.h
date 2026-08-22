#pragma once

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class TerrainAlignmentSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

private:
  static void align_transform_to_terrain(Engine::Core::TransformComponent& transform,
                                         Game::Map::TerrainService& terrain_service);
};

} // namespace Game::Systems
