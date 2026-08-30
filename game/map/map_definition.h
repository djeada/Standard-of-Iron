#pragma once

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <variant>
#include <vector>

#include "../systems/nation_id.h"
#include "../systems/resource_types.h"
#include "../units/spawn_type.h"
#include "../wildlife/wildlife_config.h"
#include "environment_lighting.h"
#include "terrain.h"

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
  int max_population = 60;
  std::optional<Game::Systems::NationID> nation;
  QString behavior;
  float guard_radius = 10.0F;
  std::vector<QVector3D> patrol_waypoints;
};

struct PointStructureGeometry {
  QVector3D position;
};

struct LineStructureGeometry {
  QVector3D start;
  QVector3D end;
  float width = 2.0F;
};

using StructureGeometry = std::variant<PointStructureGeometry, LineStructureGeometry>;

struct StructureEntry {
  Game::Units::SpawnType type = Game::Units::SpawnType::Barracks;
  StructureGeometry geometry = PointStructureGeometry{};
  int player_id = 0;
  int team_id = 0;
  int max_population = 60;
  float rotation = 0.0F;
  QString nation;
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
    CypressTree,
    PalmTree,
    Plant,
    IronOre,
    MagicShrine,
    AbandonedHome,
    Statue,
    CursedGoldVein
  };

  std::uint64_t id = 0;
  Type type = Type::Tent;
  float x = 0.0F;
  float z = 0.0F;
  float scale = 1.0F;
  float rotation = 0.0F;
  float intensity = 1.0F;
  float radius = 3.0F;
  bool persistent = true;
};

struct UndeadWaveUnitSpawn {
  Game::Units::SpawnType type = Game::Units::SpawnType::SkeletonSwordsman;
  int count = 1;
};

struct UndeadWave {
  QString trigger = "initial";
  std::vector<UndeadWaveUnitSpawn> units;
};

struct Forest {
  QString id;
  float x = 0.0F;
  float z = 0.0F;
  float radius = 8.0F;
};

inline constexpr float k_undead_zone_default_fog_density = 0.28F;
inline constexpr float k_undead_zone_default_wave_timeout = 45.0F;

struct UndeadZone {
  QString id;
  WorldProp::Type anchor_type = WorldProp::Type::Ruins;
  float x = 0.0F;
  float z = 0.0F;
  float radius = 8.0F;
  float leash_radius = 14.0F;
  int owner_id = 99;
  int team_id = 0;

  float fog_density = k_undead_zone_default_fog_density;

  float wave_timeout_seconds = k_undead_zone_default_wave_timeout;
  std::vector<QString> awaken_on;
  std::vector<UndeadWave> waves;

  Game::Systems::ResourceAmounts clear_reward;
};

[[nodiscard]] inline auto default_undead_waves() -> std::vector<UndeadWave> {
  UndeadWave wave;
  wave.trigger = QStringLiteral("initial");
  wave.units = {{Game::Units::SpawnType::SkeletonSwordsman, 2},
                {Game::Units::SpawnType::SkeletonArcher, 1},
                {Game::Units::SpawnType::GravePriest, 1}};
  return {wave};
}

[[nodiscard]] constexpr auto
is_settlement_world_prop_type(WorldProp::Type type) -> bool {
  return type == WorldProp::Type::AbandonedHome || type == WorldProp::Type::Statue;
}

[[nodiscard]] constexpr auto
is_cursed_gold_vein_world_prop_type(WorldProp::Type type) -> bool {
  return type == WorldProp::Type::CursedGoldVein;
}

[[nodiscard]] constexpr auto is_tree_world_prop_type(WorldProp::Type type) -> bool {
  return type == WorldProp::Type::PineTree || type == WorldProp::Type::OliveTree ||
         type == WorldProp::Type::CypressTree || type == WorldProp::Type::PalmTree;
}

[[nodiscard]] constexpr auto is_boulder_world_prop_type(WorldProp::Type type) -> bool {
  return type == WorldProp::Type::Boulder;
}

[[nodiscard]] constexpr auto is_iron_ore_world_prop_type(WorldProp::Type type) -> bool {
  return type == WorldProp::Type::IronOre;
}

[[nodiscard]] constexpr auto
is_harvestable_world_prop_type(WorldProp::Type type) -> bool {
  return is_tree_world_prop_type(type) || is_boulder_world_prop_type(type) ||
         is_iron_ore_world_prop_type(type);
}

