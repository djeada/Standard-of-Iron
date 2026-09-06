#include "arena_city_scenarios.h"

#include <QHash>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/units/spawn_type.h"
#include "utils/resource_utils.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Trigger = ScenarioTriggerKind;

constexpr const char* k_city_map = ":/assets/maps/map_aurelia_magna.json";

constexpr float k_content_half = 300.0F;
constexpr int k_grid_extent = 769;

auto expectation(Expect kind,
                 QString source = {},
                 float threshold = 0.0F,
                 float start = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.threshold = threshold;
  result.start_seconds = start;
  return result;
}

auto at(float time,
        Command command,
        QString source = {},
        QString target = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto harvest_at(float time, QString source, const char* kind) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::HarvestResource;
  result.group = std::move(source);
  result.resource_kind = QString::fromLatin1(kind);
  return result;
}

auto deliver_at(float time, QString source, QString structure) -> ArenaScenarioStep {
  auto result = at(time, Command::DeliverToStructure, std::move(source));
  result.target_group = std::move(structure);
  return result;
}

auto repair_at(float time, QString source, QString structure) -> ArenaScenarioStep {
  auto result = at(time, Command::RepairStructure, std::move(source));
  result.target_group = std::move(structure);
  return result;
}

auto set_value(float time,
               Command command,
               QString target,
               int value) -> ArenaScenarioStep {
  auto result = at(time, command, std::move(target));
  result.value = value;
  return result;
}

auto march_to(float time, QString source, QVector3D destination) -> ArenaScenarioStep {
  auto result = at(time, Command::FormationMove, std::move(source));
  result.destination = destination;
  return result;
}

[[nodiscard]] auto has_group(const ArenaScenarioDefinition& scenario,
                             const QString& name) -> bool {
  return std::any_of(scenario.groups.begin(),
                     scenario.groups.end(),
                     [&](const auto& item) { return item.name == name; });
}

struct Decoration {
  const char* prefix;
  int individuals;
  const char* routine;
};

constexpr Decoration k_decorations[] = {
    {"capital_sanctum_choir", 7, "rest_kneel:9.0:0.8"},
    {"capital_oracle_choir", 6, "rest_kneel:9.0:0.8"},
    {"capital_temple_healers", 6, "rest_kneel:9.0:0.8"},
    {"capital_hospice_healers", 6, "rest_kneel:9.0:0.8"},
    {"capital_garrison_horse", 8, nullptr},
    {"capital_garrison_archers", 12, nullptr},
    {"capital_garrison", 16, nullptr},
    {"capital_temple_guard", 10, nullptr},
    {"capital_gate_watch", 8, nullptr},
    {"capital_north_watch", 8, nullptr},
    {"capital_column_horse", 8, nullptr},
    {"capital_column_archers", 12, nullptr},
    {"capital_column", 16, nullptr},
    {"capital_masons", 10, nullptr},
    {"capital_repair_crew", 8, nullptr},
    {"capital_carriers", 9, nullptr},
    {"capital_foresters", 8, nullptr},
    {"capital_quarriers", 8, nullptr},
    {"capital_miners", 8, nullptr},
    {"capital_reapers", 8, nullptr},
    {"capital_shepherds", 6, nullptr},
    {"capital_wardens", 6, nullptr},
    {"capital_quarry_crew", 4, nullptr},
    {"capital_upper_healers", 5, nullptr},
    {"capital_forum_healers", 5, nullptr},
};

[[nodiscard]] auto decoration_for(const QString& name) -> const Decoration* {
  const Decoration* best = nullptr;
  std::size_t best_length = 0;
  for (const auto& entry : k_decorations) {
    const QLatin1String prefix(entry.prefix);
    if (!name.startsWith(prefix)) {
      continue;
    }
    const auto length = static_cast<std::size_t>(prefix.size());
    if (length > best_length) {
      best = &entry;
      best_length = length;
    }
  }
  return best;
}

struct Bucket {
  QString name;
  std::vector<QVector3D> positions;
  Spawn type{Spawn::Civilian};
  float facing{0.0F};
  bool resident{false};
  float roam{16.0F};
  std::vector<QVector3D> patrol_route;
};

