#pragma once

#include <QString>
#include <QVariantList>

#include <functional>
#include <memory>

#include "app_scene_context.h"
#include "entity_cache.h"

class LoadingProgressTracker;
class VisibilityCoordinator;

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
class VictoryService;
} // namespace Game::Systems

class MinimapManager;
struct LevelLoadResult {
  bool success = false;
  QString error_message;
  int updated_player_id = 1;
};

class LevelOrchestrator {
public:
  using OwnerUpdateCallback = std::function<void()>;

  LevelLoadResult load_skirmish(const QString& map_path,
                                const QVariantList& player_configs,
                                int selected_player_id,
                                Engine::Core::World& world,
                                const AppSceneContext& scene,
                                Game::Systems::LevelSnapshot& level,
                                EntityCache& entity_cache,
                                Game::Systems::VictoryService* victory_service,
                                MinimapManager* minimap_manager,
                                VisibilityCoordinator* visibility_coordinator,
                                OwnerUpdateCallback owner_update,
                                bool allow_default_player_barracks,
                                LoadingProgressTracker* progress_tracker = nullptr);
};
