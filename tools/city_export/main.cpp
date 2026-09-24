

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

#include "aurelia_magna_plan.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_landform.h"
#include "game/systems/building_collision_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Arena::Scenarios::CityPlan;
using Game::Units::SpawnType;

constexpr int k_grid_extent = 769;

constexpr const char* k_nation = "roman_republic";
constexpr int k_city_owner = 1;

[[nodiscard]] auto rounded(float value) -> double {
  return std::round(static_cast<double>(value) * 100.0) / 100.0;
}

[[nodiscard]] auto point(float x, float z) -> QJsonArray {
  return QJsonArray{rounded(x), rounded(z)};
}

[[nodiscard]] auto member_position(const Arena::ArenaScenarioGroup& group,
                                   int index) -> QVector3D {
  const float centre = (static_cast<float>(group.count) - 1.0F) * 0.5F;
  return group.origin + (group.spacing * (static_cast<float>(index) - centre));
}

[[nodiscard]] auto member_id(const QString& name, int index, int count) -> QString {
  return count > 1 ? QStringLiteral("%1_%2").arg(name).arg(index) : name;
}

[[nodiscard]] auto prop_type_name(const QString& arena_name) -> QString {
  const QString lowered = arena_name.trimmed().toLower();
  if (lowered == QStringLiteral("fire_camp")) {
    return QStringLiteral("firecamp");
  }
  return lowered;
}

void write_structures(const CityPlan& plan, QJsonObject& root) {
  QJsonArray out;
  for (const auto& group : plan.definition.groups) {
    if (!group.spawn_type.has_value() ||
        !Game::Units::is_building_spawn(*group.spawn_type)) {
      continue;
    }
    const QString type = Game::Units::spawn_typeToQString(*group.spawn_type);

    if (*group.spawn_type == SpawnType::WallSegment) {

      const QVector3D start = member_position(group, 0);
      const QVector3D end = member_position(group, group.count - 1);
      out.push_back(QJsonObject{{"id", group.name},
                                {"group", group.name},
                                {"type", type},
                                {"start", point(start.x(), start.z())},
                                {"end", point(end.x(), end.z())},
                                {"width", 2.0},
                                {"player_id", k_city_owner},
                                {"nation", k_nation}});
      continue;
    }

    for (int index = 0; index < group.count; ++index) {
      const QVector3D at = member_position(group, index);
      QJsonObject entry{{"id", member_id(group.name, index, group.count)},
                        {"group", group.name},
                        {"type", type},
                        {"x", rounded(at.x())},
                        {"z", rounded(at.z())},
                        {"rotation", rounded(group.facing_degrees)},
                        {"player_id", k_city_owner},
                        {"nation", k_nation}};
      if (*group.spawn_type == SpawnType::Barracks) {
        entry.insert("max_population", 120);
      }
      out.push_back(entry);
    }
  }
  root.insert("structures", out);
}

void write_spawns(const CityPlan& plan, QJsonObject& root) {
  std::map<QString, const Arena::Scenarios::CityPatrolRoute*> routes;
  for (const auto& patrol : plan.patrols) {
    routes.emplace(patrol.name, &patrol);
  }

  QJsonArray out;
  for (const auto& group : plan.definition.groups) {
    if (group.spawn_type.has_value() &&
        Game::Units::is_building_spawn(*group.spawn_type)) {
      continue;
    }
    const SpawnType spawn_type = group.spawn_type.value_or(
        Game::Units::spawn_typeFromTroopType(group.troop_type));
    const QString type = Game::Units::spawn_typeToQString(spawn_type);

    const auto route = routes.find(group.name);
    for (int index = 0; index < group.count; ++index) {
      const QVector3D at = member_position(group, index);
      QJsonObject entry{{"id", member_id(group.name, index, group.count)},
                        {"group", group.name},
                        {"type", type},
                        {"x", rounded(at.x())},
                        {"z", rounded(at.z())},
                        {"player_id", k_city_owner},
                        {"nation", k_nation}};

      if (route != routes.end() && !route->second->route.empty()) {
        entry.insert("behavior", QStringLiteral("patrol"));
        QJsonArray waypoints;
        for (const auto& post : route->second->route) {
          waypoints.push_back(
              QJsonObject{{"x", rounded(post.x())}, {"z", rounded(post.z())}});
        }
        entry.insert("patrol_waypoints", waypoints);
      } else if (group.settlement_resident) {

        entry.insert("behavior", QStringLiteral("guard"));
        entry.insert("guard_radius", rounded(group.settlement_roam_radius));
      }
      out.push_back(entry);
    }
  }

  out.push_back(QJsonObject{{"id", "p1_commander"},
                            {"type", "roman_veteran_consul"},
                            {"x", 0.0},
                            {"z", -46.0},
                            {"player_id", 1},
                            {"team_id", 1},
                            {"nation", k_nation}});
  out.push_back(QJsonObject{{"id", "siege_commander"},
                            {"type", "carthage_sword_commander"},
                            {"x", 0.0},
                            {"z", 268.0},
                            {"player_id", 2},
                            {"team_id", 2},
                            {"nation", "carthage"}});

  root.insert("spawns", out);
}