void finish_bucket(const Bucket& bucket, ArenaScenarioDefinition& out) {
  if (bucket.positions.empty()) {
    return;
  }
  ArenaScenarioGroup group;
  group.name = bucket.name;
  group.owner_id = 1;
  group.nation_id = Nation::RomanRepublic;
  group.count = static_cast<int>(bucket.positions.size());
  group.facing_degrees = bucket.facing;
  group.settlement_resident = bucket.resident;
  group.settlement_roam_radius = bucket.roam;

  if (Game::Units::is_building_spawn(bucket.type)) {
    group.spawn_type = bucket.type;
  } else {
    const auto troop = Game::Units::spawn_typeToTroopType(bucket.type);
    if (!troop.has_value()) {
      return;
    }
    group.troop_type = *troop;
  }

  const QVector3D first = bucket.positions.front();
  const QVector3D last = bucket.positions.back();
  group.origin = (first + last) * 0.5F;
  group.spacing = group.count > 1 ? (last - first) / static_cast<float>(group.count - 1)
                                  : QVector3D(2.6F, 0.0F, 0.0F);

  if (const auto* decoration = decoration_for(bucket.name)) {
    group.individuals_per_unit = decoration->individuals;
    if (decoration->routine != nullptr) {
      group.showcase_routine = {QString::fromLatin1(decoration->routine)};
      group.showcase_loop = true;
    }
  }

  out.groups.push_back(std::move(group));
}

void read_city(const Game::Map::MapDefinition& map, ArenaScenarioDefinition& out) {
  out.roads = map.roads;
  out.rivers = map.rivers;
  out.lakes = map.lakes;
  out.bridges = map.bridges;
  out.terrain_features = map.terrain;

  std::vector<Bucket> buckets;
  QHash<QString, std::size_t> index_of;
  const auto bucket_for = [&](const QString& name) -> Bucket& {
    const auto found = index_of.constFind(name);
    if (found != index_of.constEnd()) {
      return buckets[*found];
    }
    index_of.insert(name, buckets.size());
    buckets.push_back(Bucket{.name = name});
    return buckets.back();
  };

  for (const auto& entry : map.structures) {
    if (entry.player_id != 1) {
      continue;
    }
    const QString name = entry.group.isEmpty() ? entry.id : entry.group;
    if (const auto* line =
            std::get_if<Game::Map::LineStructureGeometry>(&entry.geometry)) {

      const QVector3D span = line->end - line->start;
      const int count = static_cast<int>(std::lround(
                            (std::abs(span.x()) + std::abs(span.z())) / 2.0F)) +
                        1;
      ArenaScenarioGroup wall;
      wall.name = name;
      wall.spawn_type = Spawn::WallSegment;
      wall.owner_id = 1;
      wall.nation_id = Nation::RomanRepublic;
      wall.count = count;
      wall.origin = (line->start + line->end) * 0.5F;
      wall.spacing = std::abs(span.x()) >= std::abs(span.z())
                         ? QVector3D(2.0F, 0.0F, 0.0F)
                         : QVector3D(0.0F, 0.0F, 2.0F);
      wall.facing_degrees = std::abs(span.x()) >= std::abs(span.z()) ? 0.0F : 90.0F;
      out.groups.push_back(std::move(wall));
      continue;
    }
    const auto* point = std::get_if<Game::Map::PointStructureGeometry>(&entry.geometry);
    if (point == nullptr) {
      continue;
    }
    Bucket& bucket = bucket_for(name);
    bucket.type = entry.type;
    bucket.facing = entry.rotation;
    bucket.positions.push_back(point->position);
  }

  for (const auto& spawn : map.spawns) {
    if (spawn.player_id != 1) {

      continue;
    }
    const QString name = spawn.group.isEmpty() ? spawn.id : spawn.group;
    Bucket& bucket = bucket_for(name);
    bucket.type = spawn.type;
    bucket.resident = spawn.behavior == QStringLiteral("guard");
    bucket.roam = spawn.guard_radius;
    if (bucket.patrol_route.empty()) {
      bucket.patrol_route = spawn.patrol_waypoints;
    }
    bucket.positions.emplace_back(spawn.x, 0.0F, spawn.z);
  }

  for (const auto& bucket : buckets) {
    finish_bucket(bucket, out);
  }

  for (const auto& bucket : buckets) {
    if (bucket.patrol_route.empty()) {
      continue;
    }
    float when = 1.5F;
    std::size_t leg = 0;
    while (when < 180.0F) {
      out.steps.push_back(march_to(
          when, bucket.name, bucket.patrol_route[leg % bucket.patrol_route.size()]));
      when += 16.0F;
      ++leg;
    }
  }

  for (const auto& prop : map.world_props) {
    ArenaScenarioResourcePatch patch;
    patch.prop_type =
        QLatin1String(Game::Map::world_prop_type_to_string(prop.type)).toString();
    patch.count = 1;
    patch.origin = QVector3D(prop.x, 0.0F, prop.z);
    patch.spacing = {};
    patch.scale = prop.scale;

    patch.exact = true;
    out.resource_patches.push_back(std::move(patch));
  }

  out.wildlife = map.wildlife;
  out.environment = map.environment;
}

