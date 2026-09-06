#pragma once

#include <QString>

#include <cstdint>
#include <vector>

#include "map_definition.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Map {

struct MatchLoadResult {
  bool ok = false;
  QString map_name;
  QString error_message;
  MapDefinition definition;
  Engine::Core::EntityID player_unit_id = 0;
  int grid_width = 50;
  int grid_height = 50;
  float tile_size = 1.0F;
  int max_troops_per_player = 500;
  VictoryConfig victory_config;
  RainSettings rain_settings;
  std::vector<FogZone> fog_zones;
  std::vector<RiverSegment> rivers;
  std::vector<Lake> lakes;
  std::uint32_t biome_seed = 0;
  EnvironmentLightingState lighting_state;
  EnvironmentDefinition environment;
};

[[nodiscard]] auto
load_match(const QString& map_path,
           Engine::Core::World& world,
           bool allow_default_player_barracks = true) -> MatchLoadResult;

} // namespace Game::Map