void write_world_props(const CityPlan& plan, QJsonObject& root) {
  QJsonArray out;
  for (const auto& patch : plan.definition.resource_patches) {
    const QString type = prop_type_name(patch.prop_type);
    for (int index = 0; index < patch.count; ++index) {
      const QVector3D at = patch.origin + (patch.spacing * static_cast<float>(index));
      out.push_back(QJsonObject{{"type", type},
                                {"x", rounded(at.x())},
                                {"z", rounded(at.z())},
                                {"scale", rounded(patch.scale)}});
    }
  }
  root.insert("world_props", out);
}

struct Step {
  float x;
  float z;
  float extent;
  float height;
  float taper;
};

[[nodiscard]] auto terrain_steps(const CityPlan& plan) -> std::vector<Step> {
  std::vector<Step> steps;
  steps.reserve(plan.definition.elevation_patches.size());
  for (const auto& patch : plan.definition.elevation_patches) {
    const float radius = std::max(patch.radius, 1.0F);
    const float plateau = std::clamp(patch.plateau, 0.0F, radius * 0.95F);
    steps.push_back({patch.center.x(),
                     patch.center.z(),
                     radius,
                     patch.height,
                     (radius - plateau) / radius});
  }

  std::stable_sort(steps.begin(), steps.end(), [](const Step& a, const Step& b) {
    return a.height < b.height;
  });
  return steps;
}

void write_terrain(const CityPlan& plan,
                   const std::vector<Step>& steps,
                   QJsonObject& root) {
  QJsonArray out;
  for (const auto& feature : plan.definition.terrain_features) {
    if (feature.type != Game::Map::TerrainType::Mountain) {
      continue;
    }
    out.push_back(QJsonObject{{"type", "mountain"},
                              {"x", rounded(feature.center_x)},
                              {"z", rounded(feature.center_z)},
                              {"width", rounded(feature.width)},
                              {"depth", rounded(feature.depth)},
                              {"height", rounded(feature.height)},
                              {"rotation", rounded(feature.rotation_deg)}});
  }
  for (const auto& step : steps) {
    out.push_back(QJsonObject{{"type", "flat"},
                              {"x", rounded(step.x)},
                              {"z", rounded(step.z)},
                              {"width", rounded(step.extent * 2.0F)},
                              {"depth", rounded(step.extent * 2.0F)},
                              {"height", rounded(step.height)},
                              {"taper", rounded(step.taper)},

                              {"raise", true}});
  }
  root.insert("terrain", out);
}