[[nodiscard]] constexpr auto is_solid_world_prop_type(WorldProp::Type type) -> bool {
  switch (type) {

  case WorldProp::Type::Plant:
    return false;
  case WorldProp::Type::FireCamp:
  case WorldProp::Type::Tent:
  case WorldProp::Type::SupplyCart:
  case WorldProp::Type::WeaponRack:
  case WorldProp::Type::Ruins:
  case WorldProp::Type::DeadTree:
  case WorldProp::Type::Boulder:
  case WorldProp::Type::PineTree:
  case WorldProp::Type::OliveTree:
  case WorldProp::Type::CypressTree:
  case WorldProp::Type::PalmTree:
  case WorldProp::Type::IronOre:
  case WorldProp::Type::MagicShrine:
  case WorldProp::Type::AbandonedHome:
  case WorldProp::Type::Statue:
  case WorldProp::Type::CursedGoldVein:
    return true;
  }
  return true;
}

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
  case WorldProp::Type::CypressTree:
    return QLatin1String("cypress_tree");
  case WorldProp::Type::PalmTree:
    return QLatin1String("palm_tree");
  case WorldProp::Type::Plant:
    return QLatin1String("plant");
  case WorldProp::Type::IronOre:
    return QLatin1String("iron_ore");
  case WorldProp::Type::MagicShrine:
    return QLatin1String("magic_shrine");
  case WorldProp::Type::AbandonedHome:
    return QLatin1String("abandoned_home");
  case WorldProp::Type::Statue:
    return QLatin1String("statue");
  case WorldProp::Type::CursedGoldVein:
    return QLatin1String("cursed_gold_vein");
  }
  Q_UNREACHABLE();
}

[[nodiscard]] inline auto world_prop_type_from_string(const QString& value,
                                                      WorldProp::Type& out) -> bool {
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
  if (value == QLatin1String("cypress_tree")) {
    out = WorldProp::Type::CypressTree;
    return true;
  }
  if (value == QLatin1String("palm_tree")) {
    out = WorldProp::Type::PalmTree;
    return true;
  }
  if (value == QLatin1String("plant")) {
    out = WorldProp::Type::Plant;
    return true;
  }
  if (value == QLatin1String("iron_ore")) {
    out = WorldProp::Type::IronOre;
    return true;
  }
  if (value == QLatin1String("magic_shrine")) {
    out = WorldProp::Type::MagicShrine;
    return true;
  }
  if (value == QLatin1String("abandoned_home")) {
    out = WorldProp::Type::AbandonedHome;
    return true;
  }
  if (value == QLatin1String("statue")) {
    out = WorldProp::Type::Statue;
    return true;
  }
  if (value == QLatin1String("cursed_gold_vein")) {
    out = WorldProp::Type::CursedGoldVein;
    return true;
  }
  return false;
}

[[nodiscard]] constexpr auto world_prop_render_scale(WorldProp::Type type) -> float {
  switch (type) {
  case WorldProp::Type::FireCamp:
    return 1.0F;
  case WorldProp::Type::Tent:
    return 3.0F;
  case WorldProp::Type::SupplyCart:
    return 1.28F;
  case WorldProp::Type::WeaponRack:
    return 0.68F;
  case WorldProp::Type::Ruins:
    return 2.90F;
  case WorldProp::Type::DeadTree:
    return 1.55F;
  case WorldProp::Type::Boulder:
    return 1.20F;
  case WorldProp::Type::PineTree:
    return 4.50F;
  case WorldProp::Type::OliveTree:
    return 4.00F;
  case WorldProp::Type::CypressTree:
    return 3.60F;
  case WorldProp::Type::PalmTree:
    return 4.20F;
  case WorldProp::Type::Plant:
    return 0.55F;
  case WorldProp::Type::IronOre:
    return 1.10F;
  case WorldProp::Type::MagicShrine:
    return 1.80F;
  case WorldProp::Type::AbandonedHome:
    return 1.90F;
  case WorldProp::Type::Statue:
    return 1.05F;
  case WorldProp::Type::CursedGoldVein:
    return 1.70F;
  }
  return 1.0F;
}

struct WorldPropHalfExtents {
  float x{0.0F};
  float z{0.0F};
};

