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

enum class TimeOfDay { Morning, Day, Afternoon, Night };

struct LightingSettings {
  QVector3D light_direction{0.35F, 0.85F, 0.42F};
  float ambient_strength = 0.30F;
};

inline auto lighting_for_time_of_day(TimeOfDay tod) -> LightingSettings {
  switch (tod) {
  case TimeOfDay::Morning:
    return {QVector3D(0.60F, 0.30F, 0.20F).normalized(), 0.22F};
  case TimeOfDay::Day:
    return {QVector3D(0.35F, 0.85F, 0.42F).normalized(), 0.30F};
  case TimeOfDay::Afternoon:
    return {QVector3D(0.55F, 0.55F, 0.35F).normalized(), 0.27F};
  case TimeOfDay::Night:
    return {QVector3D(-0.20F, 0.35F, 0.50F).normalized(), 0.12F};
  }
  return {};
}

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

struct WorldProp {
  enum class Type : std::uint8_t {
    FireCamp = 0,
    Tent,
    SupplyCart,
    WeaponRack,
    Ruins,
    DeadTree,
    Boulder,
    PineTree,
    OliveTree,
    Plant
  };

  Type type = Type::Tent;
  float x = 0.0F;
  float z = 0.0F;
  float scale = 1.0F;
  float rotation = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  bool persistent = true;
};

[[nodiscard]] inline auto
world_prop_type_to_string(WorldProp::Type type) -> QLatin1String {
  switch (type) {
  case WorldProp::Type::FireCamp:
    return QLatin1String("firecamp");
  case WorldProp::Type::Tent:
    return QLatin1String("tent");
  case WorldProp::Type::SupplyCart:
    return QLatin1String("supply_cart");
  case WorldProp::Type::WeaponRack:
    return QLatin1String("weapon_rack");
  case WorldProp::Type::Ruins:
    return QLatin1String("ruins");
  case WorldProp::Type::DeadTree:
    return QLatin1String("dead_tree");
  case WorldProp::Type::Boulder:
    return QLatin1String("boulder");
  case WorldProp::Type::PineTree:
    return QLatin1String("pine_tree");
  case WorldProp::Type::OliveTree:
    return QLatin1String("olive_tree");
  case WorldProp::Type::Plant:
    return QLatin1String("plant");
  }
  Q_UNREACHABLE();
}

[[nodiscard]] inline auto
world_prop_type_from_string(const QString &value,
                            WorldProp::Type &out) -> bool {
  if (value == QLatin1String("firecamp")) {
    out = WorldProp::Type::FireCamp;
    return true;
  }
  if (value == QLatin1String("tent")) {
    out = WorldProp::Type::Tent;
    return true;
  }
  if (value == QLatin1String("supply_cart")) {
    out = WorldProp::Type::SupplyCart;
    return true;
  }
  if (value == QLatin1String("weapon_rack")) {
    out = WorldProp::Type::WeaponRack;
    return true;
  }
  if (value == QLatin1String("ruins")) {
    out = WorldProp::Type::Ruins;
    return true;
  }
  if (value == QLatin1String("dead_tree")) {
    out = WorldProp::Type::DeadTree;
    return true;
  }
  if (value == QLatin1String("boulder")) {
    out = WorldProp::Type::Boulder;
    return true;
  }
  if (value == QLatin1String("pine_tree")) {
    out = WorldProp::Type::PineTree;
    return true;
  }
  if (value == QLatin1String("olive_tree")) {
    out = WorldProp::Type::OliveTree;
    return true;
  }
  if (value == QLatin1String("plant")) {
    out = WorldProp::Type::Plant;
    return true;
  }
  return false;
}

[[nodiscard]] constexpr auto
world_prop_render_scale(WorldProp::Type type) -> float {
  switch (type) {
  case WorldProp::Type::FireCamp:
    return 1.0F;
  case WorldProp::Type::Tent:
    return 3.0F;
  case WorldProp::Type::SupplyCart:
    return 1.28F;
  case WorldProp::Type::WeaponRack:
    return 1.35F;
  case WorldProp::Type::Ruins:
    return 1.45F;
  case WorldProp::Type::DeadTree:
    return 1.55F;
  case WorldProp::Type::Boulder:
    return 1.20F;
  case WorldProp::Type::PineTree:
    return 4.50F;
  case WorldProp::Type::OliveTree:
    return 4.00F;
  case WorldProp::Type::Plant:
    return 0.55F;
  }
  return 1.0F;
}

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
  std::vector<WorldProp> world_props;
  BiomeSettings biome;
  CoordSystem coordSystem = CoordSystem::Grid;
  int max_troops_per_player = 500;
  VictoryConfig victory;
  RainSettings rain;
  TimeOfDay time_of_day = TimeOfDay::Day;
};

} // namespace Game::Map