void write_water(const CityPlan& plan, QJsonObject& root) {
  QJsonArray rivers;
  for (const auto& river : plan.definition.rivers) {
    rivers.push_back(QJsonObject{{"start", point(river.start.x(), river.start.z())},
                                 {"end", point(river.end.x(), river.end.z())},
                                 {"width", rounded(river.width)}});
  }
  root.insert("rivers", rivers);

  QJsonArray lakes;
  for (const auto& lake : plan.definition.lakes) {
    lakes.push_back(QJsonObject{{"x", rounded(lake.center.x())},
                                {"z", rounded(lake.center.z())},
                                {"width", rounded(lake.width)},
                                {"depth", rounded(lake.depth)},
                                {"rotation", rounded(lake.rotation_deg)}});
  }
  root.insert("lakes", lakes);

  QJsonArray bridges;
  for (const auto& bridge : plan.definition.bridges) {
    bridges.push_back(QJsonObject{{"start", point(bridge.start.x(), bridge.start.z())},
                                  {"end", point(bridge.end.x(), bridge.end.z())},
                                  {"width", rounded(bridge.width)},
                                  {"height", rounded(bridge.height)}});
  }
  root.insert("bridges", bridges);

  QJsonArray roads;
  for (const auto& road : plan.definition.roads) {
    roads.push_back(QJsonObject{{"start", point(road.start.x(), road.start.z())},
                                {"end", point(road.end.x(), road.end.z())},
                                {"width", rounded(road.width)},
                                {"style", road.style}});
  }
  root.insert("roads", roads);
}

void write_wildlife(const CityPlan& plan, QJsonObject& root) {
  const auto& wildlife = plan.definition.wildlife;
  const auto species = [](const Game::Wildlife::SpeciesConfig& config) {
    QJsonObject out{{"enabled", config.enabled},
                    {"groups", config.group_count},
                    {"group_size_min", config.group_size_min},
                    {"group_size_max", config.group_size_max},
                    {"roam_radius", rounded(config.roam_radius)}};
    QJsonArray areas;
    for (const auto& area : config.spawn_areas) {
      areas.push_back(QJsonObject{{"x", rounded(area.x)},
                                  {"z", rounded(area.z)},
                                  {"radius", rounded(area.radius)}});
    }
    if (!areas.isEmpty()) {
      out.insert("spawn_areas", areas);
    }
    return out;
  };

  QJsonObject birds = species(wildlife.birds);
  birds.insert("flight_height", rounded(wildlife.birds.flight_height));

  root.insert(
      "wildlife",
      QJsonObject{{"enabled", wildlife.enabled},
                  {"seed", int(wildlife.seed)},
                  {"near_simulation_radius", rounded(wildlife.near_simulation_radius)},
                  {"far_simulation_radius", rounded(wildlife.far_simulation_radius)},
                  {"sheep", species(wildlife.sheep)},
                  {"wolves", species(wildlife.wolves)},
                  {"birds", birds}});
}

constexpr float k_height_noise = 0.04F;
constexpr float k_height_noise_frequency = 0.05F;
constexpr float k_irregularity_scale = 0.13F;
constexpr float k_irregularity_amplitude = 0.05F;
constexpr float k_plant_density = 0.35F;
constexpr float k_patch_density = 3.4F;
constexpr float k_patch_jitter = 0.9F;
constexpr float k_moisture = 0.55F;

[[nodiscard]] auto field_of_written_map(const QString& path,
                                        Game::Map::MapDefinition& map,
                                        QString* error) -> Game::Map::TerrainHeightMap {
  if (!Game::Map::MapLoader::load_from_json_file(path, map, error)) {
    return Game::Map::TerrainHeightMap(1, 1, 1.0F);
  }

  Game::Map::TerrainHeightMap field(
      map.grid.width, map.grid.height, map.grid.tile_size);
  field.apply_biome_variation(map.biome);
  field.build_from_features(map.terrain);
  field.add_lakes(map.lakes);
  field.add_river_segments(map.rivers);
  field.add_bridges(map.bridges);
  return field;
}

