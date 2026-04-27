#pragma once

#include "map_definition.h"
#include <QString>
#include <QVariantList>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class TerrainFeatureManager;
class FogRenderer;
class RainRenderer;
class TerrainScatterManager;
} // namespace Render::GL

namespace Game::Map {

struct SkirmishLoadResult {
  bool ok = false;
  QString map_name;
  QString errorMessage;
  Engine::Core::EntityID player_unit_id = 0;
  float cam_fov = 45.0F;
  float cam_near = 0.1F;
  float cam_far = 1000.0F;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  int max_troops_per_player = 500;
  VictoryConfig victoryConfig;
  QVector3D focusPosition;
  bool has_focus_position = false;
  bool is_spectator_mode = false;
  RainSettings rainSettings;
  std::uint32_t biome_seed = 0;
};

class SkirmishLoader {
public:
  using OwnersUpdatedCallback = std::function<void()>;
  using VisibilityMaskReadyCallback = std::function<void()>;

  SkirmishLoader(Engine::Core::World &world, Render::GL::Renderer &renderer,
                 Render::GL::Camera &camera);

  void set_ground_renderer(Render::GL::GroundRenderer *ground) {
    m_ground = ground;
  }
  void set_terrain_renderer(Render::GL::TerrainRenderer *terrain) {
    m_terrain = terrain;
  }
  void set_scatter_manager(Render::GL::TerrainScatterManager *scatter) {
    m_scatter = scatter;
  }
  void set_feature_manager(Render::GL::TerrainFeatureManager *features) {
    m_features = features;
  }
  void set_fog_renderer(Render::GL::FogRenderer *fog) { m_fog = fog; }
  void set_rain_renderer(Render::GL::RainRenderer *rain) { m_rain = rain; }

  void set_on_owners_updated(OwnersUpdatedCallback callback) {
    m_onOwnersUpdated = std::move(callback);
  }

  void set_on_visibility_mask_ready(VisibilityMaskReadyCallback callback) {
    m_onVisibilityMaskReady = std::move(callback);
  }

  auto start(const QString &map_path, const QVariantList &playerConfigs,
             int selected_player_id, bool allow_default_player_barracks,
             int &out_selected_player_id) -> SkirmishLoadResult;

private:
  void reset_game_state();
  Engine::Core::World &m_world;
  Render::GL::Renderer &m_renderer;
  Render::GL::Camera &m_camera;
  Render::GL::GroundRenderer *m_ground = nullptr;
  Render::GL::TerrainRenderer *m_terrain = nullptr;
  Render::GL::TerrainFeatureManager *m_features = nullptr;
  Render::GL::TerrainScatterManager *m_scatter = nullptr;
  Render::GL::FogRenderer *m_fog = nullptr;
  Render::GL::RainRenderer *m_rain = nullptr;
  OwnersUpdatedCallback m_onOwnersUpdated;
  VisibilityMaskReadyCallback m_onVisibilityMaskReady;
};

} // namespace Game::Map
