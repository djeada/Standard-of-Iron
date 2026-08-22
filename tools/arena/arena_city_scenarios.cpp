#include "arena_city_scenarios.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/systems/building_collision_registry.h"
#include "game/units/spawn_type.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

[[nodiscard]] auto plot_hash(int a, int b, int c) -> unsigned {
  unsigned value =
      static_cast<unsigned>((a * 73856093) ^ (b * 19349663) ^ (c * 83492791));
  value ^= value >> 13U;
  value *= 2246822519U;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] auto plot_unit(int a, int b, int c) -> float {
  return static_cast<float>(plot_hash(a, b, c) % 1024U) / 1023.0F;
}

constexpr int k_grid_extent = 768;
constexpr float k_world_half = 384.0F;
constexpr float k_content_half = 300.0F;

constexpr float k_gate_tower_exclusion = 12.0F;
constexpr float k_wall_west = -170.0F;
constexpr float k_wall_east = 170.0F;
constexpr float k_wall_north = -152.0F;
constexpr float k_wall_south = 120.0F;

constexpr float k_old_west = -92.0F;
constexpr float k_old_east = 92.0F;
constexpr float k_old_north = -124.0F;
constexpr float k_old_south = 4.0F;

constexpr float k_citadel_west = -48.0F;
constexpr float k_citadel_east = 48.0F;
constexpr float k_citadel_north = -350.0F;
constexpr float k_citadel_south = -258.0F;

constexpr float k_mountain_z = -306.0F;

constexpr float k_avenue_x = 0.0F;
constexpr float k_decumanus_z = -32.0F;
constexpr float k_forum_z = -84.0F;
constexpr float k_cardo_west_x = -96.0F;
constexpr float k_cardo_east_x = 96.0F;
constexpr float k_ring_north_z = -108.0F;
constexpr float k_ring_south_z = 60.0F;
constexpr float k_militaris_z = -124.0F;

constexpr float k_river_z = 176.0F;
constexpr float k_river_width = 26.0F;
constexpr float k_main_bridge_width = 16.0F;
constexpr float k_second_bridge_width = 12.0F;

constexpr float k_bridge_reach =
    Game::Map::river_bank_standing_half_width(k_river_width) +
    Game::Map::bridge_bank_landing(k_main_bridge_width, k_river_width);
constexpr float k_bridge_north_z = k_river_z - k_bridge_reach;
constexpr float k_bridge_south_z = k_river_z + k_bridge_reach;

constexpr float k_upper_south = -64.0F;
constexpr float k_circus_x = 118.0F;
constexpr float k_circus_z = 24.0F;

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals = 0,
           QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 0.0F : 180.0F;
  return result;
}

auto building(QString name,
              Spawn type,
              int count,
              QVector3D origin,
              QVector3D spacing = {6.0F, 0.0F, 0.0F},
              float facing = 0.0F) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = facing;
  return result;
}

auto residents(QString name,
               int count,
               QVector3D origin,
               QVector3D spacing,
               float roam_radius) -> ArenaScenarioGroup {
  auto result = group(std::move(name), Troop::Civilian, 1, count, origin, 1, spacing);
  result.settlement_resident = true;
  result.settlement_roam_radius = roam_radius;
  return result;
}

auto street(QVector3D start, QVector3D end, float width) -> Game::Map::RoadSegment {
  return Game::Map::RoadSegment{start, end, width, QStringLiteral("stone")};
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing = {2.5F, 0.0F, 0.0F},
           float scale = 1.0F) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale};
}

void grove(ArenaScenarioDefinition& scenario,
           const char* prop_type,
           int count,
           QVector3D centre,
           float radius_x,
           float radius_z,
           float scale,
           int seed) {
  constexpr float k_two_pi = 6.28318530717958647692F;
  for (int index = 0; index < count; ++index) {
    const float angle = plot_unit(seed, index, 1) * k_two_pi;
    const float reach = std::sqrt(plot_unit(seed, index, 2));
    const float wobble = 0.82F + (plot_unit(seed, index, 3) * 0.36F);
    const QVector3D at(centre.x() + (std::cos(angle) * reach * radius_x * wobble),
                       0.0F,
                       centre.z() + (std::sin(angle) * reach * radius_z * wobble));
    scenario.resource_patches.push_back(
        {QString::fromLatin1(prop_type),
         1,
         at,
         {},
         scale * (0.82F + (plot_unit(seed, index, 4) * 0.42F))});
  }
}

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