[[nodiscard]] constexpr auto
world_prop_model_half_extents(WorldProp::Type type) -> WorldPropHalfExtents {
  switch (type) {
  case WorldProp::Type::Ruins:
    return {0.94F, 0.70F};
  case WorldProp::Type::AbandonedHome:
    return {1.16F, 0.82F};
  case WorldProp::Type::SupplyCart:
    return {0.90F, 1.42F};
  case WorldProp::Type::WeaponRack:
    return {0.88F, 0.54F};
  case WorldProp::Type::MagicShrine:
    return {0.86F, 0.86F};
  case WorldProp::Type::CursedGoldVein:
    return {0.92F, 0.92F};
  case WorldProp::Type::Statue:
    return {0.55F, 0.55F};
  case WorldProp::Type::Tent:
    return {0.69F, 0.90F};
  case WorldProp::Type::FireCamp:
    return {0.90F, 0.90F};
  case WorldProp::Type::Boulder:
    return {0.60F, 0.55F};
  case WorldProp::Type::IronOre:
    return {0.82F, 0.56F};
  case WorldProp::Type::DeadTree:
    return {1.34F, 0.35F};
  case WorldProp::Type::PineTree:
  case WorldProp::Type::OliveTree:
  case WorldProp::Type::CypressTree:
  case WorldProp::Type::PalmTree:
    return {1.0F, 1.0F};
  case WorldProp::Type::Plant:
    break;
  }
  return {0.55F, 0.55F};
}

[[nodiscard]] constexpr auto
world_prop_model_half_extent(WorldProp::Type type) -> float {
  const WorldPropHalfExtents extents = world_prop_model_half_extents(type);
  return extents.x > extents.z ? extents.x : extents.z;
}

[[nodiscard]] constexpr auto
world_prop_blocks_only_its_stem(WorldProp::Type type) -> bool {
  switch (type) {
  case WorldProp::Type::PineTree:
  case WorldProp::Type::OliveTree:
  case WorldProp::Type::CypressTree:
  case WorldProp::Type::PalmTree:
    return true;
  default:
    break;
  }
  return false;
}

inline constexpr float k_world_prop_stem_fraction = 0.22F;
inline constexpr float k_world_prop_min_ground_half_extent = 0.5F;

[[nodiscard]] constexpr auto world_prop_ground_fraction(WorldProp::Type type) -> float {
  return world_prop_blocks_only_its_stem(type) ? k_world_prop_stem_fraction
                                               : world_prop_model_half_extent(type);
}

[[nodiscard]] constexpr auto
world_prop_ground_half_extents(WorldProp::Type type,
                               float authored_scale) -> WorldPropHalfExtents {
  float const scale = world_prop_render_scale(type) * authored_scale;
  WorldPropHalfExtents extents = world_prop_model_half_extents(type);
  if (world_prop_blocks_only_its_stem(type)) {
    extents = {k_world_prop_stem_fraction, k_world_prop_stem_fraction};
  }
  extents.x *= scale;
  extents.z *= scale;
  extents.x = extents.x > k_world_prop_min_ground_half_extent
                  ? extents.x
                  : k_world_prop_min_ground_half_extent;
  extents.z = extents.z > k_world_prop_min_ground_half_extent
                  ? extents.z
                  : k_world_prop_min_ground_half_extent;
  return extents;
}

[[nodiscard]] constexpr auto world_prop_ground_radius(WorldProp::Type type,
                                                      float authored_scale) -> float {
  const WorldPropHalfExtents extents =
      world_prop_ground_half_extents(type, authored_scale);
  return extents.x > extents.z ? extents.x : extents.z;
}

[[nodiscard]] inline auto
world_prop_ground_bounding_radius(WorldProp::Type type, float authored_scale) -> float {
  const WorldPropHalfExtents extents =
      world_prop_ground_half_extents(type, authored_scale);
  return std::hypot(extents.x, extents.z);
}

[[nodiscard]] inline auto world_prop_overlap_depth(WorldProp::Type type,
                                                   float authored_scale,
                                                   float prop_x,
                                                   float prop_z,
                                                   float rotation,
                                                   float x,
                                                   float z,
                                                   float radius) -> float {
  const WorldPropHalfExtents extents =
      world_prop_ground_half_extents(type, authored_scale);
  float const cosine = std::cos(rotation);
  float const sine = std::sin(rotation);
  float const dx = x - prop_x;
  float const dz = z - prop_z;

  float const local_x = (cosine * dx) + (sine * dz);
  float const local_z = (-sine * dx) + (cosine * dz);
  float const gap_x = std::abs(local_x) - extents.x;
  float const gap_z = std::abs(local_z) - extents.z;
  if (gap_x <= 0.0F && gap_z <= 0.0F) {
    return radius - (gap_x > gap_z ? gap_x : gap_z);
  }
  float const outside_x = gap_x > 0.0F ? gap_x : 0.0F;
  float const outside_z = gap_z > 0.0F ? gap_z : 0.0F;
  return radius - std::hypot(outside_x, outside_z);
}