void add_script(ArenaScenarioDefinition& scenario) {
  struct Drill {
    const char* group;
    QVector3D near_post;
    QVector3D far_post;
  };
  for (auto const& drill :
       {Drill{"capital_garrison_spears",
              {112.0F, 0.0F, -114.0F},
              {140.0F, 0.0F, -128.0F}},
        Drill{
            "capital_garrison_horse", {96.0F, 0.0F, -60.0F}, {150.0F, 0.0F, -96.0F}}}) {
    if (!has_group(scenario, QString::fromLatin1(drill.group))) {
      continue;
    }
    bool out = true;
    for (float when = 6.0F; when < 180.0F; when += 14.0F) {
      scenario.steps.push_back(march_to(when,
                                        QString::fromLatin1(drill.group),
                                        out ? drill.far_post : drill.near_post));
      out = !out;
    }
  }

  struct Job {
    const char* crew;
    const char* kind;
  };
  for (auto const& job : {Job{"capital_foresters_west", "tree"},
                          Job{"capital_foresters_east", "tree"},
                          Job{"capital_foresters_south", "tree"},
                          Job{"capital_quarriers", "boulder"},
                          Job{"capital_quarriers_west", "boulder"},
                          Job{"capital_miners", "iron_ore"},
                          Job{"capital_reapers_west", "grain"},
                          Job{"capital_reapers_east", "grain"},
                          Job{"capital_shepherds", "sheep"},
                          Job{"capital_wardens", "grain"},
                          Job{"capital_carriers_timber", "tree"},
                          Job{"capital_carriers_quarry", "boulder"},
                          Job{"capital_carriers_stone", "boulder"},
                          Job{"capital_carriers_iron", "iron_ore"},
                          Job{"capital_carriers_grain", "grain"},
                          Job{"capital_carriers_farm", "grain"}}) {
    if (!has_group(scenario, QString::fromLatin1(job.crew))) {
      continue;
    }
    for (float when = 2.5F; when < 180.0F; when += 12.0F) {
      scenario.steps.push_back(
          harvest_at(when, QString::fromLatin1(job.crew), job.kind));
    }
  }

  struct Haul {
    const char* crew;
    const char* structure;
  };
  for (auto const& haul : {Haul{"capital_carriers_forum", "capital_basilica"},
                           Haul{"capital_carriers_docks", "capital_dock_hall_0"},
                           Haul{"capital_carriers_barracks", "capital_armoury_0"},
                           Haul{"capital_carriers_upper", "capital_curia"}}) {
    const QString structure = QString::fromLatin1(haul.structure);
    if (!has_group(scenario, structure)) {
      continue;
    }
    for (float when = 4.0F; when < 180.0F; when += 13.0F) {
      scenario.steps.push_back(
          deliver_at(when, QString::fromLatin1(haul.crew), structure));
    }
  }

  struct BuildSite {
    const char* crew;
    const char* structure;
    int health;
  };
  for (auto const& site :
       {BuildSite{"capital_masons_forum", "capital_site_forum", 250},
        BuildSite{"capital_masons_upper", "capital_site_upper", 265},
        BuildSite{"capital_masons_docks", "capital_site_docks", 245},
        BuildSite{"capital_masons_wall", "capital_site_wall", 255},
        BuildSite{"capital_masons_circus", "capital_site_circus", 250},
        BuildSite{"capital_repair_crew", "capital_site_avenue", 240},
        BuildSite{"capital_repair_crew_east", "capital_site_circus", 248},
        BuildSite{"capital_masons", "capital_mint", 240}}) {
    const QString crew = QString::fromLatin1(site.crew);
    const QString structure = QString::fromLatin1(site.structure);
    if (!has_group(scenario, crew) || !has_group(scenario, structure)) {
      continue;
    }
    scenario.steps.push_back(
        set_value(0.5F, Command::SetHealth, structure, site.health));
    for (float when = 2.0F; when < 180.0F; when += 14.0F) {
      scenario.steps.push_back(repair_at(when, crew, structure));
      scenario.steps.push_back(
          set_value(when + 12.0F, Command::SetHealth, structure, site.health));
    }
  }
}