auto worker(QString name, Troop troop, QVector3D at, int count, bool ai, int crew = 1)
    -> ArenaScenarioGroup {
  auto result = group(std::move(name), troop, 1, count, at, crew, {5.4F, 0.0F, 0.0F});
  result.ai_controlled = ai;
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

struct WallSide {
  const char* key;
  bool along_x;
  float fixed;
  float first;
  float last;
  std::vector<std::pair<float, const char*>> gates;
  std::vector<float> towers;
};

void add_tower(ArenaScenarioDefinition& scenario,
               const QString& prefix,
               const QString& suffix,
               QVector3D position) {
  scenario.groups.push_back(
      building(prefix + suffix, Spawn::DefenseTower, 1, position));
}

void add_wall_side(ArenaScenarioDefinition& scenario,
                   const QString& prefix,
                   const WallSide& side) {
  const float facing = side.along_x ? 0.0F : 90.0F;
  const QVector3D step =
      side.along_x ? QVector3D(2.0F, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, 2.0F);
  auto const place = [&](float along, float fixed) {
    return side.along_x ? QVector3D(along, 0.0F, fixed) : QVector3D(fixed, 0.0F, along);
  };

  std::vector<float> claimed;
  for (auto const& gate : side.gates) {
    for (float offset : {-4.0F, -2.0F, 0.0F, 2.0F, 4.0F}) {
      claimed.push_back(gate.first + offset);
    }
  }

  auto const near_a_gate = [&](float value) {
    return std::any_of(side.gates.begin(), side.gates.end(), [&](const auto& gate) {
      return std::abs(gate.first - value) < k_gate_tower_exclusion;
    });
  };

  std::vector<float> wall_towers;
  for (float along : side.towers) {
    if (!near_a_gate(along)) {
      wall_towers.push_back(along);
    }
  }
  claimed.insert(claimed.end(), wall_towers.begin(), wall_towers.end());

  auto const is_claimed = [&](float value) {
    return std::any_of(claimed.begin(), claimed.end(), [&](float taken) {
      return std::abs(taken - value) < 0.5F;
    });
  };

  int run_index = 0;
  float run_first = side.first;
  float previous = side.first;
  bool in_run = false;
  auto const flush = [&](float run_last) {
    if (!in_run) {
      return;
    }
    in_run = false;
    const int count = static_cast<int>(std::lround((run_last - run_first) / 2.0F)) + 1;
    const float centre = (run_first + run_last) * 0.5F;
    scenario.groups.push_back(building(prefix + QStringLiteral("_wall_%1%2")
                                                    .arg(QString::fromLatin1(side.key))
                                                    .arg(run_index++),
                                       Spawn::WallSegment,
                                       count,
                                       place(centre, side.fixed),
                                       step,
                                       facing));
  };

  for (float along = side.first; along <= side.last + 0.5F; along += 2.0F) {
    if (is_claimed(along)) {
      flush(previous);
    } else {
      if (!in_run) {
        run_first = along;
        in_run = true;
      }
      previous = along;
    }
  }
  flush(previous);

  for (auto const& gate : side.gates) {
    scenario.groups.push_back(building(prefix + QString::fromLatin1(gate.second),
                                       Spawn::WallGate,
                                       1,
                                       place(gate.first, side.fixed),
                                       {},
                                       facing));
  }

  int tower_index = 0;
  for (float along : wall_towers) {
    add_tower(scenario,
              prefix,
              QStringLiteral("_tower_%1%2")
                  .arg(QString::fromLatin1(side.key))
                  .arg(tower_index++),
              place(along, side.fixed));
  }
}

struct Circuit {
  QString prefix;
  float west;
  float east;
  float north;
  float south;
  std::vector<std::pair<float, const char*>> north_gates;
  std::vector<std::pair<float, const char*>> south_gates;
  std::vector<std::pair<float, const char*>> west_gates;
  std::vector<std::pair<float, const char*>> east_gates;
};

[[nodiscard]] auto
tower_run(float first, float last, float step) -> std::vector<float> {
  std::vector<float> values;
  for (float value = first; value <= last + 0.5F; value += step) {
    values.push_back(std::round(value / 2.0F) * 2.0F);
  }
  return values;
}

void add_circuit(ArenaScenarioDefinition& scenario, const Circuit& circuit) {
  const float span_x = circuit.east - circuit.west;
  const float span_z = circuit.south - circuit.north;
  const float tower_pitch_x = span_x / std::round(span_x / 44.0F);
  const float tower_pitch_z = span_z / std::round(span_z / 44.0F);

  add_wall_side(scenario,
                circuit.prefix,
                {.key = "n",
                 .along_x = true,
                 .fixed = circuit.north,
                 .first = circuit.west + 2.0F,
                 .last = circuit.east - 2.0F,
                 .gates = circuit.north_gates,
                 .towers = tower_run(circuit.west + tower_pitch_x,
                                     circuit.east - tower_pitch_x,
                                     tower_pitch_x)});
  add_wall_side(scenario,
                circuit.prefix,
                {.key = "s",
                 .along_x = true,
                 .fixed = circuit.south,
                 .first = circuit.west + 2.0F,
                 .last = circuit.east - 2.0F,
                 .gates = circuit.south_gates,
                 .towers = tower_run(circuit.west + tower_pitch_x,
                                     circuit.east - tower_pitch_x,
                                     tower_pitch_x)});
  add_wall_side(scenario,
                circuit.prefix,
                {.key = "w",
                 .along_x = false,
                 .fixed = circuit.west,
                 .first = circuit.north + 2.0F,
                 .last = circuit.south - 2.0F,
                 .gates = circuit.west_gates,
                 .towers = tower_run(circuit.north + tower_pitch_z,
                                     circuit.south - tower_pitch_z,
                                     tower_pitch_z)});
  add_wall_side(scenario,
                circuit.prefix,
                {.key = "e",
                 .along_x = false,
                 .fixed = circuit.east,
                 .first = circuit.north + 2.0F,
                 .last = circuit.south - 2.0F,
                 .gates = circuit.east_gates,
                 .towers = tower_run(circuit.north + tower_pitch_z,
                                     circuit.south - tower_pitch_z,
                                     tower_pitch_z)});

  add_tower(scenario,
            circuit.prefix,
            QStringLiteral("_tower_nw"),
            {circuit.west, 0.0F, circuit.north});
  add_tower(scenario,
            circuit.prefix,
            QStringLiteral("_tower_ne"),
            {circuit.east, 0.0F, circuit.north});
  add_tower(scenario,
            circuit.prefix,
            QStringLiteral("_tower_sw"),
            {circuit.west, 0.0F, circuit.south});
  add_tower(scenario,
            circuit.prefix,
            QStringLiteral("_tower_se"),
            {circuit.east, 0.0F, circuit.south});
}

void add_walls(ArenaScenarioDefinition& scenario) {
  add_circuit(
      scenario,
      {.prefix = QStringLiteral("capital"),
       .west = k_wall_west,
       .east = k_wall_east,
       .north = k_wall_north,
       .south = k_wall_south,
       .north_gates = {{k_avenue_x, "_gate_north"}, {k_cardo_east_x, "_gate_quarry"}},
       .south_gates = {{k_avenue_x, "_gate_south"},
                       {k_cardo_west_x, "_gate_orchards"},
                       {k_cardo_east_x, "_gate_docks"}},
       .west_gates = {{k_decumanus_z, "_gate_west"}, {k_ring_south_z, "_gate_pasture"}},
       .east_gates = {{k_militaris_z, "_gate_east"}, {k_decumanus_z, "_gate_road"}}});

  add_circuit(scenario,
              {.prefix = QStringLiteral("oldwall"),
               .west = k_old_west,
               .east = k_old_east,
               .north = k_old_north,
               .south = k_old_south,
               .north_gates = {{k_avenue_x, "_gate_sacred"}},
               .south_gates = {{k_avenue_x, "_gate_forum"}},
               .west_gates = {{k_decumanus_z, "_gate_old_west"}},
               .east_gates = {{k_decumanus_z, "_gate_old_east"}}});

  add_wall_side(scenario,
                QStringLiteral("upper"),
                {.key = "s",
                 .along_x = true,
                 .fixed = k_upper_south,
                 .first = k_old_west + 2.0F,
                 .last = k_old_east - 2.0F,
                 .gates = {{k_avenue_x, "_gate_upper"},
                           {-60.0F, "_gate_upper_west"},
                           {60.0F, "_gate_upper_east"}},
                 .towers = {-30.0F, 30.0F}});
  add_tower(scenario,
            QStringLiteral("upper"),
            QStringLiteral("_tower_sw"),
            {k_old_west, 0.0F, k_upper_south});
  add_tower(scenario,
            QStringLiteral("upper"),
            QStringLiteral("_tower_se"),
            {k_old_east, 0.0F, k_upper_south});

  add_circuit(scenario,
              {.prefix = QStringLiteral("citadel"),
               .west = k_citadel_west,
               .east = k_citadel_east,
               .north = k_citadel_north,
               .south = k_citadel_south,
               .north_gates = {},
               .south_gates = {{k_avenue_x, "_gate_citadel"}},
               .west_gates = {},
               .east_gates = {}});
}
[[nodiscard]] auto footprint_half(Spawn type) -> QVector2D {
  const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size(
      Game::Units::spawn_typeToQString(type).toStdString());
  return {size.width * 0.5F, size.depth * 0.5F};
}

[[nodiscard]] auto rotated_half(QVector2D half, float facing_degrees) -> QVector2D {
  constexpr float k_deg_to_rad = 0.01745329251994329577F;
  const float radians = facing_degrees * k_deg_to_rad;
  const float cosine = std::abs(std::cos(radians));
  const float sine = std::abs(std::sin(radians));
  return {(cosine * half.x()) + (sine * half.y()),
          (sine * half.x()) + (cosine * half.y())};
}

class CityPlanner {
public:
  explicit CityPlanner(ArenaScenarioDefinition& scenario)
      : m_scenario(scenario) {}

  void note_existing() {
    for (const auto& item : m_scenario.groups) {
      if (!item.spawn_type.has_value() ||
          !Game::Units::is_building_spawn(*item.spawn_type)) {
        continue;
      }
      const QVector2D half =
          rotated_half(footprint_half(*item.spawn_type), item.facing_degrees);
      const float centre = (static_cast<float>(item.count) - 1.0F) * 0.5F;
      for (int index = 0; index < item.count; ++index) {
        const QVector3D at =
            item.origin + (item.spacing * (static_cast<float>(index) - centre));
        m_taken.push_back({at.x(), at.z(), half.x(), half.y()});
      }
    }
  }

  auto place(const QString& name, Spawn type, QVector3D at, float facing, bool nudge)
      -> bool {
    const QVector2D half = rotated_half(footprint_half(type), facing);
    QVector3D chosen = at;
    if (!fits(chosen, half)) {
      if (!nudge) {
        return false;
      }
      bool found = false;
      for (float reach : {1.6F, 3.2F, 4.8F}) {
        for (int step = 0; step < 8 && !found; ++step) {
          const float angle = 0.7853981F * static_cast<float>(step);
          const QVector3D candidate(at.x() + (std::cos(angle) * reach),
                                    0.0F,
                                    at.z() + (std::sin(angle) * reach));
          if (fits(candidate, half)) {
            chosen = candidate;
            found = true;
          }
        }
        if (found) {
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
    m_taken.push_back({chosen.x(), chosen.z(), half.x(), half.y()});
    m_scenario.groups.push_back(building(name, type, 1, chosen, {}, facing));
    return true;
  }

private:
  struct Taken {
    float x;
    float z;
    float half_w;
    float half_d;
  };

  [[nodiscard]] static auto
  segment_distance(float px, float pz, QVector3D start, QVector3D end) -> float {
    const float dx = end.x() - start.x();
    const float dz = end.z() - start.z();
    const float length_sq = (dx * dx) + (dz * dz);
    float t = 0.0F;
    if (length_sq > 1.0e-6F) {
      t = std::clamp(
          (((px - start.x()) * dx) + ((pz - start.z()) * dz)) / length_sq, 0.0F, 1.0F);
    }
    return std::hypot(px - (start.x() + (dx * t)), pz - (start.z() + (dz * t)));
  }

  [[nodiscard]] auto fits(QVector3D at, QVector2D half) const -> bool {
    constexpr float k_gap =
        (2.0F * Game::Systems::k_default_building_grid_padding) + 0.9F;
    constexpr float k_road_margin = 0.8F;
    constexpr float k_water_margin = 2.0F;
    constexpr float k_bridge_lane = 4.0F;

    const float minor = std::min(half.x(), half.y());
    for (const auto& road : m_scenario.roads) {
      if (segment_distance(at.x(), at.z(), road.start, road.end) <
          (road.width * 0.5F) + minor + k_road_margin) {
        return false;
      }
    }
    for (const auto& river : m_scenario.rivers) {
      if (segment_distance(at.x(), at.z(), river.start, river.end) <
          Game::Map::river_bank_standing_half_width(river.width) + minor +
              k_water_margin) {
        return false;
      }
    }
    for (const auto& lake : m_scenario.lakes) {
      if (Game::Map::point_in_lake(lake, at.x(), at.z(), minor + k_water_margin)) {
        return false;
      }
    }
    for (const auto& bridge : m_scenario.bridges) {
      const float low = std::min(bridge.start.z(), bridge.end.z()) - 13.0F;
      const float high = std::max(bridge.start.z(), bridge.end.z()) + 13.0F;
      if (at.z() > low && at.z() < high &&
          std::abs(at.x() - bridge.start.x()) < half.x() + k_bridge_lane) {
        return false;
      }
    }
    for (const auto& taken : m_taken) {
      const bool overlap_x =
          std::abs(at.x() - taken.x) < half.x() + taken.half_w + k_gap;
      const bool overlap_z =
          std::abs(at.z() - taken.z) < half.y() + taken.half_d + k_gap;
      if (overlap_x && overlap_z) {
        return false;
      }
    }
    return true;
  }

  ArenaScenarioDefinition& m_scenario;
  std::vector<Taken> m_taken;
};

struct District {
  const char* name;
  float x0;
  float z0;
  float x1;
  float z1;
  float pitch_x;
  float pitch_z;
  float rotation_deg;
  float density;
  int accent_every;
  Spawn accent;
};

void add_colonnade(CityPlanner& planner,
                   const QString& name,
                   QVector3D from,
                   QVector3D to,
                   float pitch,
                   float facing) {
  const float span_x = to.x() - from.x();
  const float span_z = to.z() - from.z();
  const float length = std::hypot(span_x, span_z);
  if (length < pitch) {
    return;
  }
  const int count = static_cast<int>(length / pitch) + 1;
  for (int index = 0; index < count; ++index) {
    const float t = static_cast<float>(index) / static_cast<float>(count - 1);
    planner.place(QStringLiteral("%1_%2").arg(name).arg(index),
                  Spawn::Temple,
                  {from.x() + (span_x * t), 0.0F, from.z() + (span_z * t)},
                  facing,
                  false);
  }
}

void add_temple_precinct(CityPlanner& planner, QVector3D centre) {
  constexpr float k_column_pitch = 9.6F;
  constexpr float k_row_pitch = 9.0F;

  struct Cell {
    const char* suffix;
    float columns;
    float rows;
  };

  for (auto const& cell : {Cell{"apex", 0.0F, -1.0F},
                           Cell{"west", -1.0F, 0.0F},
                           Cell{"east", 1.0F, 0.0F},
                           Cell{"front_west", -0.5F, 1.0F},
                           Cell{"front_east", 0.5F, 1.0F}}) {
    planner.place(QStringLiteral("capital_sanctum_%1").arg(QLatin1String(cell.suffix)),
                  Spawn::Temple,
                  {centre.x() + (cell.columns * k_column_pitch),
                   0.0F,
                   centre.z() + (cell.rows * k_row_pitch)},
                  180.0F,
                  false);
  }
}

void fill_district(CityPlanner& planner, const District& district, int seed) {
  constexpr float k_deg_to_rad = 0.01745329251994329577F;
  const float centre_x = (district.x0 + district.x1) * 0.5F;
  const float centre_z = (district.z0 + district.z1) * 0.5F;
  const float radians = district.rotation_deg * k_deg_to_rad;
  const float cos_a = std::cos(radians);
  const float sin_a = std::sin(radians);
  const float reach_x = (district.x1 - district.x0) * 0.5F;
  const float reach_z = (district.z1 - district.z0) * 0.5F;
  const float span = std::hypot(reach_x, reach_z);

  const int columns = static_cast<int>(span * 2.0F / district.pitch_x) + 1;
  const int rows = static_cast<int>(span * 2.0F / district.pitch_z) + 1;
  int index = 0;
  for (int row = -rows / 2; row <= rows / 2; ++row) {
    for (int column = -columns / 2; column <= columns / 2; ++column) {
      if (plot_unit(seed, column, row) > district.density) {
        continue;
      }
      const float jitter_x =
          (plot_unit(seed + 1, column, row) - 0.5F) * district.pitch_x * 0.16F;
      const float jitter_z =
          (plot_unit(seed + 2, column, row) - 0.5F) * district.pitch_z * 0.16F;
      const float local_x = (static_cast<float>(column) * district.pitch_x) + jitter_x;
      const float local_z = (static_cast<float>(row) * district.pitch_z) + jitter_z;
      const float world_x = centre_x + (local_x * cos_a) - (local_z * sin_a);
      const float world_z = centre_z + (local_x * sin_a) + (local_z * cos_a);
      if (world_x < district.x0 || world_x > district.x1 || world_z < district.z0 ||
          world_z > district.z1) {
        continue;
      }
      const float roll = plot_unit(seed + 5, column, row);
      Spawn type = Spawn::Home;
      if (district.accent_every > 0 && roll > 0.88F) {
        type = district.accent;
      } else if (roll > 0.76F) {
        type = Spawn::Marketplace;
      } else if (roll > 0.70F) {
        type = Spawn::Temple;
      }
      const float lean = (plot_unit(seed + 3, column, row) - 0.5F) * 30.0F;
      const float quarter_turn =
          (plot_unit(seed + 4, column, row) > 0.78F) ? 90.0F : 0.0F;
      const float facing = district.rotation_deg + ((row % 2 == 0) ? 180.0F : 0.0F) +
                           lean + quarter_turn;
      if (planner.place(QStringLiteral("%1_%2")
                            .arg(QString::fromLatin1(district.name))
                            .arg(index),
                        type,
                        {world_x, 0.0F, world_z},
                        facing,
                        false)) {
        ++index;
      }
    }
  }
}

void add_ring(CityPlanner& planner,
              const QString& prefix,
              QVector3D centre,
              float radius_x,
              float radius_z,
              int count,
              float start_degrees) {
  constexpr float k_deg_to_rad = 0.01745329251994329577F;
  int placed = 0;
  for (int index = 0; index < count; ++index) {
    const float degrees = start_degrees + (360.0F * static_cast<float>(index) /
                                           static_cast<float>(count));
    const float radians = degrees * k_deg_to_rad;
    const QVector3D at(centre.x() + (std::cos(radians) * radius_x),
                       0.0F,
                       centre.z() + (std::sin(radians) * radius_z));
    if (planner.place(prefix + QStringLiteral("_%1").arg(index),
                      (index % 5 == 0) ? Spawn::Marketplace : Spawn::Home,
                      at,
                      degrees + 180.0F,
                      false)) {
      ++placed;
    }
  }
}

struct Landmark {
  const char* name;
  Spawn type;
  float x;
  float z;
  float facing;
};

void add_monuments(ArenaScenarioDefinition& scenario, CityPlanner& planner) {
  for (auto const& tower :
       {std::pair{"_tower_bridge_nw", QVector3D(-24.0F, 0.0F, 150.0F)},
        std::pair{"_tower_bridge_ne", QVector3D(24.0F, 0.0F, 150.0F)},
        std::pair{"_tower_bridge_sw", QVector3D(-24.0F, 0.0F, 202.0F)},
        std::pair{"_tower_bridge_se", QVector3D(24.0F, 0.0F, 202.0F)},
        std::pair{"_tower_ford_w", QVector3D(-142.0F, 0.0F, 150.0F)},
        std::pair{"_tower_ford_e", QVector3D(-98.0F, 0.0F, 150.0F)},
        std::pair{"_tower_sacred_w", QVector3D(-30.0F, 0.0F, -152.0F)},
        std::pair{"_tower_sacred_e", QVector3D(30.0F, 0.0F, -152.0F)},
        std::pair{"_tower_road_north", QVector3D(-124.0F, 0.0F, -196.0F)},
        std::pair{"_tower_road_east", QVector3D(266.0F, 0.0F, -60.0F)},
        std::pair{"_tower_road_west", QVector3D(-264.0F, 0.0F, -60.0F)},
        std::pair{"_tower_road_south", QVector3D(4.0F, 0.0F, 268.0F)}}) {
    add_tower(scenario,
              QStringLiteral("capital"),
              QString::fromLatin1(tower.first),
              tower.second);
    planner.place(
        QStringLiteral("capital_towerpad%1").arg(QString::fromLatin1(tower.first)),
        Spawn::WallSegment,
        tower.second,
        0.0F,
        false);
  }

  for (auto const& landmark : {
           Landmark{"capital_temple", Spawn::Temple, k_avenue_x, k_mountain_z, 180.0F},
           Landmark{
               "capital_oracle", Spawn::Temple, k_avenue_x, k_mountain_z - 38.0F, 0.0F},
           Landmark{"capital_treasury",
                    Spawn::Marketplace,
                    -38.0F,
                    k_mountain_z - 24.0F,
                    90.0F},
           Landmark{"capital_hospice",
                    Spawn::Marketplace,
                    38.0F,
                    k_mountain_z - 24.0F,
                    270.0F},
           Landmark{"capital_lower_temple", Spawn::Temple, -26.0F, -150.0F, 180.0F},

           Landmark{"capital_basilica", Spawn::Marketplace, -26.0F, k_forum_z, 90.0F},
           Landmark{"capital_curia", Spawn::Marketplace, 26.0F, k_forum_z, 270.0F},
           Landmark{"capital_forum_temple", Spawn::Temple, k_avenue_x, -104.0F, 180.0F},
           Landmark{"capital_forum_shrine", Spawn::Temple, k_avenue_x, -64.0F, 0.0F},
           Landmark{"capital_mint", Spawn::Marketplace, -26.0F, -16.0F, 90.0F},
           Landmark{"capital_granary", Spawn::Marketplace, 26.0F, -16.0F, 270.0F},
           Landmark{"capital_baths", Spawn::Marketplace, -52.0F, -8.0F, 0.0F},
           Landmark{"capital_library", Spawn::Marketplace, 52.0F, -8.0F, 0.0F},

           Landmark{"capital_site_forum", Spawn::Marketplace, -54.0F, -76.0F, 90.0F},
           Landmark{"capital_site_upper", Spawn::Temple, 46.0F, -106.0F, 250.0F},
           Landmark{"capital_site_docks", Spawn::Marketplace, 18.0F, 132.0F, 200.0F},
           Landmark{"capital_site_wall", Spawn::Marketplace, -130.0F, -142.0F, 20.0F},
           Landmark{"capital_site_circus", Spawn::Marketplace, 102.0F, 40.0F, 300.0F},
           Landmark{"capital_site_avenue", Spawn::Marketplace, -20.0F, 46.0F, 90.0F},
           Landmark{"capital_site_theatre", Spawn::Temple, -96.0F, 66.0F, 160.0F},
       }) {
    planner.place(QString::fromLatin1(landmark.name),
                  landmark.type,
                  {landmark.x, 0.0F, landmark.z},
                  landmark.facing,
                  true);
  }

  add_temple_precinct(planner, {k_avenue_x, 0.0F, k_mountain_z});

  add_colonnade(planner,
                QStringLiteral("capital_portico_west"),
                {-20.0F, 0.0F, -104.0F},
                {-20.0F, 0.0F, -142.0F},
                10.4F,
                90.0F);
  add_colonnade(planner,
                QStringLiteral("capital_portico_east"),
                {20.0F, 0.0F, -104.0F},
                {20.0F, 0.0F, -142.0F},
                10.4F,
                270.0F);
  add_colonnade(planner,
                QStringLiteral("capital_forum_portico_west"),
                {-40.0F, 0.0F, k_forum_z - 26.0F},
                {-40.0F, 0.0F, k_forum_z + 26.0F},
                10.4F,
                90.0F);
  add_colonnade(planner,
                QStringLiteral("capital_forum_portico_east"),
                {40.0F, 0.0F, k_forum_z - 26.0F},
                {40.0F, 0.0F, k_forum_z + 26.0F},
                10.4F,
                270.0F);
  add_colonnade(planner,
                QStringLiteral("capital_circus_portico_north"),
                {k_circus_x - 34.0F, 0.0F, k_circus_z - 28.0F},
                {k_circus_x + 34.0F, 0.0F, k_circus_z - 28.0F},
                10.4F,
                180.0F);
  add_colonnade(planner,
                QStringLiteral("capital_circus_portico_south"),
                {k_circus_x - 34.0F, 0.0F, k_circus_z + 28.0F},
                {k_circus_x + 34.0F, 0.0F, k_circus_z + 28.0F},
                10.4F,
                0.0F);

  for (int index = 0; index < 5; ++index) {
    const float x = 112.0F + (static_cast<float>(index) * 12.0F);
    planner.place(QStringLiteral("capital_barracks_north_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -178.0F},
                  180.0F,
                  true);
    planner.place(QStringLiteral("capital_barracks_mid_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -146.0F},
                  0.0F,
                  true);
    planner.place(QStringLiteral("capital_barracks_south_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -124.0F},
                  0.0F,
                  true);
  }
  for (int index = 0; index < 3; ++index) {
    planner.place(QStringLiteral("capital_armoury_%1").arg(index),
                  Spawn::Marketplace,
                  {110.0F + (static_cast<float>(index) * 14.0F), 0.0F, -76.0F},
                  0.0F,
                  true);
  }

  for (int index = 0; index < 8; ++index) {
    planner.place(QStringLiteral("capital_warehouse_%1").arg(index),
                  Spawn::Home,
                  {-84.0F + (static_cast<float>(index) * 12.0F), 0.0F, 136.0F},
                  180.0F,
                  true);
    planner.place(QStringLiteral("capital_dock_hall_%1").arg(index),
                  Spawn::Marketplace,
                  {48.0F + (static_cast<float>(index) * 13.0F), 0.0F, 138.0F},
                  180.0F,
                  true);
  }

  int farm_index = 0;
  struct Field {
    float x0;
    float z0;
    int columns;
    int rows;
    float pitch;
  };
  for (auto const& field : {Field{-268.0F, 212.0F, 5, 3, 34.0F},
                            Field{-92.0F, 212.0F, 4, 2, 34.0F},
                            Field{72.0F, 212.0F, 5, 3, 34.0F},
                            Field{-208.0F, 276.0F, 6, 2, 34.0F},
                            Field{36.0F, 276.0F, 6, 2, 34.0F},
                            Field{-292.0F, 96.0F, 2, 3, 34.0F},
                            Field{212.0F, 104.0F, 2, 3, 34.0F},
                            Field{-296.0F, -168.0F, 2, 3, 34.0F},
                            Field{240.0F, -244.0F, 2, 2, 34.0F}}) {
    for (int row = 0; row < field.rows; ++row) {
      for (int column = 0; column < field.columns; ++column) {
        const float x = field.x0 + (static_cast<float>(column) * field.pitch) +
                        ((row % 2 == 0) ? 0.0F : field.pitch * 0.4F);
        const float z = field.z0 + (static_cast<float>(row) * field.pitch);
        planner.place(QStringLiteral("capital_farm_%1").arg(farm_index),
                      Spawn::Farm,
                      {x, 0.0F, z},
                      static_cast<float>((farm_index * 37) % 360),
                      true);
        ++farm_index;
      }
    }
  }
}

void add_districts(CityPlanner& planner) {
  int seed = 4100;
  for (auto const& district : {
           District{"capital_villa_west",
                    -86.0F,
                    -118.0F,
                    -14.0F,
                    -72.0F,
                    17.0F,
                    16.0F,
                    -6.0F,
                    0.62F,
                    3,
                    Spawn::Temple},
           District{"capital_villa_east",
                    14.0F,
                    -118.0F,
                    86.0F,
                    -72.0F,
                    17.0F,
                    16.0F,
                    6.0F,
                    0.62F,
                    3,
                    Spawn::Temple},
           District{"capital_core_sw",
                    -88.0F,
                    -56.0F,
                    -8.0F,
                    0.0F,
                    9.2F,
                    8.8F,
                    -5.0F,
                    0.9F,
                    7,
                    Spawn::Marketplace},
           District{"capital_core_se",
                    8.0F,
                    -56.0F,
                    88.0F,
                    0.0F,
                    9.2F,
                    8.8F,
                    5.0F,
                    0.9F,
                    7,
                    Spawn::Marketplace},

           District{"capital_insula_nw",
                    -166.0F,
                    -148.0F,
                    -100.0F,
                    -68.0F,
                    9.4F,
                    9.0F,
                    -11.0F,
                    0.84F,
                    7,
                    Spawn::Marketplace},
           District{"capital_insula_w",
                    -166.0F,
                    -60.0F,
                    -100.0F,
                    8.0F,
                    9.4F,
                    8.8F,
                    13.0F,
                    0.84F,
                    7,
                    Spawn::Marketplace},
           District{"capital_insula_sw",
                    -166.0F,
                    16.0F,
                    -8.0F,
                    114.0F,
                    9.4F,
                    9.0F,
                    8.0F,
                    0.82F,
                    8,
                    Spawn::Marketplace},
           District{"capital_insula_s",
                    -8.0F,
                    24.0F,
                    100.0F,
                    114.0F,
                    9.2F,
                    8.8F,
                    -7.0F,
                    0.82F,
                    8,
                    Spawn::Marketplace},
           District{"capital_insula_ne",
                    100.0F,
                    -60.0F,
                    166.0F,
                    8.0F,
                    9.4F,
                    8.8F,
                    -13.0F,
                    0.84F,
                    7,
                    Spawn::Marketplace},
           District{"capital_insula_e",
                    100.0F,
                    16.0F,
                    166.0F,
                    114.0F,
                    9.4F,
                    9.0F,
                    9.0F,
                    0.82F,
                    8,
                    Spawn::Marketplace},
           District{"capital_insula_nnw",
                    -166.0F,
                    -148.0F,
                    -8.0F,
                    -132.0F,
                    9.4F,
                    8.8F,
                    4.0F,
                    0.7F,
                    0,
                    Spawn::Home},

           District{"capital_suburb_west",
                    -286.0F,
                    -110.0F,
                    -190.0F,
                    60.0F,
                    11.0F,
                    11.5F,
                    -8.0F,
                    0.5F,
                    5,
                    Spawn::Farm},
           District{"capital_suburb_northwest",
                    -308.0F,
                    -300.0F,
                    -212.0F,
                    -200.0F,
                    12.0F,
                    12.0F,
                    7.0F,
                    0.45F,
                    5,
                    Spawn::Farm},
           District{"capital_suburb_northeast",
                    212.0F,
                    -300.0F,
                    308.0F,
                    -200.0F,
                    12.0F,
                    12.0F,
                    -7.0F,
                    0.45F,
                    5,
                    Spawn::Farm},
           District{"capital_suburb_east",
                    202.0F,
                    -110.0F,
                    288.0F,
                    60.0F,
                    11.5F,
                    11.5F,
                    10.0F,
                    0.48F,
                    5,
                    Spawn::Farm},
           District{"capital_suburb_south",
                    -170.0F,
                    132.0F,
                    170.0F,
                    156.0F,
                    10.4F,
                    10.0F,
                    3.0F,
                    0.55F,
                    6,
                    Spawn::Marketplace},
           District{"capital_hamlet_west",
                    -282.0F,
                    188.0F,
                    -198.0F,
                    262.0F,
                    12.0F,
                    12.0F,
                    -9.0F,
                    0.4F,
                    4,
                    Spawn::Farm},
           District{"capital_hamlet_east",
                    188.0F,
                    246.0F,
                    274.0F,
                    292.0F,
                    12.0F,
                    12.0F,
                    8.0F,
                    0.4F,
                    4,
                    Spawn::Farm},
       }) {
    fill_district(planner, district, seed);
    seed += 17;
  }

  add_ring(planner,
           QStringLiteral("capital_circus"),
           {k_circus_x, 0.0F, k_circus_z},
           34.0F,
           23.0F,
           30,
           14.0F);
  add_ring(planner,
           QStringLiteral("capital_theatre"),
           {-124.0F, 0.0F, 46.0F},
           26.0F,
           18.0F,
           22,
           8.0F);
  add_ring(planner,
           QStringLiteral("capital_villa_court"),
           {0.0F, 0.0F, -96.0F},
           30.0F,
           18.0F,
           14,
           26.0F);
}

void add_roads(ArenaScenarioDefinition& scenario) {
  scenario.roads = {
      street({k_avenue_x, 0.0F, k_bridge_north_z}, {k_avenue_x, 0.0F, -258.0F}, 6.1F),
      street({k_avenue_x, 0.0F, k_bridge_south_z}, {k_avenue_x, 0.0F, 300.0F}, 4.8F),
      street({164.0F, 0.0F, k_decumanus_z}, {-164.0F, 0.0F, k_decumanus_z}, 5.4F),
      street({164.0F, 0.0F, k_militaris_z}, {-164.0F, 0.0F, k_militaris_z}, 4.1F),
      street({k_cardo_west_x, 0.0F, k_wall_north},
             {k_cardo_west_x, 0.0F, k_wall_south},
             4.1F),
      street({k_cardo_east_x, 0.0F, k_wall_north},
             {k_cardo_east_x, 0.0F, k_wall_south},
             4.1F),
      street({-160.0F, 0.0F, k_ring_north_z}, {160.0F, 0.0F, k_ring_north_z}, 3.4F),
      street({-160.0F, 0.0F, k_ring_south_z}, {160.0F, 0.0F, k_ring_south_z}, 3.4F),
      street({-160.0F, 0.0F, 100.0F}, {160.0F, 0.0F, 100.0F}, 3.4F),
      street({-140.0F, 0.0F, -78.0F}, {-14.0F, 0.0F, -6.0F}, 3.4F),
      street({140.0F, 0.0F, -78.0F}, {14.0F, 0.0F, -6.0F}, 3.4F),
      street({-140.0F, 0.0F, 96.0F}, {-16.0F, 0.0F, 20.0F}, 3.4F),
      street({140.0F, 0.0F, 96.0F}, {16.0F, 0.0F, 20.0F}, 3.4F),

      street({k_wall_west, 0.0F, k_decumanus_z}, {-300.0F, 0.0F, -32.0F}, 4.1F),
      street({k_wall_east, 0.0F, k_decumanus_z}, {300.0F, 0.0F, -32.0F}, 4.1F),
      street({k_wall_east, 0.0F, k_militaris_z}, {284.0F, 0.0F, -216.0F}, 3.4F),
      street({k_cardo_east_x, 0.0F, k_wall_north}, {166.0F, 0.0F, -276.0F}, 3.4F),
      street({k_cardo_west_x, 0.0F, k_wall_south}, {-180.0F, 0.0F, 244.0F}, 3.4F),
      street({k_cardo_east_x, 0.0F, k_wall_south}, {186.0F, 0.0F, 236.0F}, 3.4F),
      street({k_wall_west, 0.0F, k_ring_south_z}, {-292.0F, 0.0F, 140.0F}, 3.4F),
      street({-280.0F, 0.0F, 236.0F}, {268.0F, 0.0F, 236.0F}, 3.4F),
      street({-260.0F, 0.0F, 292.0F}, {252.0F, 0.0F, 292.0F}, 3.4F),
      street({-120.0F, 0.0F, k_bridge_north_z}, {-120.0F, 0.0F, 100.0F}, 4.1F),
      street({-120.0F, 0.0F, k_bridge_south_z}, {-120.0F, 0.0F, 236.0F}, 4.1F),
  };
}

void add_water(ArenaScenarioDefinition& scenario) {
  scenario.rivers.push_back(Game::Map::RiverSegment{
      {-292.0F, 0.0F, k_river_z}, {292.0F, 0.0F, k_river_z}, k_river_width});
  scenario.rivers.push_back(Game::Map::RiverSegment{
      {-236.0F, 0.0F, -170.0F}, {-206.0F, 0.0F, k_river_z}, 14.0F});
  scenario.lakes.push_back(
      Game::Map::Lake{{-246.0F, 0.0F, 96.0F}, 92.0F, 66.0F, 12.0F});
  scenario.lakes.push_back(
      Game::Map::Lake{{252.0F, 0.0F, 214.0F}, 84.0F, 58.0F, -22.0F});
  scenario.bridges.push_back(Game::Map::Bridge{{k_avenue_x, 0.0F, k_bridge_north_z},
                                               {k_avenue_x, 0.0F, k_bridge_south_z},
                                               k_main_bridge_width,
                                               0.6F});
  scenario.bridges.push_back(Game::Map::Bridge{{-120.0F, 0.0F, k_bridge_north_z},
                                               {-120.0F, 0.0F, k_bridge_south_z},
                                               k_second_bridge_width,
                                               0.6F});
}

void add_relief(ArenaScenarioDefinition& scenario) {
  scenario.elevation_patches = {
      {{k_avenue_x, 0.0F, k_mountain_z}, 150.0F, 58.0F, 76.0F},

      {{-92.0F, 0.0F, k_mountain_z - 34.0F}, 74.0F, 44.0F, 16.0F},
      {{86.0F, 0.0F, k_mountain_z - 46.0F}, 66.0F, 40.0F, 14.0F},
      {{-104.0F, 0.0F, k_mountain_z + 26.0F}, 62.0F, 34.0F, 10.0F},
      {{112.0F, 0.0F, k_mountain_z + 14.0F}, 70.0F, 38.0F, 12.0F},
      {{-46.0F, 0.0F, k_mountain_z - 58.0F}, 60.0F, 36.0F, 12.0F},
      {{58.0F, 0.0F, k_mountain_z - 64.0F}, 56.0F, 32.0F, 10.0F},
      {{-140.0F, 0.0F, k_mountain_z - 10.0F}, 52.0F, 22.0F, 0.0F},
      {{146.0F, 0.0F, k_mountain_z - 22.0F}, 50.0F, 20.0F, 0.0F},

      {{k_avenue_x, 0.0F, -214.0F}, 62.0F, 38.0F, 20.0F},
      {{k_avenue_x, 0.0F, -184.0F}, 48.0F, 20.0F, 16.0F},

      {{k_avenue_x, 0.0F, -96.0F}, 128.0F, 10.0F, 92.0F},
      {{124.0F, 0.0F, -114.0F}, 46.0F, 5.0F, 20.0F},
      {{-134.0F, 0.0F, -92.0F}, 40.0F, 4.0F, 18.0F},
      {{-96.0F, 0.0F, 52.0F}, 44.0F, 3.0F, 20.0F},

      {{-336.0F, 0.0F, -250.0F}, 90.0F, 88.0F, 0.0F},
      {{-352.0F, 0.0F, -70.0F}, 76.0F, 70.0F, 0.0F},
      {{344.0F, 0.0F, -250.0F}, 96.0F, 96.0F, 0.0F},
      {{356.0F, 0.0F, 30.0F}, 78.0F, 64.0F, 0.0F},
      {{-232.0F, 0.0F, -348.0F}, 88.0F, 62.0F, 0.0F},
      {{226.0F, 0.0F, -350.0F}, 82.0F, 58.0F, 0.0F},
      {{210.0F, 0.0F, 344.0F}, 84.0F, 40.0F, 0.0F},
      {{-330.0F, 0.0F, 330.0F}, 80.0F, 34.0F, 0.0F},
  };
}

void add_dressing(ArenaScenarioDefinition& scenario) {
  scenario.resource_patches = {
      patch("magic_shrine", 1, {k_avenue_x, 0.0F, k_mountain_z - 56.0F}, {}, 1.8F),
      patch("magic_shrine",
            2,
            {k_avenue_x - 17.5F, 0.0F, k_mountain_z + 4.5F},
            {35.0F, 0.0F, 0.0F},
            1.5F),
      patch("statue", 6, {-27.0F, 0.0F, -104.0F}, {0.0F, 0.0F, -8.0F}, 1.4F),
      patch("statue", 6, {27.0F, 0.0F, -104.0F}, {0.0F, 0.0F, -8.0F}, 1.4F),
      patch("statue", 6, {-47.0F, 0.0F, k_forum_z - 22.0F}, {0.0F, 0.0F, 9.0F}, 1.3F),
      patch("statue", 6, {47.0F, 0.0F, k_forum_z - 22.0F}, {0.0F, 0.0F, 9.0F}, 1.3F),
      patch(
          "statue", 2, {-11.0F, 0.0F, k_mountain_z - 20.0F}, {22.0F, 0.0F, 0.0F}, 1.5F),
      patch(
          "statue", 2, {-11.0F, 0.0F, k_mountain_z + 4.0F}, {22.0F, 0.0F, 0.0F}, 1.5F),
      patch(
          "statue", 2, {-11.0F, 0.0F, k_mountain_z + 28.0F}, {22.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-11.0F, 0.0F, -226.0F}, {22.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-11.0F, 0.0F, -196.0F}, {22.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-11.0F, 0.0F, -166.0F}, {22.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-11.0F, 0.0F, -136.0F}, {22.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-11.0F, 0.0F, k_forum_z}, {22.0F, 0.0F, 0.0F}, 1.5F),
      patch("statue", 2, {-11.0F, 0.0F, -52.0F}, {22.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-11.0F, 0.0F, 16.0F}, {22.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-11.0F, 0.0F, 76.0F}, {22.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-11.0F, 0.0F, 110.0F}, {22.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-14.0F, 0.0F, 148.0F}, {28.0F, 0.0F, 0.0F}, 1.5F),
      patch(
          "statue", 4, {k_circus_x - 5.0F, 0.0F, k_circus_z}, {3.4F, 0.0F, 0.0F}, 1.2F),
      patch("statue", 3, {-124.0F, 0.0F, 46.0F}, {3.4F, 0.0F, 0.0F}, 1.2F),

      patch("fire_camp", 2, {-12.0F, 0.0F, 112.0F}, {24.0F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 2, {-12.0F, 0.0F, -144.0F}, {24.0F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 2, {-158.0F, 0.0F, -40.0F}, {0.0F, 0.0F, 20.0F}, 1.0F),
      patch("fire_camp", 2, {158.0F, 0.0F, -40.0F}, {0.0F, 0.0F, 20.0F}, 1.0F),
      patch("fire_camp", 3, {116.0F, 0.0F, -108.0F}, {14.0F, 0.0F, 0.0F}, 0.95F),
      patch("fire_camp", 2, {-12.0F, 0.0F, -252.0F}, {24.0F, 0.0F, 0.0F}, 1.1F),

      patch("supply_cart", 6, {-46.0F, 0.0F, -76.0F}, {5.0F, 0.0F, 0.0F}, 0.95F),
      patch("supply_cart", 6, {30.0F, 0.0F, -92.0F}, {5.0F, 0.0F, 0.0F}, 0.95F),
      patch("supply_cart", 8, {-70.0F, 0.0F, 128.0F}, {6.0F, 0.0F, 0.0F}, 0.95F),
      patch("supply_cart", 8, {60.0F, 0.0F, 130.0F}, {6.0F, 0.0F, 0.0F}, 0.95F),
      patch("weapon_rack", 8, {104.0F, 0.0F, -132.0F}, {6.0F, 0.0F, 0.0F}, 1.0F),
      patch("tent", 10, {104.0F, 0.0F, -148.0F}, {6.5F, 0.0F, 0.0F}, 0.9F),
      patch("tent", 10, {-150.0F, 0.0F, -142.0F}, {6.5F, 0.0F, 0.0F}, 0.9F),
  };

  struct Scatter {
    const char* type;
    int count;
    float x;
    float z;
    float radius_x;
    float radius_z;
    float scale;
  };
  int seed = 9100;
  for (auto const& item : {
           Scatter{"olive_tree", 26, -212.0F, -104.0F, 34.0F, 26.0F, 1.2F},
           Scatter{"pine_tree", 20, -238.0F, -74.0F, 28.0F, 24.0F, 1.3F},
           Scatter{"olive_tree", 26, 212.0F, -96.0F, 34.0F, 26.0F, 1.2F},
           Scatter{"pine_tree", 20, 238.0F, -66.0F, 28.0F, 24.0F, 1.3F},
           Scatter{"olive_tree", 24, -190.0F, 214.0F, 32.0F, 24.0F, 1.2F},
           Scatter{"olive_tree", 30, -246.0F, 196.0F, 38.0F, 26.0F, 1.25F},
           Scatter{"olive_tree", 30, 200.0F, 200.0F, 38.0F, 26.0F, 1.25F},
           Scatter{"olive_tree", 26, -272.0F, 132.0F, 30.0F, 26.0F, 1.25F},
           Scatter{"olive_tree", 26, 232.0F, 138.0F, 30.0F, 26.0F, 1.25F},
           Scatter{"olive_tree", 24, -150.0F, 268.0F, 34.0F, 22.0F, 1.25F},
           Scatter{"olive_tree", 24, 140.0F, 272.0F, 34.0F, 22.0F, 1.25F},
           Scatter{"olive_tree", 22, -58.0F, 300.0F, 32.0F, 20.0F, 1.25F},
           Scatter{"olive_tree", 22, 56.0F, 306.0F, 32.0F, 20.0F, 1.25F},
           Scatter{"olive_tree", 20, -40.0F, k_mountain_z + 30.0F, 30.0F, 18.0F, 1.15F},
           Scatter{"olive_tree", 20, 40.0F, k_mountain_z + 30.0F, 30.0F, 18.0F, 1.15F},
           Scatter{"dead_tree", 12, -34.0F, k_mountain_z - 40.0F, 26.0F, 16.0F, 1.25F},
           Scatter{"ruins", 8, 30.0F, k_mountain_z - 40.0F, 24.0F, 14.0F, 1.3F},

           Scatter{"pine_tree", 34, -320.0F, -288.0F, 44.0F, 34.0F, 1.4F},
           Scatter{"pine_tree", 34, 312.0F, -300.0F, 44.0F, 34.0F, 1.4F},
           Scatter{"pine_tree", 30, -334.0F, -150.0F, 32.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 30, 334.0F, -150.0F, 32.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 26, -126.0F, -344.0F, 40.0F, 26.0F, 1.4F},
           Scatter{"pine_tree", 26, 118.0F, -346.0F, 40.0F, 26.0F, 1.4F},
           Scatter{"pine_tree", 24, 336.0F, 130.0F, 30.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 24, -336.0F, 170.0F, 30.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 22, 40.0F, 344.0F, 44.0F, 24.0F, 1.4F},
           Scatter{"pine_tree", 22, -160.0F, 340.0F, 44.0F, 24.0F, 1.4F},

           Scatter{"plant", 30, -168.0F, -100.0F, 14.0F, 34.0F, 0.9F},
           Scatter{"plant", 30, 168.0F, 20.0F, 14.0F, 34.0F, 0.9F},
           Scatter{"plant", 26, -30.0F, 86.0F, 26.0F, 16.0F, 0.9F},

           Scatter{"abandoned_home", 14, -250.0F, 24.0F, 34.0F, 26.0F, 1.2F},
           Scatter{"ruins", 10, -280.0F, 62.0F, 26.0F, 30.0F, 1.3F},
           Scatter{"dead_tree", 14, -262.0F, 94.0F, 30.0F, 22.0F, 1.25F},
           Scatter{"abandoned_home", 12, 254.0F, -86.0F, 30.0F, 26.0F, 1.2F},
           Scatter{"ruins", 9, 284.0F, -60.0F, 24.0F, 28.0F, 1.3F},

           Scatter{"boulder", 22, 172.0F, -272.0F, 32.0F, 22.0F, 1.3F},
           Scatter{"iron_ore", 14, 206.0F, -288.0F, 22.0F, 16.0F, 1.1F},
           Scatter{"boulder", 20, -244.0F, -262.0F, 32.0F, 22.0F, 1.3F},

           Scatter{"plant", 60, -230.0F, 176.0F, 60.0F, 34.0F, 1.0F},
           Scatter{"plant", 60, -80.0F, 184.0F, 60.0F, 34.0F, 1.0F},
           Scatter{"plant", 60, 96.0F, 180.0F, 60.0F, 34.0F, 1.0F},
           Scatter{"plant", 60, 244.0F, 190.0F, 48.0F, 34.0F, 1.0F},
           Scatter{"plant", 60, -196.0F, 262.0F, 60.0F, 36.0F, 1.0F},
           Scatter{"plant", 60, -34.0F, 268.0F, 60.0F, 36.0F, 1.0F},
           Scatter{"plant", 60, 128.0F, 264.0F, 60.0F, 36.0F, 1.0F},
           Scatter{"plant", 50, -276.0F, 116.0F, 34.0F, 46.0F, 1.0F},
           Scatter{"plant", 50, 268.0F, 120.0F, 34.0F, 46.0F, 1.0F},
           Scatter{"plant", 50, -252.0F, -60.0F, 40.0F, 56.0F, 1.0F},
           Scatter{"plant", 50, 250.0F, -60.0F, 40.0F, 56.0F, 1.0F},
           Scatter{"plant", 46, -110.0F, -228.0F, 54.0F, 34.0F, 1.0F},
           Scatter{"plant", 46, 112.0F, -230.0F, 54.0F, 34.0F, 1.0F},
           Scatter{"plant", 40, -60.0F, k_mountain_z + 56.0F, 44.0F, 26.0F, 1.0F},
           Scatter{"plant", 40, 62.0F, k_mountain_z + 56.0F, 44.0F, 26.0F, 1.0F},
           Scatter{"olive_tree", 22, -196.0F, 140.0F, 30.0F, 20.0F, 1.2F},
           Scatter{"olive_tree", 22, 180.0F, 146.0F, 30.0F, 20.0F, 1.2F},
           Scatter{"olive_tree", 20, -66.0F, 196.0F, 28.0F, 16.0F, 1.2F},
           Scatter{"olive_tree", 20, 68.0F, 200.0F, 28.0F, 16.0F, 1.2F},
           Scatter{"pine_tree", 24, -288.0F, 40.0F, 26.0F, 40.0F, 1.35F},
           Scatter{"pine_tree", 24, 284.0F, 44.0F, 26.0F, 40.0F, 1.35F},

           Scatter{"tent", 22, -212.0F, -160.0F, 26.0F, 18.0F, 0.95F},
           Scatter{"tent", 22, 216.0F, -156.0F, 26.0F, 18.0F, 0.95F},
           Scatter{"tent", 20, -196.0F, 150.0F, 24.0F, 16.0F, 0.95F},
           Scatter{"tent", 20, 190.0F, 154.0F, 24.0F, 16.0F, 0.95F},
           Scatter{"tent", 18, -46.0F, -206.0F, 24.0F, 14.0F, 0.95F},
           Scatter{"tent", 18, 52.0F, -208.0F, 24.0F, 14.0F, 0.95F},
           Scatter{"tent", 16, 168.0F, -262.0F, 22.0F, 16.0F, 0.95F},
           Scatter{"tent", 16, -242.0F, -258.0F, 22.0F, 16.0F, 0.95F},
           Scatter{"fire_camp", 10, -210.0F, -158.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"fire_camp", 10, 214.0F, -154.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"fire_camp", 9, -194.0F, 152.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 9, 188.0F, 156.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 8, 170.0F, -260.0F, 20.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 8, -240.0F, -256.0F, 20.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 8, -50.0F, -204.0F, 22.0F, 12.0F, 1.0F},
           Scatter{"fire_camp", 8, 56.0F, -206.0F, 22.0F, 12.0F, 1.0F},
           Scatter{"weapon_rack", 14, -206.0F, -166.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"weapon_rack", 14, 210.0F, -162.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"weapon_rack", 12, 120.0F, -128.0F, 20.0F, 16.0F, 1.0F},
           Scatter{"weapon_rack", 10, -190.0F, 146.0F, 20.0F, 12.0F, 1.0F},
           Scatter{"supply_cart", 14, -180.0F, -92.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 14, 184.0F, -88.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 12, 164.0F, -244.0F, 22.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 12, -120.0F, 180.0F, 24.0F, 16.0F, 1.0F},

           Scatter{"pine_tree", 16, -14.0F, -128.0F, 3.4F, 30.0F, 1.15F},
           Scatter{"pine_tree", 16, 14.0F, -128.0F, 3.4F, 30.0F, 1.15F},
           Scatter{"pine_tree", 14, -14.0F, -232.0F, 3.2F, 26.0F, 1.2F},
           Scatter{"pine_tree", 14, 14.0F, -232.0F, 3.2F, 26.0F, 1.2F},
           Scatter{"pine_tree", 12, -40.0F, -288.0F, 4.0F, 18.0F, 1.25F},
           Scatter{"pine_tree", 12, 40.0F, -288.0F, 4.0F, 18.0F, 1.25F},
           Scatter{"pine_tree", 10, -70.0F, -96.0F, 4.0F, 16.0F, 1.1F},
           Scatter{"pine_tree", 10, 70.0F, -96.0F, 4.0F, 16.0F, 1.1F},

           Scatter{"olive_tree", 18, -62.0F, -110.0F, 16.0F, 12.0F, 1.05F},
           Scatter{"olive_tree", 18, 62.0F, -110.0F, 16.0F, 12.0F, 1.05F},
           Scatter{"olive_tree", 16, -44.0F, -40.0F, 13.0F, 11.0F, 1.0F},
           Scatter{"olive_tree", 16, 44.0F, -40.0F, 13.0F, 11.0F, 1.0F},
           Scatter{"olive_tree", 14, -96.0F, 28.0F, 14.0F, 12.0F, 1.0F},
           Scatter{"olive_tree", 14, 96.0F, 28.0F, 14.0F, 12.0F, 1.0F},
           Scatter{"olive_tree", 14, -128.0F, -46.0F, 14.0F, 12.0F, 1.0F},
           Scatter{"olive_tree", 14, 128.0F, -46.0F, 14.0F, 12.0F, 1.0F},
           Scatter{"olive_tree", 12, -66.0F, 84.0F, 13.0F, 11.0F, 1.0F},
           Scatter{"olive_tree", 12, 66.0F, 84.0F, 13.0F, 11.0F, 1.0F},

           Scatter{"plant", 30, -58.0F, -104.0F, 17.0F, 13.0F, 1.0F},
           Scatter{"plant", 30, 58.0F, -104.0F, 17.0F, 13.0F, 1.0F},
           Scatter{"plant", 26, -40.0F, -34.0F, 14.0F, 11.0F, 0.95F},
           Scatter{"plant", 26, 40.0F, -34.0F, 14.0F, 11.0F, 0.95F},
           Scatter{"plant", 24, -100.0F, 34.0F, 15.0F, 12.0F, 0.95F},
           Scatter{"plant", 24, 100.0F, 34.0F, 15.0F, 12.0F, 0.95F},
           Scatter{"plant", 22, -20.0F, -270.0F, 12.0F, 14.0F, 1.0F},
           Scatter{"plant", 22, 20.0F, -270.0F, 12.0F, 14.0F, 1.0F},

           Scatter{"plant", 12, -74.0F, -50.0F, 5.0F, 4.5F, 0.95F},
           Scatter{"plant", 12, 76.0F, -46.0F, 5.0F, 4.5F, 0.95F},
           Scatter{"plant", 11, -112.0F, -14.0F, 4.6F, 4.2F, 0.95F},
           Scatter{"plant", 11, 114.0F, -18.0F, 4.6F, 4.2F, 0.95F},
           Scatter{"plant", 12, -56.0F, 44.0F, 5.2F, 4.6F, 0.95F},
           Scatter{"plant", 12, 58.0F, 40.0F, 5.2F, 4.6F, 0.95F},
           Scatter{"plant", 10, -146.0F, -74.0F, 4.4F, 4.0F, 0.95F},
           Scatter{"plant", 10, 148.0F, -70.0F, 4.4F, 4.0F, 0.95F},
           Scatter{"plant", 11, -92.0F, 92.0F, 4.8F, 4.4F, 0.95F},
           Scatter{"plant", 11, 94.0F, 88.0F, 4.8F, 4.4F, 0.95F},
           Scatter{"plant", 10, -34.0F, -142.0F, 4.6F, 4.2F, 0.95F},
           Scatter{"plant", 10, 36.0F, -138.0F, 4.6F, 4.2F, 0.95F},

           Scatter{"olive_tree", 7, -78.0F, -22.0F, 8.0F, 7.0F, 1.0F},
           Scatter{"olive_tree", 7, 80.0F, -26.0F, 8.0F, 7.0F, 1.0F},
           Scatter{"olive_tree", 8, -120.0F, 62.0F, 9.0F, 7.5F, 1.0F},
           Scatter{"olive_tree", 8, 122.0F, 58.0F, 9.0F, 7.5F, 1.0F},
           Scatter{"olive_tree", 7, -60.0F, -132.0F, 8.0F, 7.0F, 1.0F},
           Scatter{"olive_tree", 7, 62.0F, -128.0F, 8.0F, 7.0F, 1.0F},
           Scatter{"pine_tree", 6, -104.0F, -58.0F, 7.0F, 6.0F, 1.05F},
           Scatter{"pine_tree", 6, 106.0F, -54.0F, 7.0F, 6.0F, 1.05F},
           Scatter{"pine_tree", 6, -30.0F, 74.0F, 7.0F, 6.0F, 1.05F},
           Scatter{"pine_tree", 6, 32.0F, 70.0F, 7.0F, 6.0F, 1.05F},
       }) {
    grove(scenario,
          item.type,
          item.count,
          {item.x, 0.0F, item.z},
          item.radius_x,
          item.radius_z,
          item.scale,
          seed);
    seed += 13;
  }
}

struct Patrol {
  const char* name;
  Troop troop;
  float x;
  float z;
  std::vector<QVector3D> route;
};

[[nodiscard]] auto city_patrols() -> std::vector<Patrol> {
  return {
      {"capital_patrol_avenue",
       Troop::Spearman,
       0.0F,
       100.0F,
       {{0.0F, 0.0F, 40.0F},
        {0.0F, 0.0F, -40.0F},
        {0.0F, 0.0F, -130.0F},
        {0.0F, 0.0F, -40.0F},
        {0.0F, 0.0F, 60.0F}}},
      {"capital_patrol_decumanus",
       Troop::Swordsman,
       -150.0F,
       -60.0F,
       {{-60.0F, 0.0F, -60.0F},
        {60.0F, 0.0F, -60.0F},
        {150.0F, 0.0F, -60.0F},
        {40.0F, 0.0F, -60.0F},
        {-150.0F, 0.0F, -60.0F}}},
      {"capital_patrol_ring",
       Troop::Spearman,
       -150.0F,
       60.0F,
       {{-40.0F, 0.0F, 60.0F},
        {90.0F, 0.0F, 60.0F},
        {150.0F, 0.0F, 60.0F},
        {20.0F, 0.0F, 60.0F},
        {-150.0F, 0.0F, 60.0F}}},
      {"capital_patrol_north",
       Troop::Archer,
       -90.0F,
       -140.0F,
       {{20.0F, 0.0F, -140.0F},
        {150.0F, 0.0F, -140.0F},
        {60.0F, 0.0F, -140.0F},
        {-90.0F, 0.0F, -140.0F}}},
      {"capital_patrol_cardo_west",
       Troop::Swordsman,
       -96.0F,
       110.0F,
       {{-96.0F, 0.0F, 0.0F},
        {-96.0F, 0.0F, -150.0F},
        {-96.0F, 0.0F, -20.0F},
        {-96.0F, 0.0F, 110.0F}}},
      {"capital_patrol_cardo_east",
       Troop::Spearman,
       96.0F,
       -180.0F,
       {{96.0F, 0.0F, -40.0F},
        {96.0F, 0.0F, 100.0F},
        {96.0F, 0.0F, -30.0F},
        {96.0F, 0.0F, -180.0F}}},
      {"capital_patrol_wall_south",
       Troop::Archer,
       -150.0F,
       108.0F,
       {{0.0F, 0.0F, 108.0F},
        {150.0F, 0.0F, 108.0F},
        {0.0F, 0.0F, 108.0F},
        {-150.0F, 0.0F, 108.0F}}},
      {"capital_patrol_docks",
       Troop::Swordsman,
       120.0F,
       134.0F,
       {{-60.0F, 0.0F, 134.0F},
        {-118.0F, 0.0F, 150.0F},
        {-40.0F, 0.0F, 134.0F},
        {120.0F, 0.0F, 134.0F}}},
      {"capital_patrol_sacred",
       Troop::Spearman,
       0.0F,
       -200.0F,
       {{0.0F, 0.0F, -238.0F},
        {0.0F, 0.0F, -200.0F},
        {0.0F, 0.0F, -160.0F},
        {0.0F, 0.0F, -230.0F}}},
      {"capital_patrol_old_west",
       Troop::Swordsman,
       -92.0F,
       -60.0F,
       {{-40.0F, 0.0F, -60.0F},
        {-92.0F, 0.0F, -160.0F},
        {-40.0F, 0.0F, -100.0F},
        {-92.0F, 0.0F, -60.0F}}},
  };
}

void add_people(ArenaScenarioDefinition& scenario) {
  struct Crowd {
    const char* name;
    int count;
    float x;
    float z;
    float roam;
  };
  for (auto const& crowd :
       {Crowd{"capital_forum_crowd", 18, -8.0F, k_forum_z, 30.0F},
        Crowd{"capital_market_crowd", 14, -44.0F, -20.0F, 24.0F},
        Crowd{"capital_core_folk", 14, 44.0F, -96.0F, 26.0F},
        Crowd{"capital_north_quarter_folk", 12, -130.0F, -110.0F, 26.0F},
        Crowd{"capital_west_quarter_folk", 12, -134.0F, -22.0F, 26.0F},
        Crowd{"capital_east_quarter_folk", 12, 136.0F, -18.0F, 26.0F},
        Crowd{"capital_south_quarter_folk", 16, -70.0F, 56.0F, 30.0F},
        Crowd{"capital_circus_crowd", 16, k_circus_x, k_circus_z, 24.0F},
        Crowd{"capital_theatre_crowd", 12, -124.0F, 46.0F, 20.0F},
        Crowd{"capital_dock_hands", 16, 40.0F, 130.0F, 22.0F},
        Crowd{"capital_temple_pilgrims", 14, -6.0F, -262.0F, 20.0F},
        Crowd{"capital_farm_folk", 18, -120.0F, 232.0F, 26.0F},
        Crowd{"capital_farm_folk_east", 16, 140.0F, 244.0F, 26.0F},

        Crowd{"capital_gate_throng", 22, -6.0F, 118.0F, 22.0F},
        Crowd{"capital_gate_throng_west", 16, -34.0F, 108.0F, 18.0F},
        Crowd{"capital_gate_throng_east", 16, 32.0F, 110.0F, 18.0F},
        Crowd{"capital_avenue_folk_s", 20, 4.0F, 82.0F, 20.0F},
        Crowd{"capital_avenue_folk_m", 20, -4.0F, 44.0F, 20.0F},
        Crowd{"capital_avenue_folk_c", 20, 6.0F, 6.0F, 20.0F},
        Crowd{"capital_avenue_folk_n", 18, -6.0F, -46.0F, 20.0F},
        Crowd{"capital_sacred_way_folk", 18, 4.0F, -150.0F, 20.0F},
        Crowd{"capital_sacred_way_upper", 16, -4.0F, -196.0F, 18.0F},
        Crowd{"capital_upper_folk_west", 16, -52.0F, -96.0F, 20.0F},
        Crowd{"capital_upper_folk_east", 16, 54.0F, -98.0F, 20.0F},
        Crowd{"capital_upper_garden", 14, -30.0F, -118.0F, 16.0F},
        Crowd{"capital_upper_garden_east", 14, 30.0F, -120.0F, 16.0F},
        Crowd{"capital_insula_nw_folk", 16, -132.0F, -108.0F, 24.0F},
        Crowd{"capital_insula_ne_folk", 16, 134.0F, -104.0F, 24.0F},
        Crowd{"capital_insula_sw_folk", 16, -138.0F, 62.0F, 24.0F},
        Crowd{"capital_insula_se_folk", 16, 140.0F, 58.0F, 24.0F},
        Crowd{"capital_baths_folk", 14, -52.0F, -4.0F, 16.0F},
        Crowd{"capital_library_folk", 14, 52.0F, -4.0F, 16.0F},
        Crowd{"capital_mint_folk", 14, -26.0F, -12.0F, 14.0F},
        Crowd{"capital_granary_folk", 14, 26.0F, -12.0F, 14.0F},
        Crowd{"capital_circus_stands", 18, k_circus_x - 22.0F, k_circus_z, 20.0F},
        Crowd{"capital_dock_market", 16, -36.0F, 124.0F, 20.0F},
        Crowd{"capital_wall_west_folk", 14, -152.0F, -40.0F, 22.0F},
        Crowd{"capital_wall_east_folk", 14, 154.0F, -36.0F, 22.0F},
        Crowd{"capital_outskirt_south", 16, -78.0F, 156.0F, 26.0F},
        Crowd{"capital_outskirt_south_east", 16, 84.0F, 152.0F, 26.0F},
        Crowd{"capital_outskirt_north", 14, -96.0F, -190.0F, 24.0F},
        Crowd{"capital_outskirt_north_east", 14, 98.0F, -186.0F, 24.0F}}) {
    scenario.groups.push_back(residents(QString::fromLatin1(crowd.name),
                                        crowd.count,
                                        {crowd.x, 0.0F, crowd.z},
                                        {4.0F, 0.0F, 0.0F},
                                        crowd.roam));
  }

  struct Congregation {
    const char* name;
    float x;
    float z;
    int units;
    int per_unit;
  };
  float kneel_delay = 0.0F;
  for (auto const& row :
       {Congregation{"capital_temple_healers", -20.0F, k_mountain_z + 24.0F, 4, 6},
        Congregation{"capital_hospice_healers", 20.0F, k_mountain_z - 10.0F, 3, 6},
        Congregation{"capital_sanctum_choir_s", 0.0F, k_mountain_z + 34.0F, 4, 7},
        Congregation{"capital_sanctum_choir_n", 0.0F, k_mountain_z - 34.0F, 4, 7},
        Congregation{"capital_sanctum_choir_w", -34.0F, k_mountain_z, 4, 7},
        Congregation{"capital_sanctum_choir_e", 34.0F, k_mountain_z, 4, 7},
        Congregation{"capital_sanctum_choir_sw", -26.0F, k_mountain_z + 26.0F, 3, 6},
        Congregation{"capital_sanctum_choir_se", 26.0F, k_mountain_z + 26.0F, 3, 6},
        Congregation{"capital_sanctum_choir_nw", -26.0F, k_mountain_z - 26.0F, 3, 6},
        Congregation{"capital_sanctum_choir_ne", 26.0F, k_mountain_z - 26.0F, 3, 6},
        Congregation{"capital_oracle_choir", 0.0F, k_mountain_z - 46.0F, 3, 6}}) {
    auto congregation = group(QString::fromLatin1(row.name),
                              Troop::Healer,
                              1,
                              row.units,
                              {row.x, 0.0F, row.z},
                              row.per_unit,
                              {9.0F, 0.0F, 0.0F});
    congregation.showcase_routine = {QStringLiteral("rest_kneel:9.0:0.8")};
    congregation.showcase_loop = true;
    congregation.showcase_start_delay = kneel_delay;
    kneel_delay += 0.7F;
    scenario.groups.push_back(std::move(congregation));
  }
  scenario.groups.push_back(group(QStringLiteral("capital_temple_guard"),
                                  Troop::Spearman,
                                  1,
                                  2,
                                  {0.0F, 0.0F, -266.0F},
                                  10,
                                  {26.0F, 0.0F, 0.0F}));

  scenario.groups.push_back(group(QStringLiteral("capital_garrison_swords"),
                                  Troop::Swordsman,
                                  1,
                                  4,
                                  {126.0F, 0.0F, -138.0F},
                                  16,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_spears"),
                                  Troop::Spearman,
                                  1,
                                  4,
                                  {126.0F, 0.0F, -114.0F},
                                  16,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_archers"),
                                  Troop::Archer,
                                  1,
                                  3,
                                  {126.0F, 0.0F, -94.0F},
                                  12,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_horse"),
                                  Troop::HorseSpearman,
                                  1,
                                  2,
                                  {150.0F, 0.0F, -72.0F},
                                  8,
                                  {14.0F, 0.0F, 0.0F}));

  scenario.groups.push_back(group(QStringLiteral("capital_gate_watch"),
                                  Troop::Spearman,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, 108.0F},
                                  8,
                                  {26.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_north_watch"),
                                  Troop::Spearman,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, -142.0F},
                                  8,
                                  {26.0F, 0.0F, 0.0F}));

  scenario.groups.push_back(group(QStringLiteral("capital_column_swords"),
                                  Troop::Swordsman,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, 232.0F},
                                  16,
                                  {8.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_column_spears"),
                                  Troop::Spearman,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, 248.0F},
                                  16,
                                  {8.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_column_archers"),
                                  Troop::Archer,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, 264.0F},
                                  12,
                                  {8.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_column_horse"),
                                  Troop::HorseSpearman,
                                  1,
                                  2,
                                  {k_avenue_x, 0.0F, 216.0F},
                                  8,
                                  {11.0F, 0.0F, 0.0F}));

  scenario.groups.push_back(group(QStringLiteral("capital_masons"),
                                  Troop::Builder,
                                  1,
                                  2,
                                  {186.0F, 0.0F, -272.0F},
                                  4,
                                  {5.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_quarry_crew"),
                                  Troop::Builder,
                                  1,
                                  2,
                                  {-232.0F, 0.0F, -252.0F},
                                  4,
                                  {5.0F, 0.0F, 0.0F}));

  for (auto const& patrol : city_patrols()) {
    auto walker = group(QString::fromLatin1(patrol.name),
                        patrol.troop,
                        1,
                        1,
                        {patrol.x, 0.0F, patrol.z},
                        1,
                        {2.0F, 0.0F, 0.0F});
    scenario.groups.push_back(walker);
  }

  struct Crew {
    const char* name;
    Troop troop;
    float x;
    float z;
    int count;
    bool ai;
    int crew;
  };
  for (auto const& crew :
       {Crew{"capital_foresters_west", Troop::Builder, -206.0F, -100.0F, 3, false, 8},
        Crew{"capital_foresters_east", Troop::Builder, 206.0F, -92.0F, 3, false, 8},
        Crew{"capital_foresters_south", Troop::Builder, -178.0F, 216.0F, 3, false, 8},
        Crew{"capital_quarriers", Troop::Builder, 176.0F, -262.0F, 3, false, 8},
        Crew{"capital_quarriers_west", Troop::Builder, -238.0F, -256.0F, 3, false, 8},
        Crew{"capital_miners", Troop::Builder, 202.0F, -280.0F, 3, false, 8},
        Crew{"capital_reapers_west", Troop::Builder, -200.0F, 208.0F, 3, false, 8},
        Crew{"capital_reapers_east", Troop::Builder, 132.0F, 210.0F, 3, false, 8},
        Crew{"capital_shepherds", Troop::Builder, -246.0F, 262.0F, 2, false, 6},
        Crew{"capital_wardens", Troop::Builder, 96.0F, 244.0F, 2, false, 6},
        Crew{"capital_masons_forum", Troop::Builder, -44.0F, -70.0F, 4, false, 10},
        Crew{"capital_masons_upper", Troop::Builder, 36.0F, -100.0F, 4, false, 10},
        Crew{"capital_masons_docks", Troop::Builder, 8.0F, 126.0F, 4, false, 10},
        Crew{"capital_masons_wall", Troop::Builder, -140.0F, -136.0F, 3, false, 10},
        Crew{"capital_masons_circus", Troop::Builder, 92.0F, 34.0F, 3, false, 10},
        Crew{"capital_repair_crew", Troop::Builder, -110.0F, -20.0F, 3, false, 8},
        Crew{
            "capital_repair_crew_east", Troop::Builder, 118.0F, -24.0F, 3, false, 8}}) {
    scenario.groups.push_back(worker(QString::fromLatin1(crew.name),
                                     crew.troop,
                                     {crew.x, 0.0F, crew.z},
                                     crew.count,
                                     crew.ai,
                                     crew.crew));
  }

  for (auto const& carriers :
       {Crew{"capital_carriers_forum", Troop::Civilian, -62.0F, -60.0F, 4, false, 9},
        Crew{"capital_carriers_docks", Troop::Civilian, 20.0F, 118.0F, 4, false, 10},
        Crew{"capital_carriers_barracks", Troop::Civilian, 104.0F, -70.0F, 4, false, 9},
        Crew{"capital_carriers_upper", Troop::Civilian, -30.0F, -90.0F, 4, false, 9},
        Crew{"capital_carriers_farm", Troop::Builder, -144.0F, 200.0F, 4, false, 9},
        Crew{"capital_carriers_quarry", Troop::Builder, 150.0F, -234.0F, 4, false, 9},
        Crew{"capital_carriers_timber", Troop::Builder, -172.0F, -86.0F, 4, false, 9},
        Crew{"capital_carriers_stone", Troop::Builder, 158.0F, -238.0F, 4, false, 9},
        Crew{"capital_carriers_iron", Troop::Builder, 186.0F, -262.0F, 4, false, 9},
        Crew{"capital_carriers_grain", Troop::Builder, -110.0F, 176.0F, 4, false, 9}}) {
    scenario.groups.push_back(worker(QString::fromLatin1(carriers.name),
                                     carriers.troop,
                                     {carriers.x, 0.0F, carriers.z},
                                     carriers.count,
                                     false,
                                     carriers.crew));
  }

  for (auto const& healers :
       {Crew{"capital_upper_healers_west", Troop::Healer, -52.0F, -96.0F, 3, false, 5},
        Crew{"capital_upper_healers_east", Troop::Healer, 52.0F, -96.0F, 3, false, 5},
        Crew{"capital_upper_healers_court", Troop::Healer, 0.0F, -118.0F, 3, false, 5},
        Crew{"capital_upper_healers_walk", Troop::Healer, -22.0F, -76.0F, 2, false, 5},
        Crew{"capital_forum_healers", Troop::Healer, 34.0F, -70.0F, 2, false, 5}}) {
    auto ward = worker(QString::fromLatin1(healers.name),
                       healers.troop,
                       {healers.x, 0.0F, healers.z},
                       healers.count,
                       false,
                       healers.crew);
    ward.settlement_resident = true;
    ward.settlement_roam_radius = 14.0F;
    scenario.groups.push_back(std::move(ward));
  }
}

[[nodiscard]] auto has_group(const ArenaScenarioDefinition& scenario,
                             const QString& name) -> bool {
  return std::any_of(scenario.groups.begin(),
                     scenario.groups.end(),
                     [&](const auto& item) { return item.name == name; });
}

void add_script(ArenaScenarioDefinition& scenario) {
  scenario.steps = {
      at(0.4F, Command::Hold, QStringLiteral("capital_gate_watch")),
      at(0.4F, Command::Hold, QStringLiteral("capital_north_watch")),
      at(0.4F, Command::Hold, QStringLiteral("capital_temple_guard")),
      at(0.4F, Command::Hold, QStringLiteral("capital_garrison_archers")),
      march_to(1.0F, QStringLiteral("capital_column_horse"), {0.0F, 0.0F, -20.0F}),
      march_to(2.0F, QStringLiteral("capital_column_swords"), {0.0F, 0.0F, 20.0F}),
      march_to(14.0F, QStringLiteral("capital_column_spears"), {0.0F, 0.0F, 50.0F}),
      march_to(28.0F, QStringLiteral("capital_column_archers"), {0.0F, 0.0F, 80.0F}),
      march_to(70.0F, QStringLiteral("capital_column_horse"), {0.0F, 0.0F, -150.0F}),
      march_to(84.0F, QStringLiteral("capital_column_swords"), {0.0F, 0.0F, -110.0F}),
      march_to(96.0F, QStringLiteral("capital_column_spears"), {0.0F, 0.0F, -70.0F}),
      march_to(110.0F, QStringLiteral("capital_column_archers"), {0.0F, 0.0F, -40.0F}),
      march_to(
          20.0F, QStringLiteral("capital_garrison_swords"), {112.0F, 0.0F, -100.0F}),
      march_to(
          90.0F, QStringLiteral("capital_garrison_swords"), {126.0F, 0.0F, -168.0F}),
  };

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

  const auto patrols = city_patrols();
  for (const auto& patrol : patrols) {
    if (patrol.route.empty()) {
      continue;
    }
    float when = 1.5F;
    std::size_t leg = 0;
    while (when < 180.0F) {
      scenario.steps.push_back(march_to(when,
                                        QString::fromLatin1(patrol.name),
                                        patrol.route[leg % patrol.route.size()]));
      when += 16.0F;
      ++leg;
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

  add_roads(s);
  add_water(s);
  add_relief(s);
  add_walls(s);

  CityPlanner planner(s);
  planner.note_existing();
  add_monuments(s, planner);
  add_districts(planner);

  add_dressing(s);
  add_people(s);
  add_script(s);
  for (const auto& item : s.groups) {
    if (item.spawn_type.has_value() && *item.spawn_type == Spawn::Farm) {
      s.steps.push_back(set_value(0.5F, Command::SetFarmGrowth, item.name, 100));
    }
  }

  s.wildlife = Game::Wildlife::default_settings();
  s.wildlife.enabled = true;
  s.wildlife.seed = 20260819U;
  s.wildlife.near_simulation_radius = 200.0F;
  s.wildlife.far_simulation_radius = 380.0F;
  s.wildlife.sheep.enabled = true;
  s.wildlife.sheep.group_count = 6;
  s.wildlife.sheep.group_size_min = 8;
  s.wildlife.sheep.group_size_max = 11;
  s.wildlife.sheep.roam_radius = 9.0F;
  s.wildlife.sheep.group_count = 25;
  s.wildlife.sheep.group_size_min = 9;
  s.wildlife.sheep.group_size_max = 14;
  s.wildlife.sheep.spawn_areas = {
      {-60.0F, 206.0F, 13.0F},  {6.0F, 212.0F, 13.0F},    {-108.0F, 214.0F, 12.0F},
      {-238.0F, 172.0F, 16.0F}, {-166.0F, 178.0F, 16.0F}, {-96.0F, 186.0F, 16.0F},
      {-24.0F, 180.0F, 16.0F},  {48.0F, 178.0F, 16.0F},   {124.0F, 174.0F, 16.0F},
      {196.0F, 180.0F, 16.0F},  {-282.0F, 214.0F, 16.0F}, {-140.0F, 250.0F, 16.0F},
      {-58.0F, 246.0F, 16.0F},  {36.0F, 252.0F, 16.0F},   {156.0F, 248.0F, 16.0F},
      {248.0F, 246.0F, 16.0F},  {-214.0F, 296.0F, 16.0F}, {-100.0F, 300.0F, 16.0F},
      {14.0F, 302.0F, 16.0F},   {132.0F, 298.0F, 16.0F},  {240.0F, 300.0F, 16.0F},
      {-286.0F, 128.0F, 16.0F}, {268.0F, 132.0F, 16.0F},  {-296.0F, -128.0F, 16.0F},
      {272.0F, -200.0F, 16.0F}};
  s.wildlife.wolves.enabled = false;
  s.wildlife.wolves.group_count = 0;
  s.wildlife.birds.enabled = true;
  s.wildlife.birds.group_count = 6;
  s.wildlife.birds.group_size_min = 12;
  s.wildlife.birds.group_size_max = 16;
  s.wildlife.birds.flight_height = 30.0F;
  s.wildlife.birds.roam_radius = 40.0F;
  s.wildlife.birds.spawn_areas = {{0.0F, -240.0F, 20.0F},
                                  {-110.0F, -60.0F, 20.0F},
                                  {110.0F, -60.0F, 20.0F},
                                  {0.0F, 60.0F, 20.0F},
                                  {-120.0F, 180.0F, 20.0F},
                                  {130.0F, 190.0F, 20.0F}};

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
