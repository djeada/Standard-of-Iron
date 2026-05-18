#pragma once

#include <QJsonObject>

#include "app_scene_context.h"
#include "entity_cache.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class TerrainFeatureManager;
class TerrainScatterManager;
class FogRenderer;
class MapBoundaryFogRenderer;
class RainRenderer;
} // namespace Render::GL

namespace Game::Systems {
struct LevelSnapshot;
struct RuntimeSnapshot;
} // namespace Game::Systems

class MinimapManager;
class VisibilityCoordinator;

class GameStateRestorer {
public:
  static void restore_environment_from_metadata(const QJsonObject& metadata,
                                                const AppSceneContext& scene,
                                                Game::Systems::LevelSnapshot& level,
                                                int local_owner_id,
                                                MinimapManager* minimap_manager,
                                                VisibilityCoordinator* visibility_coordinator);

  static void rebuild_registries_after_load(Engine::Core::World* world,
                                            int& selected_player_id,
                                            Game::Systems::LevelSnapshot& level,
                                            int local_owner_id);

  static void rebuild_building_collisions(Engine::Core::World* world);

  static void rebuild_entity_cache(Engine::Core::World* world,
                                   EntityCache& entity_cache,
                                   int local_owner_id);
};