auto imperial_capital() -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = QString::fromLatin1(k_imperial_capital_id);
  s.label = QStringLiteral("Aurelia Magna: Imperial Capital");
  s.description = QStringLiteral(
      "An enormous capital on a 768 m stage, built in rings. Farmland, orchards, "
      "quarries, lakes and a ruined old town fill the countryside; a great "
      "eight-gated wall encloses the new city with its circus, theatre, docks and "
      "insulae; an older Republican circuit holds the forum and the ancient core; "
      "and a walled citadel crowns the sacred mountain behind the whole thing, "
      "where the healers keep the temples. A legion crosses the bridge and marches "
      "the sacred way to the mountain while patrols walk the streets and the "
      "garrison drills at the barracks.");
  s.duration_seconds = 190.0F;
  s.camera = {470.0F, 52.0F, 16.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, -120.0F);
  s.arena_floor_half_extent = k_content_half;
  s.terrain_grid_extent = k_grid_extent;
  s.ground_type = QStringLiteral("soil_fertile");
  s.terrain_seed_override = 20260819;
  s.terrain_height_scale_override = 42.0F;
  s.suppress_boundary_mountains = true;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 11.4F;
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.environment.fog_density_override = 0.0009F;
  s.environment.exposure_override = 1.04F;

  Game::Map::MapDefinition map;
  QString error;
  if (!Game::Map::MapLoader::load_from_json_file(
          Utils::Resources::resolve_resource_path(QString::fromLatin1(k_city_map)),
          map,
          &error)) {
    qWarning() << "Arena: cannot read the city map" << k_city_map << ":" << error;
    return s;
  }
  read_city(map, s);
  add_script(s);

  for (const auto& item : s.groups) {
    if (item.spawn_type.has_value() && *item.spawn_type == Spawn::Farm) {
      s.steps.push_back(set_value(0.5F, Command::SetFarmGrowth, item.name, 100));
    }
  }

  for (auto const& name : {QStringLiteral("capital_temple"),
                           QStringLiteral("capital_oracle"),
                           QStringLiteral("capital_basilica"),
                           QStringLiteral("capital_curia"),
                           QStringLiteral("capital_barracks_north_0"),
                           QStringLiteral("capital_barracks_mid_0"),
                           QStringLiteral("capital_farm_0"),
                           QStringLiteral("capital_dock_hall_0"),
                           QStringLiteral("capital_gate_south"),
                           QStringLiteral("capital_gate_north"),
                           QStringLiteral("capital_gate_west"),
                           QStringLiteral("capital_gate_east"),
                           QStringLiteral("capital_gate_docks"),
                           QStringLiteral("oldwall_gate_sacred"),
                           QStringLiteral("oldwall_gate_forum"),
                           QStringLiteral("citadel_gate_citadel"),
                           QStringLiteral("capital_tower_nw"),
                           QStringLiteral("capital_tower_se")}) {
    s.expectations.push_back(expectation(Expect::GroupExists, name));
  }
  for (auto const& name : {QStringLiteral("capital_forum_crowd"),
                           QStringLiteral("capital_south_quarter_folk"),
                           QStringLiteral("capital_north_quarter_folk"),
                           QStringLiteral("capital_west_quarter_folk"),
                           QStringLiteral("capital_east_quarter_folk"),
                           QStringLiteral("capital_market_crowd"),
                           QStringLiteral("capital_core_folk"),
                           QStringLiteral("capital_circus_crowd"),
                           QStringLiteral("capital_theatre_crowd"),
                           QStringLiteral("capital_temple_pilgrims"),
                           QStringLiteral("capital_patrol_avenue"),
                           QStringLiteral("capital_patrol_decumanus")}) {
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
  }
  s.expectations.push_back(
      expectation(Expect::GateOpenedObserved, QStringLiteral("capital_gate_south")));
  s.expectations.push_back(expectation(Expect::UnitsClearOfBuildings,
                                       QStringLiteral("capital_column_swords")));
  s.expectations.push_back(
      expectation(Expect::NoRootTeleport, QStringLiteral("capital_column_swords")));

  return s;
}

} // namespace

auto build_city_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(imperial_capital());
  return result;
}

} // namespace Arena::Scenarios