[[nodiscard]] auto reachable_from_camp(const Game::Map::TerrainHeightMap& field,
                                       int camp_x,
                                       int camp_z) -> std::vector<std::uint8_t> {
  const int width = field.get_width();
  const int height = field.get_height();
  std::vector<std::uint8_t> seen(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
  if (!field.is_walkable(camp_x, camp_z)) {
    return seen;
  }
  std::deque<std::pair<int, int>> queue{{camp_x, camp_z}};
  seen[static_cast<std::size_t>(camp_z) * width + camp_x] = 1U;
  while (!queue.empty()) {
    const auto [x, z] = queue.front();
    queue.pop_front();
    for (const auto& step :
         {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
      const int nx = x + step.first;
      const int nz = z + step.second;
      if (nx < 0 || nz < 0 || nx >= width || nz >= height) {
        continue;
      }
      const auto index = static_cast<std::size_t>(nz) * width + nx;
      if (seen[index] != 0U || !field.is_walkable(nx, nz)) {
        continue;
      }
      seen[index] = 1U;
      queue.emplace_back(nx, nz);
    }
  }
  return seen;
}

[[nodiscard]] auto first_player_barracks(const QJsonObject& root) -> QVector3D {
  for (const auto value : root.value("structures").toArray()) {
    const QJsonObject entry = value.toObject();
    if (entry.value("type").toString() == QLatin1String("barracks") &&
        entry.value("player_id").toInt() == k_city_owner) {
      return {static_cast<float>(entry.value("x").toDouble()),
              0.0F,
              static_cast<float>(entry.value("z").toDouble())};
    }
  }
  return {};
}

constexpr float k_relief_tolerance = 0.35F;

constexpr float k_spawn_body_half = 0.5F;

constexpr float k_road_margin = 1.2F;

struct Footprint {
  float half_x = 0.0F;
  float half_z = 0.0F;
};

[[nodiscard]] auto ground_is_settled(const Game::Map::TerrainHeightMap& field,
                                     const std::vector<std::uint8_t>& reachable,
                                     double world_x,
                                     double world_z,
                                     const Footprint& body) -> bool {
  const int offset = (k_grid_extent - 1) / 2;
  const auto grid_x = static_cast<float>(world_x) + static_cast<float>(offset);
  const auto grid_z = static_cast<float>(world_z) + static_cast<float>(offset);

  constexpr float k_sample_step = 0.5F;
  const int steps_x =
      std::max(1, static_cast<int>(std::ceil(body.half_x * 2.0F / k_sample_step)));
  const int steps_z =
      std::max(1, static_cast<int>(std::ceil(body.half_z * 2.0F / k_sample_step)));

  float lowest = std::numeric_limits<float>::max();
  float highest = std::numeric_limits<float>::lowest();
  for (int index_x = 0; index_x <= steps_x; ++index_x) {
    const float step_x =
        -body.half_x + (body.half_x * 2.0F * static_cast<float>(index_x) /
                        static_cast<float>(steps_x));
    for (int index_z = 0; index_z <= steps_z; ++index_z) {
      const float step_z =
          -body.half_z + (body.half_z * 2.0F * static_cast<float>(index_z) /
                          static_cast<float>(steps_z));
      const float sample_x = grid_x + step_x;
      const float sample_z = grid_z + step_z;
      const int cell_x = static_cast<int>(std::lround(sample_x));
      const int cell_z = static_cast<int>(std::lround(sample_z));
      const float world_sample_x = static_cast<float>(world_x) + step_x;
      const float world_sample_z = static_cast<float>(world_z) + step_z;
      if (cell_x < 0 || cell_z < 0 || cell_x >= field.get_width() ||
          cell_z >= field.get_height()) {
        return false;
      }
      if (!field.is_walkable(cell_x, cell_z)) {
        return false;
      }
      if (reachable[static_cast<std::size_t>(cell_z) * field.get_width() + cell_x] ==
          0U) {
        return false;
      }
      const float height = field.get_height_at(world_sample_x, world_sample_z);
      lowest = std::min(lowest, height);
      highest = std::max(highest, height);
    }
  }
  return (highest - lowest) <= k_relief_tolerance;
}

[[nodiscard]] auto corridor_clearance(const QVector3D& point,
                                      const QVector3D& start,
                                      const QVector3D& end) -> float {
  const QVector3D run = end - start;
  const float run_length_sq = run.lengthSquared();
  float along = 0.0F;
  if (run_length_sq > 1.0e-6F) {
    along = std::clamp(
        QVector3D::dotProduct(point - start, run) / run_length_sq, 0.0F, 1.0F);
  }
  return (point - (start + (run * along))).length();
}

[[nodiscard]] auto clear_of_roads(const Game::Map::MapDefinition& map,
                                  double world_x,
                                  double world_z,
                                  const Footprint& body) -> bool {
  const float reach = std::max(body.half_x, body.half_z) + k_road_margin;
  const QVector3D point(static_cast<float>(world_x), 0.0F, static_cast<float>(world_z));
  for (const auto& road : map.roads) {
    const QVector3D start(road.start.x(), 0.0F, road.start.z());
    const QVector3D end(road.end.x(), 0.0F, road.end.z());
    if (corridor_clearance(point, start, end) < (road.width * 0.5F) + reach) {
      return false;
    }
  }
  for (const auto& bridge : map.bridges) {
    const QVector3D start(bridge.start.x(), 0.0F, bridge.start.z());
    const QVector3D end(bridge.end.x(), 0.0F, bridge.end.z());
    if (corridor_clearance(point, start, end) < (bridge.width * 0.5F) + reach) {
      return false;
    }
  }
  return true;
}

struct SolidRect {
  float x = 0.0F;
  float z = 0.0F;
  float half_x = 0.0F;
  float half_z = 0.0F;
};

[[nodiscard]] auto structure_bodies(const QJsonObject& root) -> std::vector<SolidRect> {
  std::vector<SolidRect> bodies;
  for (const auto value : root.value("structures").toArray()) {
    const QJsonObject entry = value.toObject();
    if (!entry.contains("x")) {
      continue;
    }
    const auto body = Game::Systems::BuildingCollisionRegistry::get_building_body(
        entry.value("type").toString().toStdString());
    const auto size = Game::Systems::BuildingCollisionRegistry::axis_aligned_size(
        {body.width, body.depth},
        static_cast<float>(entry.value("rotation").toDouble(0.0)));
    bodies.push_back({static_cast<float>(entry.value("x").toDouble()),
                      static_cast<float>(entry.value("z").toDouble()),
                      size.width * 0.5F,
                      size.depth * 0.5F});
  }
  return bodies;
}

[[nodiscard]] auto clear_of_structures(const std::vector<SolidRect>& bodies,
                                       double world_x,
                                       double world_z,
                                       const Footprint& body) -> bool {
  constexpr float k_clearance = 0.15F;
  const auto x = static_cast<float>(world_x);
  const auto z = static_cast<float>(world_z);
  for (const auto& rect : bodies) {
    if (std::abs(rect.x - x) < rect.half_x + body.half_x + k_clearance &&
        std::abs(rect.z - z) < rect.half_z + body.half_z + k_clearance) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] auto settle_placement(const Game::Map::TerrainHeightMap& field,
                                    const std::vector<std::uint8_t>& reachable,
                                    const Game::Map::MapDefinition& map,
                                    const Footprint& body,
                                    bool avoids_roads,
                                    const std::vector<SolidRect>* solids,
                                    bool needs_settled_ground,
                                    float travel,
                                    QJsonObject& entry) -> bool {
  const double origin_x = entry.value("x").toDouble();
  const double origin_z = entry.value("z").toDouble();
  const auto legal = [&](double x, double z) {
    return (!needs_settled_ground || ground_is_settled(field, reachable, x, z, body)) &&
           (!avoids_roads || clear_of_roads(map, x, z, body)) &&
           (solids == nullptr || clear_of_structures(*solids, x, z, body));
  };
  if (legal(origin_x, origin_z)) {
    return true;
  }

  constexpr float k_step = 0.5F;
  const int rings = static_cast<int>(std::lround(travel / k_step));
  for (int ring = 1; ring <= rings; ++ring) {
    const float reach = static_cast<float>(ring) * k_step;
    for (int slice = 0; slice < 16; ++slice) {
      const float bearing =
          (static_cast<float>(slice) / 16.0F) * 2.0F * std::numbers::pi_v<float>;
      const double x = origin_x + (static_cast<double>(reach) * std::cos(bearing));
      const double z = origin_z + (static_cast<double>(reach) * std::sin(bearing));
      if (legal(x, z)) {
        entry.insert("x", rounded(x));
        entry.insert("z", rounded(z));
        return true;
      }
    }
  }
  return false;
}

struct SettleTally {
  int moved = 0;
  int dropped = 0;
};

[[nodiscard]] auto settle_gameplay_bodies(const Game::Map::TerrainHeightMap& field,
                                          const std::vector<std::uint8_t>& reachable,
                                          const Game::Map::MapDefinition& map,
                                          QJsonObject& root) -> SettleTally {
  SettleTally tally;

  QJsonArray structures;
  for (const auto value : root.value("structures").toArray()) {
    QJsonObject entry = value.toObject();
    const auto type = entry.value("type").toString();
    if (!entry.contains("x") || type.startsWith(QLatin1String("wall_"))) {
      structures.push_back(entry);
      continue;
    }
    const auto body =
        Game::Systems::BuildingCollisionRegistry::get_building_body(type.toStdString());
    const Footprint footprint{body.width * 0.5F, body.depth * 0.5F};
    const bool is_fill =
        type == QLatin1String("home") || type == QLatin1String("defense_tower");
    const double was_x = entry.value("x").toDouble();
    const double was_z = entry.value("z").toDouble();
    if (!settle_placement(field,
                          reachable,
                          map,
                          footprint,
                          true,
                          nullptr,
                          is_fill,
                          is_fill ? 8.0F : 3.0F,
                          entry)) {
      if (!is_fill) {
        structures.push_back(entry);
        continue;
      }
      ++tally.dropped;
      continue;
    }
    tally.moved +=
        (entry.value("x").toDouble() != was_x || entry.value("z").toDouble() != was_z)
            ? 1
            : 0;
    structures.push_back(entry);
  }
  root.insert("structures", structures);

  const std::vector<SolidRect> solids = structure_bodies(root);
  QJsonArray spawns;
  for (const auto value : root.value("spawns").toArray()) {
    QJsonObject entry = value.toObject();
    const double was_x = entry.value("x").toDouble();
    const double was_z = entry.value("z").toDouble();
    if (!settle_placement(field,
                          reachable,
                          map,
                          {k_spawn_body_half, k_spawn_body_half},
                          false,
                          &solids,
                          true,
                          12.0F,
                          entry)) {
      ++tally.dropped;
      continue;
    }
    tally.moved +=
        (entry.value("x").toDouble() != was_x || entry.value("z").toDouble() != was_z)
            ? 1
            : 0;
    spawns.push_back(entry);
  }
  root.insert("spawns", spawns);

  return tally;
}

[[nodiscard]] auto drop_what_cannot_stand(const Game::Map::TerrainHeightMap& field,
                                          const std::vector<std::uint8_t>& reachable,
                                          QJsonObject& root) -> int {
  int dropped = 0;
  const QJsonArray in = root.value(QLatin1String("world_props")).toArray();
  QJsonArray out;
  for (const auto value : in) {
    const QJsonObject entry = value.toObject();
    auto prop_type = Game::Map::WorldProp::Type::Boulder;
    (void)Game::Map::world_prop_type_from_string(entry.value("type").toString(),
                                                 prop_type);
    const auto extents = Game::Map::world_prop_ground_half_extents(
        prop_type, static_cast<float>(entry.value("scale").toDouble(1.0)));
    if (ground_is_settled(field,
                          reachable,
                          entry.value("x").toDouble(),
                          entry.value("z").toDouble(),
                          {extents.x, extents.z})) {
      out.push_back(entry);
    } else {
      ++dropped;
    }
  }
  root.insert(QLatin1String("world_props"), out);
  return dropped;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  QTextStream err(stderr);

  const QString output = argc > 1
                             ? QString::fromLocal8Bit(argv[1])
                             : QStringLiteral("assets/maps/map_aurelia_magna.json");

  const CityPlan plan = Arena::Scenarios::authored_city_plan();

  QJsonObject root;
  root.insert("name", QStringLiteral("Aurelia Magna"));
  root.insert("description", plan.definition.description);
  root.insert("theme",
              QStringLiteral("Three walls, nine gates, and every field outside them."));
  root.insert("generated_by", QStringLiteral("tools/city_export"));
  root.insert("coord_system", QStringLiteral("world"));
  root.insert("max_troops_per_player", 3600);
  root.insert("grid",
              QJsonObject{{"width", k_grid_extent},
                          {"height", k_grid_extent},
                          {"tile_size", 1.0}});

  root.insert("camera",
              QJsonObject{{"center", QJsonArray{0.0, 0.0, -40.0}},
                          {"distance", 330.0},
                          {"tilt_deg", 48.0},
                          {"yaw", 225.0},
                          {"fov_y", 45.0},
                          {"near", 1.0},
                          {"far", 1800.0}});
  root.insert("time_of_day", QStringLiteral("day"));
  root.insert(
      "environment",
      QJsonObject{{"start_time", rounded(plan.definition.environment.start_time)},
                  {"time_mode", QStringLiteral("locked")},
                  {"day_length_seconds", 1800.0},
                  {"lighting_profile", QStringLiteral("clear_day")}});
  root.insert("biome",
              QJsonObject{{"ground_type", plan.definition.ground_type},
                          {"seed", plan.definition.terrain_seed_override},

                          {"height_noise",
                           QJsonArray{rounded(k_height_noise),
                                      rounded(k_height_noise_frequency)}},
                          {"patch_density", rounded(k_patch_density)},
                          {"patch_jitter", rounded(k_patch_jitter)},
                          {"plant_density", rounded(k_plant_density)},
                          {"moisture_level", rounded(k_moisture)},
                          {"ground_irregularity_enabled", true},
                          {"irregularity_scale", rounded(k_irregularity_scale)},
                          {"irregularity_amplitude", rounded(k_irregularity_amplitude)},
                          {"procedural_boulders_enabled", false},
                          {"procedural_iron_ore_enabled", false},
                          {"procedural_trees_enabled", false}});

  const auto steps = terrain_steps(plan);
  write_terrain(plan, steps, root);
  write_water(plan, root);
  write_structures(plan, root);
  write_spawns(plan, root);
  write_world_props(plan, root);
  write_wildlife(plan, root);
  root.insert("rain",
              QJsonObject{{"enabled", false},
                          {"type", QStringLiteral("rain")},
                          {"intensity", 0.0}});
  root.insert("victory",
              QJsonObject{{"type", QStringLiteral("elimination")},
                          {"key_structures", QJsonArray{QStringLiteral("barracks")}},
                          {"defeat_conditions",
                           QJsonArray{QStringLiteral("no_key_structures"),
                                      QStringLiteral("no_commander"),
                                      QStringLiteral("only_commander_remaining")}}});
  root.insert("forests", QJsonArray{});
  root.insert("undead_zones", QJsonArray{});
  root.insert(
      "starting_resources",
      QJsonObject{
          {"gold", 600}, {"food", 500}, {"wood", 400}, {"stone", 300}, {"iron", 150}});

  const auto write_map = [&output, &err](const QJsonObject& document) -> bool {
    QFile file(output);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      err << "city_export: cannot write " << output << "\n";
      return false;
    }
    file.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
    file.close();
    return true;
  };

  if (!write_map(root)) {
    return 1;
  }

  QString load_error;
  Game::Map::MapDefinition written;
  const Game::Map::TerrainHeightMap field =
      field_of_written_map(output, written, &load_error);
  if (field.get_width() <= 1) {
    err << "city_export: cannot read back " << output << ": " << load_error << "\n";
    return 1;
  }

  const int offset = (k_grid_extent - 1) / 2;
  const QVector3D camp = first_player_barracks(root);
  const auto reachable =
      reachable_from_camp(field,
                          static_cast<int>(std::lround(camp.x())) + offset,
                          static_cast<int>(std::lround(camp.z())) + offset);
  const SettleTally settled = settle_gameplay_bodies(field, reachable, written, root);
  const int dropped = drop_what_cannot_stand(field, reachable, root);

  if (!write_map(root)) {
    return 1;
  }

  QTextStream out(stdout);
  out << "wrote " << output << "\n"
      << "  structures  " << root.value("structures").toArray().size() << "\n"
      << "  spawns      " << root.value("spawns").toArray().size() << "\n"
      << "  world_props " << root.value("world_props").toArray().size() << "\n"
      << "  terrain     " << root.value("terrain").toArray().size() << "\n"
      << "  roads       " << root.value("roads").toArray().size() << "\n"
      << "  dressing dropped off settled ground " << dropped << "\n"
      << "  gameplay bodies settled " << settled.moved << ", dropped "
      << settled.dropped << "\n";
  return 0;
}
