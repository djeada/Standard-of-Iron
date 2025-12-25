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
} 

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
class OliveRenderer;
class FireCampRenderer;
class RainRenderer;
class RiverRenderer;
class RoadRenderer;
class RiverbankRenderer;
class BridgeRenderer;
} 

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
  void set_biome_renderer(Render::GL::BiomeRenderer *biome) { m_biome = biome; }
  void set_river_renderer(Render::GL::RiverRenderer *river) { m_river = river; }
  void set_road_renderer(Render::GL::RoadRenderer *road) { m_road = road; }
  void set_riverbank_renderer(Render::GL::RiverbankRenderer *riverbank) {
    m_riverbank = riverbank;
  }
  void set_bridge_renderer(Render::GL::BridgeRenderer *bridge) {
    m_bridge = bridge;
  }
  void set_fog_renderer(Render::GL::FogRenderer *fog) { m_fog = fog; }
  void set_stone_renderer(Render::GL::StoneRenderer *stone) { m_stone = stone; }
  void set_plant_renderer(Render::GL::PlantRenderer *plant) { m_plant = plant; }
  void set_pine_renderer(Render::GL::PineRenderer *pine) { m_pine = pine; }
  void set_olive_renderer(Render::GL::OliveRenderer *olive) { m_olive = olive; }
  void set_fire_camp_renderer(Render::GL::FireCampRenderer *firecamp) {
    m_firecamp = firecamp;
  }
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
  Render::GL::BiomeRenderer *m_biome = nullptr;
  Render::GL::RiverRenderer *m_river = nullptr;
  Render::GL::RoadRenderer *m_road = nullptr;
  Render::GL::RiverbankRenderer *m_riverbank = nullptr;
  Render::GL::BridgeRenderer *m_bridge = nullptr;
  Render::GL::FogRenderer *m_fog = nullptr;
  Render::GL::StoneRenderer *m_stone = nullptr;
  Render::GL::PlantRenderer *m_plant = nullptr;
  Render::GL::PineRenderer *m_pine = nullptr;
  Render::GL::OliveRenderer *m_olive = nullptr;
  Render::GL::FireCampRenderer *m_firecamp = nullptr;
  Render::GL::RainRenderer *m_rain = nullptr;
  OwnersUpdatedCallback m_onOwnersUpdated;
  VisibilityMaskReadyCallback m_onVisibilityMaskReady;
};

} 
