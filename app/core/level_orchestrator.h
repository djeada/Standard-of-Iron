#pragma once

#include <QString>
#include <QVariantList>
#include <functional>
#include <memory>

class LoadingProgressTracker;

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class RiverRenderer;
class RoadRenderer;
class RiverbankRenderer;
class BridgeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
class OliveRenderer;
class FireCampRenderer;
} // namespace Render::GL

namespace Game::Systems {
struct LevelSnapshot;
class VictoryService;
} // namespace Game::Systems

class MinimapManager;
class EntityCache;

struct LevelLoadResult {
  bool success = false;
  QString error_message;
  int updated_player_id = 1;
};

class LevelOrchestrator {
public:
  struct RendererRefs {
    Render::GL::Renderer *renderer;
    Render::GL::Camera *camera;
    Render::GL::GroundRenderer *ground;
    Render::GL::TerrainRenderer *terrain;
    Render::GL::BiomeRenderer *biome;
    Render::GL::RiverRenderer *river;
    Render::GL::RoadRenderer *road;
    Render::GL::RiverbankRenderer *riverbank;
    Render::GL::BridgeRenderer *bridge;
    Render::GL::FogRenderer *fog;
    Render::GL::StoneRenderer *stone;
    Render::GL::PlantRenderer *plant;
    Render::GL::PineRenderer *pine;
    Render::GL::OliveRenderer *olive;
    Render::GL::FireCampRenderer *firecamp;
  };

  using VisibilityReadyCallback = std::function<void()>;
  using OwnerUpdateCallback = std::function<void()>;

  LevelLoadResult load_skirmish(
      const QString &map_path, const QVariantList &player_configs,
      int selected_player_id, Engine::Core::World &world,
      const RendererRefs &renderers, Game::Systems::LevelSnapshot &level,
      EntityCache &entity_cache, Game::Systems::VictoryService *victory_service,
      MinimapManager *minimap_manager, VisibilityReadyCallback visibility_ready,
      OwnerUpdateCallback owner_update,
      LoadingProgressTracker *progress_tracker = nullptr);
};