enum class CoordSystem {
  Grid,
  World
};

struct UndeadObjective {
  QString type;
  QString zone_id;
  int wave_count = 1;
};

struct VictoryConfig {
  QString victory_type = "elimination";
  std::vector<QString> key_structures = {"barracks"};
  float survive_time_duration = 0.0F;
  std::vector<QString> defeat_conditions = {"no_commander", "only_commander_remaining"};
  int required_key_structures = 0;
  std::vector<UndeadObjective> undead_objectives;
};

enum class WeatherType {
  Rain,
  Snow
};

inline constexpr float k_weather_intensity_light = 0.3F;
inline constexpr float k_weather_intensity_medium = 0.6F;
inline constexpr float k_weather_intensity_heavy = 0.9F;

struct RainSettings {
  bool enabled = false;
  WeatherType type = WeatherType::Rain;
  float cycle_duration = 300.0F;
  float active_duration = 60.0F;
  float intensity = 0.5F;
  float fade_duration = 5.0F;
  float wind_strength = 0.0F;

  float wind_direction_deg = 45.0F;
};

[[nodiscard]] inline auto parse_weather_intensity(const QString& value,
                                                  float fallback) -> float {
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("light")) {
    return k_weather_intensity_light;
  }
  if (normalized == QStringLiteral("medium")) {
    return k_weather_intensity_medium;
  }
  if (normalized == QStringLiteral("heavy")) {
    return k_weather_intensity_heavy;
  }
  bool parsed = false;
  const float numeric = normalized.toFloat(&parsed);
  return parsed ? std::clamp(numeric, 0.0F, 1.0F) : fallback;
}

[[nodiscard]] inline auto weather_intensity_name(float intensity) -> const char* {
  const float midpoint_light_medium =
      (k_weather_intensity_light + k_weather_intensity_medium) * 0.5F;
  const float midpoint_medium_heavy =
      (k_weather_intensity_medium + k_weather_intensity_heavy) * 0.5F;
  if (intensity < midpoint_light_medium) {
    return "light";
  }
  if (intensity < midpoint_medium_heavy) {
    return "medium";
  }
  return "heavy";
}

[[nodiscard]] inline auto
weather_wind_vector(float wind_direction_deg) noexcept -> QVector3D {
  const float radians = wind_direction_deg * std::numbers::pi_v<float> / 180.0F;
  return {std::sin(radians), 0.0F, std::cos(radians)};
}

struct FogZone {
  float x = 0.0F;
  float z = 0.0F;

  float y = 0.0F;
  float width = 10.0F;
  float height = 10.0F;
  float density = 0.6F;
};

[[nodiscard]] inline auto
undead_zone_fog(float world_x, float world_z, float radius, float density) -> FogZone {
  FogZone fog;
  fog.x = world_x;
  fog.z = world_z;
  fog.width = std::max(1.0F, radius * 2.0F);
  fog.height = fog.width;
  fog.density = std::clamp(density, 0.0F, 1.0F);
  return fog;
}

struct MapDefinition {
  QString name;
  GridDefinition grid;
  CameraDefinition camera;
  std::vector<UnitSpawn> spawns;
  std::vector<TerrainFeature> terrain;
  std::vector<RiverSegment> rivers;
  std::vector<Lake> lakes;
  std::vector<RoadSegment> roads;
  std::vector<Bridge> bridges;
  std::vector<StructureEntry> structures;
  std::vector<WorldProp> world_props;
  std::vector<UndeadZone> undead_zones;
  std::vector<Forest> forests;
  std::vector<FogZone> fog_zones;
  BiomeSettings biome;
  CoordSystem coordSystem = CoordSystem::Grid;
  int max_troops_per_player = 250;
  VictoryConfig victory;
  RainSettings rain;
  Game::Wildlife::WildlifeSettings wildlife;
  TimeOfDay time_of_day = TimeOfDay::Day;
  EnvironmentDefinition environment;
  Game::Systems::ResourceAmounts starting_resources{};
};

} // namespace Game::Map
