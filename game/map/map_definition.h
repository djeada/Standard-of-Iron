#pragma once

#include "../systems/nation_id.h"
#include "../units/spawn_type.h"
#include "terrain.h"
#include <QString>
#include <QVector3D>
#include <cstdint>
#include <optional>
#include <vector>

namespace Game::Map {

struct GridDefinition {
  int width = 50;
  int height = 50;
  float tile_size = 1.0F;
};

struct CameraDefinition {
  QVector3D center{0.0F, 0.0F, 0.0F};
  float distance = 15.0F;
  float tilt_deg = 45.0F;
  float fov_y = 45.0F;

  float near_plane = 1.0F;
  float far_plane = 200.0F;

  float yaw_deg = 225.0F;
};

struct UnitSpawn {
  Game::Units::SpawnType type = Game::Units::SpawnType::Archer;
  float x = 0.0F;
  float z = 0.0F;
  int player_id = 0;
  int team_id = 0;
  int max_population = 100;
  std::optional<Game::Systems::NationID> nation;
};

struct FireCamp {
  float x = 0.0F;
  float z = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  bool persistent = true;
};

struct WorldProp {
  enum class Type : std::uint8_t {
    Tent = 0,
    SupplyCart,
    WeaponRack,
    Ruins,
    DeadTree
  };

  Type type = Type::Tent;
  float x = 0.0F;
  float z = 0.0F;
  float scale = 1.0F;
  float rotation = 0.0F;
};

enum class CoordSystem { Grid, World };

struct VictoryConfig {
  QString victory_type = "elimination";
  std::vector<QString> key_structures = {"barracks"};
  float survive_time_duration = 0.0F;
  std::vector<QString> defeat_conditions = {"no_key_structures"};
  int required_key_structures = 0;
};

enum class WeatherType { Rain, Snow };

struct RainSettings {
  bool enabled = false;
  WeatherType type = WeatherType::Rain;
  float cycle_duration = 300.0F;
  float active_duration = 60.0F;
  float intensity = 0.5F;
  float fade_duration = 5.0F;
  float wind_strength = 0.0F;
};

struct MapDefinition {
  QString name;
  GridDefinition grid;
  CameraDefinition camera;
  std::vector<UnitSpawn> spawns;
  std::vector<TerrainFeature> terrain;
  std::vector<RiverSegment> rivers;
  std::vector<RoadSegment> roads;
  std::vector<Bridge> bridges;
  std::vector<FireCamp> firecamps;
  std::vector<WorldProp> world_props;
  BiomeSettings biome;
  CoordSystem coordSystem = CoordSystem::Grid;
  int max_troops_per_player = 500;
  VictoryConfig victory;
  RainSettings rain;
};

} // namespace Game::Map
