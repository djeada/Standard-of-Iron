#pragma once

#include <QJsonObject>

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
class RainRenderer;
} // namespace Render::GL

namespace Game::Systems {
struct LevelSnapshot;
struct RuntimeSnapshot;
} // namespace Game::Systems

class EntityCache;

struct ViewportState;

class GameStateRestorer {
public:
  struct RendererRefs {
    Render::GL::Renderer *renderer;
    Render::GL::Camera *camera;
    Render::GL::GroundRenderer *ground;
    Render::GL::TerrainRenderer *terrain;
    Render::GL::TerrainFeatureManager *features;
    Render::GL::TerrainScatterManager *scatter;
    Render::GL::FogRenderer *fog;
    Render::GL::RainRenderer *rain;
  };

  static void restore_environment_from_metadata(
      const QJsonObject &metadata, Engine::Core::World *world,
      const RendererRefs &renderers, Game::Systems::LevelSnapshot &level,
      int local_owner_id, const ViewportState &viewport);

  static void rebuild_registries_after_load(Engine::Core::World *world,
                                            int &selected_player_id,
                                            Game::Systems::LevelSnapshot &level,
                                            int local_owner_id);

  static void rebuild_building_collisions(Engine::Core::World *world);

  static void rebuild_entity_cache(Engine::Core::World *world,
                                   EntityCache &entity_cache,
                                   int local_owner_id);
};
