#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../map/map_definition.h"
#include "rain_manager.h"
#include "resource_types.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

struct LevelSnapshot {
  QString map_path;
  QString map_name;
  Engine::Core::EntityID player_unit_id = 0;
  float cam_fov = 45.0F;
  float cam_near = 0.1F;
  float cam_far = 1000.0F;
  int max_troops_per_player = 500;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  bool is_spectator_mode = false;
  Game::Map::RainSettings rain;
  RainRuntimeState weather_runtime;
  std::uint32_t biome_seed = 0;
  Game::Map::EnvironmentLightingState lighting;
  Game::Map::EnvironmentDefinition environment;
  Game::Map::EnvironmentClockState environment_clock;
  ResourceAmounts starting_resources{};
};

struct RuntimeSnapshot {
  bool paused = false;
  float time_scale = 1.0F;
  int local_owner_id = 1;
  QString victory_state = "";
  int cursor_mode = 0;
  int selected_player_id = 1;
  bool follow_selection = false;
  std::vector<OwnerResourceState> resources_by_owner;
  std::vector<OwnerResourceState> harvested_by_owner;

  std::uint64_t simulation_tick = 0;
  std::uint64_t rng_seed = 0;
  std::uint64_t rng_draw_count = 0;
};

class GameStateSerializer {
public:
  static auto build_metadata(const Engine::Core::World& world,
                             const Render::GL::Camera* camera,
                             const LevelSnapshot& level,
                             const RuntimeSnapshot& runtime) -> QJsonObject;

  static void restore_camera_from_metadata(const QJsonObject& metadata,
                                           Render::GL::Camera* camera,
                                           int viewport_width,
                                           int viewport_height);

  static void restore_runtime_from_metadata(const QJsonObject& metadata,
                                            RuntimeSnapshot& runtime);

  static void restore_level_from_metadata(const QJsonObject& metadata,
                                          LevelSnapshot& level);

  static void restore_player_nations_from_metadata(const QJsonObject& metadata);

  static void restore_visibility_from_metadata(const QJsonObject& metadata);
};

} // namespace Game::Systems
