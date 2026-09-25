#pragma once

#include <QString>
#include <QVariantList>

#include <functional>
#include <memory>

#include "app/core/app_scene_context.h"
#include "app/core/entity_cache.h"

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

  QVariantList resolved_player_configs;
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
                                bool defer_ai_initialization,
                                LoadingProgressTracker* progress_tracker = nullptr);
};

// Bakes every creature body the match can draw, then forbids render-time
// bakes. Call it once the world, owners and nations of a new or loaded match
// are in place and before its first frame: a body that was not baked here is
// not drawn for the rest of the match.
void prewarm_match_render_templates(Engine::Core::World& world,
                                    const AppSceneContext& scene,
                                    LoadingProgressTracker* progress_tracker = nullptr);
