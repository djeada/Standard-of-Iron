#pragma once

#include <QString>

#include <cstdint>
#include <memory>

#include "game/map/map_definition.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class Camera;
} // namespace Render::GL

namespace App::Core {

struct LevelLoadResult {
  bool ok = false;
  QString map_name;
  QString error_message;
  Engine::Core::EntityID player_unit_id = 0;
  float cam_fov = 45.0F;
  float cam_near = 0.1F;
  float cam_far = 1000.0F;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  int max_troops_per_player = 500;
  Game::Map::VictoryConfig victory_config;
  Game::Map::RainSettings rain_settings;
  std::vector<Game::Map::FogZone> fog_zones;
  std::vector<Game::Map::RiverSegment> rivers;
  std::vector<Game::Map::Lake> lakes;
  std::uint32_t biome_seed = 0;
  Game::Map::EnvironmentLightingState lighting_state;
  Game::Map::EnvironmentDefinition environment;
};

class LevelLoader {
public:
  static auto
  loadFromAssets(const QString& map_path,
                 Engine::Core::World& world,
                 Render::GL::Renderer& renderer,
                 Render::GL::Camera& camera,
                 bool allow_default_player_barracks = true) -> LevelLoadResult;
};

} // namespace App::Core
