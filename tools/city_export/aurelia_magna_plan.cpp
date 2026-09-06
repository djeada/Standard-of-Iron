#include "aurelia_magna_plan.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

#include "game/map/terrain.h"
#include "game/systems/building_collision_registry.h"
#include "game/units/spawn_type.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Troop = Game::Units::TroopType;

[[nodiscard]] auto plot_hash(int a, int b, int c) -> unsigned {

  unsigned value = (static_cast<unsigned>(a) * 73856093U) ^
                   (static_cast<unsigned>(b) * 19349663U) ^
                   (static_cast<unsigned>(c) * 83492791U);
  value ^= value >> 13U;
  value *= 2246822519U;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] auto plot_unit(int a, int b, int c) -> float {
  return static_cast<float>(plot_hash(a, b, c) % 1024U) / 1023.0F;
}

constexpr int k_grid_extent = 768;
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
constexpr float k_forum_z = -90.0F;
constexpr float k_cardo_west_x = -96.0F;
constexpr float k_cardo_east_x = 96.0F;
constexpr float k_ring_south_z = 60.0F;
constexpr float k_ring_outer_z = 100.0F;
constexpr float k_militaris_z = -124.0F;
constexpr float k_north_lane_z = -138.0F;
constexpr float k_upper_south = -64.0F;

constexpr float k_river_z = 176.0F;
constexpr float k_river_width = 26.0F;
constexpr float k_main_bridge_width = 16.0F;
constexpr float k_second_bridge_width = 12.0F;

constexpr float k_bridge_reach =
    Game::Map::river_bank_standing_half_width(k_river_width) +
    Game::Map::bridge_bank_landing(k_main_bridge_width, k_river_width);
constexpr float k_bridge_north_z = k_river_z - k_bridge_reach;
constexpr float k_bridge_south_z = k_river_z + k_bridge_reach;

constexpr float k_quay_z = 148.0F;
constexpr float k_dock_row_z = 141.0F;
constexpr float k_vicus_lane_z = 133.0F;
constexpr float k_vicus_row_z = 127.2F;

constexpr float k_circus_x = 133.0F;
constexpr float k_circus_z = 32.0F;
constexpr float k_theatre_x = -138.0F;
constexpr float k_theatre_z = 90.0F;

constexpr float k_street_width = 3.6F;
constexpr float k_frontage_gap = 0.8F;
constexpr float k_setback = 1.2F;
constexpr float k_wall_clear = 5.0F;

constexpr float k_campaign_mountain_scale = 1.9F;

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

void orchard(ArenaScenarioDefinition& scenario,
             const char* prop_type,
             QVector3D origin,
             int columns,
             int rows,
             float pitch,
             float rotation_deg,
             float scale,
             int seed) {
  constexpr float k_deg_to_rad = 0.01745329251994329577F;
  const float cos_a = std::cos(rotation_deg * k_deg_to_rad);
  const float sin_a = std::sin(rotation_deg * k_deg_to_rad);
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      if (plot_unit(seed, column, row) > 0.93F) {
        continue;
      }
      const float local_x =
          (static_cast<float>(column) - ((columns - 1) * 0.5F)) * pitch +
          ((plot_unit(seed + 1, column, row) - 0.5F) * pitch * 0.18F);
      const float local_z = (static_cast<float>(row) - ((rows - 1) * 0.5F)) * pitch +
                            ((plot_unit(seed + 2, column, row) - 0.5F) * pitch * 0.18F);
      scenario.resource_patches.push_back(
          {QString::fromLatin1(prop_type),
           1,
           {origin.x() + (local_x * cos_a) - (local_z * sin_a),
            0.0F,
            origin.z() + (local_x * sin_a) + (local_z * cos_a)},
           {},
           scale * (0.9F + (plot_unit(seed + 3, column, row) * 0.2F))});
    }
  }
}

auto worker(QString name, Troop troop, QVector3D at, int count, bool ai, int crew = 1)
    -> ArenaScenarioGroup {
  auto result = group(std::move(name), troop, 1, count, at, crew, {5.4F, 0.0F, 0.0F});
  result.ai_controlled = ai;
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

constexpr float k_default_gap =
    (2.0F * Game::Systems::k_default_building_grid_padding) + 0.9F;

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

  auto place(const QString& name,
             Spawn type,
             QVector3D at,
             float facing,
             bool nudge,
             float gap = k_default_gap) -> bool {
    const QVector2D half = rotated_half(footprint_half(type), facing);
    QVector3D chosen = at;
    if (!fits(chosen, half, gap)) {
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
          if (fits(candidate, half, gap)) {
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

  [[nodiscard]] auto scenario() -> ArenaScenarioDefinition& { return m_scenario; }

  void pave(QVector3D centre, float width, float depth) {

    constexpr float k_strip = 7.0F;
    constexpr float k_overlap = 1.2F;
    constexpr float k_shortest = 3.0F;
    const bool along_x = width >= depth;
    const float across = along_x ? depth : width;
    const float along = along_x ? width : depth;
    const int strips =
        std::max(1, static_cast<int>(std::ceil(across / (k_strip - k_overlap))));
    const float pitch =
        strips > 1 ? (across - k_strip) / static_cast<float>(strips - 1) : 0.0F;
    const float along_centre = along_x ? centre.x() : centre.z();
    const float across_centre = along_x ? centre.z() : centre.x();

    for (int index = 0; index < strips; ++index) {
      const float offset = across_centre + (across * -0.5F) + (k_strip * 0.5F) +
                           (pitch * static_cast<float>(index));

      std::vector<std::pair<float, float>> cuts;
      for (const auto& taken : m_taken) {
        const float taken_along = along_x ? taken.x : taken.z;
        const float taken_across = along_x ? taken.z : taken.x;
        const float forbidden = (k_strip * 0.5F) + std::min(taken.half_w, taken.half_d);
        const float distance = std::abs(taken_across - offset);
        if (distance >= forbidden) {
          continue;
        }
        const float reach =
            std::sqrt((forbidden * forbidden) - (distance * distance)) + 0.3F;
        cuts.emplace_back(taken_along - reach, taken_along + reach);
      }
      std::sort(cuts.begin(), cuts.end());

      float cursor = along_centre - (along * 0.5F);
      const float last = along_centre + (along * 0.5F);
      auto const lay_piece = [&](float from, float to) {
        if (to - from < k_shortest) {
          return;
        }
        if (along_x) {
          m_scenario.roads.push_back(
              street({from, 0.0F, offset}, {to, 0.0F, offset}, k_strip));
        } else {
          m_scenario.roads.push_back(
              street({offset, 0.0F, from}, {offset, 0.0F, to}, k_strip));
        }
      };
      for (const auto& cut : cuts) {
        if (cut.second <= cursor) {
          continue;
        }
        if (cut.first > cursor) {
          lay_piece(cursor, std::min(cut.first, last));
        }
        cursor = std::max(cursor, cut.second);
        if (cursor >= last) {
          break;
        }
      }
      if (cursor < last) {
        lay_piece(cursor, last);
      }
    }
  }

  [[nodiscard]] auto ground_height(float x, float z) const -> float {
    float height = 0.0F;
    for (const auto& patch : m_scenario.elevation_patches) {
      const float radius = std::max(patch.radius, 1.0F);
      const float plateau = std::clamp(patch.plateau, 0.0F, radius * 0.95F);
      const float taper = (radius - plateau) / radius;
      const float distance =
          std::hypot(x - patch.center.x(), z - patch.center.z()) / radius;
      const float feather = std::clamp((1.0F - distance) / taper, 0.0F, 1.0F);
      const float blend = feather * feather * (3.0F - (2.0F * feather));
      height = std::max(height, patch.height * blend);
    }
    return height;
  }

  [[nodiscard]] auto steep(float x, float z, float limit) const -> bool {
    constexpr float k_reach = 2.5F;
    const float dx = ground_height(x + k_reach, z) - ground_height(x - k_reach, z);
    const float dz = ground_height(x, z + k_reach) - ground_height(x, z - k_reach);
    return std::hypot(dx, dz) / (2.0F * k_reach) > limit;
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

  [[nodiscard]] auto fits(QVector3D at, QVector2D half, float gap) const -> bool {
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
      constexpr float k_slack = 0.02F;
      const bool overlap_x =
          std::abs(at.x() - taken.x) < half.x() + taken.half_w + gap - k_slack;
      const bool overlap_z =
          std::abs(at.z() - taken.z) < half.y() + taken.half_d + gap - k_slack;
      if (overlap_x && overlap_z) {
        return false;
      }
    }
    return true;
  }

  ArenaScenarioDefinition& m_scenario;
  std::vector<Taken> m_taken;
};

struct Rect {
  float x0;
  float z0;
  float x1;
  float z1;

  [[nodiscard]] auto width() const -> float { return x1 - x0; }
  [[nodiscard]] auto depth() const -> float { return z1 - z0; }
  [[nodiscard]] auto centre() const -> QVector3D {
    return {(x0 + x1) * 0.5F, 0.0F, (z0 + z1) * 0.5F};
  }
};

enum class BlockKind {
  Insula,
  Domus,
  Market,
  Garden,
  Precinct
};

struct RowSpec {
  QString prefix;
  QVector3D from;
  QVector3D to;
  float facing;
  float gap;
  float shop_chance;
  int seed;
  int skip_index;
};

auto lay_row(CityPlanner& planner, const RowSpec& spec) -> int {
  const QVector3D span = spec.to - spec.from;
  const float length = std::hypot(span.x(), span.z());
  if (length < 3.0F) {
    return 0;
  }
  const QVector3D dir = span / length;
  const bool along_x = std::abs(dir.x()) > 0.5F;

  struct Item {
    Spawn type;
    float extent;
    float depth_shift;
  };
  std::vector<Item> items;
  float used = 0.0F;
  for (int index = 0;; ++index) {
    const bool shop = plot_unit(spec.seed, index, 11) < spec.shop_chance;
    const Spawn type = shop ? Spawn::Marketplace : Spawn::Home;
    const QVector2D half = rotated_half(footprint_half(type), spec.facing);
    const float extent = along_x ? half.x() * 2.0F : half.y() * 2.0F;
    const float across = along_x ? half.y() : half.x();
    const float next = used + (items.empty() ? 0.0F : spec.gap) + extent;
    if (next > length + 0.01F) {
      break;
    }
    const QVector2D home_half = rotated_half(footprint_half(Spawn::Home), spec.facing);
    const float home_across = along_x ? home_half.y() : home_half.x();
    items.push_back({type, extent, across - home_across});
    used = next;
  }

  int placed = 0;
  float cursor = (length - used) * 0.5F;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const Item& item = items[index];
    const float centre = cursor + (item.extent * 0.5F);
    cursor += item.extent + spec.gap;
    if (static_cast<int>(index) == spec.skip_index) {
      continue;
    }

    const QVector3D inward =
        along_x
            ? QVector3D(0.0F,
                        0.0F,
                        (spec.facing < 90.0F || spec.facing > 270.0F) ? 1.0F : -1.0F)
            : QVector3D(spec.facing > 180.0F ? 1.0F : -1.0F, 0.0F, 0.0F);
    const QVector3D at = spec.from + (dir * centre) + (inward * item.depth_shift);
    if (planner.place(QStringLiteral("%1_%2").arg(spec.prefix).arg(placed),
                      item.type,
                      at,
                      spec.facing,
                      false,
                      spec.gap)) {
      ++placed;
    }
  }
  return placed;
}

void courtyard(ArenaScenarioDefinition& scenario,
               const Rect& rect,
               BlockKind kind,
               int seed) {
  const QVector3D centre = rect.centre();
  const float reach_x = std::max(rect.width() * 0.5F - 7.0F, 1.0F);
  const float reach_z = std::max(rect.depth() * 0.5F - 7.0F, 1.0F);
  switch (kind) {
  case BlockKind::Insula:
    grove(scenario, "plant", 2, centre, reach_x, reach_z, 0.9F, seed);
    break;
  case BlockKind::Domus:
    grove(scenario,
          "olive_tree",
          std::max(1, static_cast<int>((reach_x * reach_z) / 40.0F)),
          centre,
          reach_x,
          reach_z,
          0.95F,
          seed);
    grove(scenario, "plant", 3, centre, reach_x, reach_z, 0.9F, seed + 1);
    break;
  case BlockKind::Market:
    grove(scenario, "supply_cart", 3, centre, reach_x, reach_z, 0.9F, seed);
    if (rect.width() > 20.0F && rect.depth() > 20.0F) {
      scenario.resource_patches.push_back(patch("statue", 1, centre, {}, 1.1F));
    }
    break;
  case BlockKind::Garden:
  case BlockKind::Precinct:
    break;
  }
}

void fill_block(CityPlanner& planner,
                const QString& name,
                const Rect& rect,
                BlockKind kind,
                float front_facing,
                int seed) {
  ArenaScenarioDefinition& scenario = planner.scenario();
  if (rect.depth() < 9.0F || rect.width() < 9.0F) {
    if (rect.depth() < 4.4F || rect.width() < 4.4F) {
      return;
    }
    const QVector2D home = footprint_half(Spawn::Home);
    const float shop = kind == BlockKind::Market ? 1.0F : 0.12F;
    if (rect.width() >= rect.depth()) {
      const float z = front_facing == 0.0F ? rect.z0 + home.y() : rect.z1 - home.y();
      lay_row(planner,
              {name + QStringLiteral("_row"),
               {rect.x0, 0.0F, z},
               {rect.x1, 0.0F, z},
               front_facing == 0.0F ? 0.0F : 180.0F,
               k_frontage_gap,
               shop,
               seed + 1,
               -1});
    } else {
      const float x = front_facing == 270.0F ? rect.x0 + home.x() : rect.x1 - home.x();
      lay_row(planner,
              {name + QStringLiteral("_row"),
               {x, 0.0F, rect.z0},
               {x, 0.0F, rect.z1},
               front_facing == 270.0F ? 270.0F : 90.0F,
               k_frontage_gap,
               shop,
               seed + 1,
               -1});
    }
    return;
  }

  if (kind == BlockKind::Garden) {
    const int columns = std::max(1, static_cast<int>((rect.width() - 6.0F) / 6.5F));
    const int rows = std::max(1, static_cast<int>((rect.depth() - 6.0F) / 6.5F));
    orchard(
        scenario, "olive_tree", rect.centre(), columns, rows, 6.5F, 0.0F, 0.95F, seed);
    grove(scenario,
          "plant",
          4,
          rect.centre(),
          rect.width() * 0.4F,
          rect.depth() * 0.4F,
          0.9F,
          seed + 2);
    if (rect.width() > 18.0F && rect.depth() > 18.0F) {
      scenario.resource_patches.push_back(patch("statue", 1, rect.centre(), {}, 1.2F));
    }
    return;
  }

  if (kind == BlockKind::Precinct) {
    const QVector3D centre = rect.centre();
    if (planner.place(name + QStringLiteral("_temple"),
                      Spawn::Temple,
                      centre,
                      front_facing,
                      true)) {
      const QVector3D front = front_facing == 0.0F     ? QVector3D(0.0F, 0.0F, -1.0F)
                              : front_facing == 90.0F  ? QVector3D(1.0F, 0.0F, 0.0F)
                              : front_facing == 180.0F ? QVector3D(0.0F, 0.0F, 1.0F)
                                                       : QVector3D(-1.0F, 0.0F, 0.0F);
      const QVector3D side(front.z(), 0.0F, front.x());
      scenario.resource_patches.push_back(
          patch("statue", 1, centre + (front * 8.5F) + (side * 4.0F), {}, 1.2F));
      scenario.resource_patches.push_back(
          patch("statue", 1, centre + (front * 8.5F) - (side * 4.0F), {}, 1.2F));
    }
    return;
  }

  const float gap = kind == BlockKind::Insula  ? k_frontage_gap
                    : kind == BlockKind::Domus ? 2.2F
                                               : 1.0F;
  const float shop = kind == BlockKind::Insula  ? 0.12F
                     : kind == BlockKind::Domus ? 0.0F
                                                : 1.0F;
  const QVector2D home = footprint_half(Spawn::Home);
  const float depth_ns = home.y();
  const float depth_ew = home.x();

  const int south_gate = kind == BlockKind::Insula ? 2 : -1;
  lay_row(planner,
          {name + QStringLiteral("_n"),
           {rect.x0, 0.0F, rect.z0 + depth_ns},
           {rect.x1, 0.0F, rect.z0 + depth_ns},
           0.0F,
           gap,
           shop,
           seed + 1,
           -1});
  lay_row(planner,
          {name + QStringLiteral("_s"),
           {rect.x0, 0.0F, rect.z1 - depth_ns},
           {rect.x1, 0.0F, rect.z1 - depth_ns},
           180.0F,
           gap,
           shop,
           seed + 2,
           south_gate});
  const float side_z0 = rect.z0 + (depth_ns * 2.0F) + gap;
  const float side_z1 = rect.z1 - (depth_ns * 2.0F) - gap;
  lay_row(planner,
          {name + QStringLiteral("_e"),
           {rect.x1 - depth_ew, 0.0F, side_z0},
           {rect.x1 - depth_ew, 0.0F, side_z1},
           90.0F,
           gap,
           shop,
           seed + 3,
           -1});
  lay_row(planner,
          {name + QStringLiteral("_w"),
           {rect.x0 + depth_ew, 0.0F, side_z0},
           {rect.x0 + depth_ew, 0.0F, side_z1},
           270.0F,
           gap,
           shop,
           seed + 4,
           -1});

  const float inner_depth = side_z1 - side_z0;
  const float inner_width = rect.width() - (depth_ew * 4.0F) - (2.0F * gap);
  if (kind == BlockKind::Insula && inner_depth >= 13.0F && inner_width >= 14.0F) {
    const float mid_z = (rect.z0 + rect.z1) * 0.5F;
    lay_row(planner,
            {name + QStringLiteral("_m"),
             {rect.x0 + (depth_ew * 2.0F) + gap + 1.5F, 0.0F, mid_z},
             {rect.x1 - (depth_ew * 2.0F) - gap - 1.5F, 0.0F, mid_z},
             plot_unit(seed, 7, 7) > 0.5F ? 0.0F : 180.0F,
             gap,
             0.0F,
             seed + 5,
             -1});
    return;
  }
  courtyard(scenario, rect, kind, seed + 6);
}

struct Line {
  float at;
  float width = k_street_width;
  bool existing = false;
};

struct Quarter {
  const char* name;
  Rect bounds;
  float inset_n;
  float inset_e;
  float inset_s;
  float inset_w;
  std::vector<Line> x_lines;
  std::vector<Line> z_lines;
  float insula;
  float domus;
  float market;
  float garden;
  float thin_facing = 180.0F;
};

[[nodiscard]] auto pick_kind(const Quarter& quarter, float roll) -> BlockKind {
  float edge = quarter.insula;
  if (roll < edge) {
    return BlockKind::Insula;
  }
  edge += quarter.domus;
  if (roll < edge) {
    return BlockKind::Domus;
  }
  edge += quarter.market;
  if (roll < edge) {
    return BlockKind::Market;
  }
  edge += quarter.garden;
  if (roll < edge) {
    return BlockKind::Garden;
  }
  return BlockKind::Precinct;
}

void lay_quarter(CityPlanner& planner, const Quarter& quarter, int seed) {
  ArenaScenarioDefinition& scenario = planner.scenario();
  for (const Line& line : quarter.x_lines) {
    if (!line.existing) {
      scenario.roads.push_back(street({line.at, 0.0F, quarter.bounds.z0},
                                      {line.at, 0.0F, quarter.bounds.z1},
                                      line.width));
    }
  }
  for (const Line& line : quarter.z_lines) {
    if (!line.existing) {
      scenario.roads.push_back(street({quarter.bounds.x0, 0.0F, line.at},
                                      {quarter.bounds.x1, 0.0F, line.at},
                                      line.width));
    }
  }

  std::vector<Line> xs{Line{quarter.bounds.x0}};
  xs.insert(xs.end(), quarter.x_lines.begin(), quarter.x_lines.end());
  xs.push_back(Line{quarter.bounds.x1});
  std::vector<Line> zs{Line{quarter.bounds.z0}};
  zs.insert(zs.end(), quarter.z_lines.begin(), quarter.z_lines.end());
  zs.push_back(Line{quarter.bounds.z1});

  auto const inset_of = [](const Line& line) {
    return (line.width * 0.5F) + k_setback;
  };

  int block_index = 0;
  for (std::size_t j = 0; j + 1 < zs.size(); ++j) {
    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
      Rect rect;
      rect.x0 = i == 0 ? xs[i].at + quarter.inset_w : xs[i].at + inset_of(xs[i]);
      rect.x1 = i + 2 == xs.size() ? xs[i + 1].at - quarter.inset_e
                                   : xs[i + 1].at - inset_of(xs[i + 1]);
      rect.z0 = j == 0 ? zs[j].at + quarter.inset_n : zs[j].at + inset_of(zs[j]);
      rect.z1 = j + 2 == zs.size() ? zs[j + 1].at - quarter.inset_s
                                   : zs[j + 1].at - inset_of(zs[j + 1]);

      const float roll = plot_unit(seed, static_cast<int>(i), static_cast<int>(j));
      BlockKind kind = pick_kind(quarter, roll);
      if (planner.steep(rect.centre().x(), rect.centre().z(), 0.11F)) {
        kind = BlockKind::Garden;
      }
      float front = (j + 2 == zs.size()) ? 0.0F : 180.0F;
      if (zs.size() == 2) {
        front = quarter.thin_facing;
      }
      fill_block(planner,
                 QStringLiteral("%1_%2")
                     .arg(QString::fromLatin1(quarter.name))
                     .arg(block_index++),
                 rect,
                 kind,
                 front,
                 seed + (static_cast<int>(i) * 31) + (static_cast<int>(j) * 131));
    }
  }
}

void add_ring(CityPlanner& planner,
              const QString& prefix,
              QVector3D centre,
              float radius_x,
              float radius_z,
              float start_degrees) {
  constexpr float k_deg_to_rad = 0.01745329251994329577F;
  constexpr float k_pi = 3.14159265358979323846F;
  const float perimeter =
      k_pi * (3.0F * (radius_x + radius_z) -
              std::sqrt((3.0F * radius_x + radius_z) * (radius_x + 3.0F * radius_z)));
  const int count = std::max(8, static_cast<int>(perimeter / 5.6F));
  for (int index = 0; index < count; ++index) {
    const float degrees = start_degrees + (360.0F * static_cast<float>(index) /
                                           static_cast<float>(count));
    const float radians = degrees * k_deg_to_rad;
    const QVector3D at(centre.x() + (std::cos(radians) * radius_x),
                       0.0F,
                       centre.z() + (std::sin(radians) * radius_z));
    planner.place(prefix + QStringLiteral("_%1").arg(index),
                  (index % 6 == 0) ? Spawn::Marketplace : Spawn::Home,
                  at,
                  degrees + 180.0F,
                  false,
                  0.6F);
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
           Landmark{"capital_temple",
                    Spawn::Temple,
                    k_avenue_x,
                    k_mountain_z + 6.0F,
                    180.0F},
           Landmark{"capital_oracle",
                    Spawn::Temple,
                    k_avenue_x,
                    k_mountain_z - 28.0F,
                    180.0F},
           Landmark{"capital_treasury",
                    Spawn::Marketplace,
                    -30.0F,
                    k_mountain_z - 12.0F,
                    90.0F},
           Landmark{"capital_hospice",
                    Spawn::Marketplace,
                    30.0F,
                    k_mountain_z - 12.0F,
                    270.0F},
           Landmark{"capital_sanctum_west",
                    Spawn::Temple,
                    -30.0F,
                    k_mountain_z + 20.0F,
                    90.0F},
           Landmark{"capital_sanctum_east",
                    Spawn::Temple,
                    30.0F,
                    k_mountain_z + 20.0F,
                    270.0F},
           Landmark{"capital_lower_temple", Spawn::Temple, -116.0F, -138.0F, 180.0F},

           Landmark{"capital_basilica", Spawn::Marketplace, -42.0F, k_forum_z, 90.0F},
           Landmark{"capital_curia", Spawn::Marketplace, 42.0F, k_forum_z, 270.0F},
           Landmark{"capital_forum_temple", Spawn::Temple, k_avenue_x, -104.0F, 180.0F},
           Landmark{"capital_mint", Spawn::Marketplace, -18.0F, -14.0F, 90.0F},
           Landmark{"capital_granary", Spawn::Marketplace, 18.0F, -14.0F, 270.0F},
           Landmark{"capital_baths", Spawn::Marketplace, -48.0F, -14.0F, 0.0F},
           Landmark{"capital_library", Spawn::Marketplace, 48.0F, -14.0F, 0.0F},

           Landmark{"capital_site_forum",
                    Spawn::Marketplace,
                    -42.0F,
                    k_forum_z + 14.0F,
                    90.0F},
           Landmark{"capital_site_upper", Spawn::Temple, 56.0F, -108.0F, 270.0F},
           Landmark{
               "capital_site_docks", Spawn::Marketplace, 20.0F, k_dock_row_z, 180.0F},
           Landmark{"capital_site_wall", Spawn::Marketplace, -150.0F, -142.0F, 180.0F},
           Landmark{"capital_site_circus", Spawn::Marketplace, 108.0F, 8.0F, 90.0F},
           Landmark{"capital_site_avenue", Spawn::Marketplace, -8.0F, 46.0F, 90.0F},
           Landmark{"capital_site_theatre", Spawn::Temple, -106.0F, 112.0F, 0.0F},
       }) {
    planner.place(QString::fromLatin1(landmark.name),
                  landmark.type,
                  {landmark.x, 0.0F, landmark.z},
                  landmark.facing,
                  true);
  }

  for (int index = 0; index < 5; ++index) {
    const float x = 108.0F + (static_cast<float>(index) * 12.0F);
    planner.place(QStringLiteral("capital_barracks_north_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -178.0F},
                  180.0F,
                  true);
    planner.place(QStringLiteral("capital_barracks_mid_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -144.0F},
                  180.0F,
                  true);
    planner.place(QStringLiteral("capital_barracks_south_%1").arg(index),
                  Spawn::Barracks,
                  {x, 0.0F, -100.0F},
                  0.0F,
                  true);
  }

  for (auto const& depot :
       {std::pair{"capital_grange_west", QVector3D(-150.0F, 0.0F, 206.0F)},
        std::pair{"capital_grange_east", QVector3D(146.0F, 0.0F, 210.0F)},
        std::pair{"capital_quarry_camp_east", QVector3D(192.0F, 0.0F, -252.0F)},
        std::pair{"capital_quarry_camp_west", QVector3D(-252.0F, 0.0F, -246.0F)}}) {
    planner.place(
        QString::fromLatin1(depot.first), Spawn::Barracks, depot.second, 0.0F, true);
  }

  for (int index = 0; index < 3; ++index) {
    planner.place(QStringLiteral("capital_armoury_%1").arg(index),
                  Spawn::Marketplace,
                  {108.0F + (static_cast<float>(index) * 14.0F), 0.0F, -80.0F},
                  0.0F,
                  true);
  }

  int hall = 0;
  for (int index = 0; index < 16; ++index) {
    const float x = -120.0F + (static_cast<float>(index) * 16.0F);
    if (std::abs(x) < 9.0F || std::abs(x - 20.0F) < 9.0F) {
      continue;
    }
    if (planner.place(QStringLiteral("capital_dock_hall_%1").arg(hall),
                      Spawn::Marketplace,
                      {x, 0.0F, k_dock_row_z},
                      180.0F,
                      false,
                      1.0F)) {
      ++hall;
    }
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
                            Field{240.0F, -244.0F, 2, 2, 34.0F},
                            Field{-270.0F, -96.0F, 3, 4, 30.0F},
                            Field{-284.0F, -262.0F, 3, 2, 30.0F},
                            Field{226.0F, -270.0F, 3, 2, 30.0F},
                            Field{222.0F, -96.0F, 3, 4, 30.0F},
                            Field{-262.0F, 206.0F, 3, 2, 30.0F},
                            Field{206.0F, 256.0F, 3, 2, 30.0F}}) {
    for (int row = 0; row < field.rows; ++row) {
      for (int column = 0; column < field.columns; ++column) {
        const float x = field.x0 + (static_cast<float>(column) * field.pitch);
        const float z = field.z0 + (static_cast<float>(row) * field.pitch);
        if (planner.place(QStringLiteral("capital_farm_%1").arg(farm_index),
                          Spawn::Farm,
                          {x, 0.0F, z},
                          ((column + row) % 2 == 0) ? 0.0F : 90.0F,
                          true,
                          1.4F)) {
          ++farm_index;
        }
      }
    }
  }

  struct Farmstead {
    float x;
    float z;
    float facing;
  };
  int stead = 0;
  for (auto const& farmstead :
       {Farmstead{-236.0F, 196.0F, 180.0F},  Farmstead{-228.0F, 196.0F, 180.0F},
        Farmstead{-60.0F, 198.0F, 180.0F},   Farmstead{-52.0F, 198.0F, 180.0F},
        Farmstead{104.0F, 198.0F, 180.0F},   Farmstead{112.0F, 198.0F, 180.0F},
        Farmstead{-176.0F, 262.0F, 0.0F},    Farmstead{-168.0F, 262.0F, 0.0F},
        Farmstead{4.0F, 262.0F, 0.0F},       Farmstead{12.0F, 262.0F, 0.0F},
        Farmstead{-256.0F, 80.0F, 90.0F},    Farmstead{-256.0F, 88.0F, 90.0F},
        Farmstead{196.0F, 88.0F, 270.0F},    Farmstead{196.0F, 96.0F, 270.0F},
        Farmstead{-240.0F, -62.0F, 90.0F},   Farmstead{-240.0F, -54.0F, 90.0F},
        Farmstead{-240.0F, -46.0F, 90.0F},   Farmstead{196.0F, -62.0F, 270.0F},
        Farmstead{196.0F, -54.0F, 270.0F},   Farmstead{196.0F, -46.0F, 270.0F},
        Farmstead{-262.0F, -186.0F, 180.0F}, Farmstead{-254.0F, -186.0F, 180.0F},
        Farmstead{266.0F, -224.0F, 0.0F},    Farmstead{274.0F, -224.0F, 0.0F}}) {
    if (planner.place(QStringLiteral("capital_farmstead_%1").arg(stead),
                      Spawn::Home,
                      {farmstead.x, 0.0F, farmstead.z},
                      farmstead.facing,
                      true,
                      1.0F)) {
      ++stead;
    }
  }
}

void add_quarters(CityPlanner& planner) {
  const float cardo_inset = (4.1F * 0.5F) + k_setback;
  const float avenue_inset = (6.1F * 0.5F) + k_setback;
  const float decumanus_inset = (5.4F * 0.5F) + k_setback;
  const float ring_inset = (4.0F * 0.5F) + k_setback;
  const float outer_inset = (3.4F * 0.5F) + k_setback;
  const float lane_inset = (k_street_width * 0.5F) + k_setback;

  int seed = 4100;
  const Line militaris{k_militaris_z, 4.1F, true};
  const Line decumanus{k_decumanus_z, 5.4F, true};
  const Line ring_south{k_ring_south_z, 4.0F, true};
  const Line ring_outer{k_ring_outer_z, 3.4F, true};
  for (auto const& quarter : {
           Quarter{"capital_nw",
                   {k_wall_west, k_wall_north, k_cardo_west_x, k_decumanus_z},
                   k_wall_clear,
                   cardo_inset,
                   decumanus_inset,
                   k_wall_clear,
                   {Line{-132.0F}},
                   {militaris, Line{-94.0F}, Line{-62.0F}},
                   0.62F,
                   0.16F,
                   0.14F,
                   0.08F},
           Quarter{"capital_w",
                   {k_wall_west, k_decumanus_z, k_cardo_west_x, k_ring_south_z},
                   decumanus_inset,
                   cardo_inset,
                   ring_inset,
                   k_wall_clear,
                   {Line{-132.0F}},
                   {Line{-2.0F}, Line{30.0F}},
                   0.6F,
                   0.2F,
                   0.14F,
                   0.06F},
           Quarter{"capital_north_w",
                   {k_old_west, k_wall_north + 3.0F, -8.0F, k_old_north - 3.0F},
                   k_wall_clear - 3.0F,
                   avenue_inset,
                   k_wall_clear - 3.0F,
                   cardo_inset,
                   {Line{-48.0F}},
                   {Line{k_north_lane_z, k_street_width, true}},
                   0.8F,
                   0.0F,
                   0.2F,
                   0.0F},
           Quarter{"capital_north_e",
                   {8.0F, k_wall_north + 3.0F, k_old_east, k_old_north - 3.0F},
                   k_wall_clear - 3.0F,
                   cardo_inset,
                   k_wall_clear - 3.0F,
                   avenue_inset,
                   {Line{48.0F}},
                   {Line{k_north_lane_z, k_street_width, true}},
                   0.8F,
                   0.0F,
                   0.2F,
                   0.0F},
           Quarter{"capital_sw",
                   {k_cardo_west_x, k_old_south, k_avenue_x, k_wall_south},
                   k_wall_clear,
                   avenue_inset,
                   k_wall_clear,
                   cardo_inset,
                   {Line{-64.0F}, Line{-32.0F}},
                   {Line{30.0F}, ring_south, ring_outer},
                   0.64F,
                   0.16F,
                   0.14F,
                   0.06F},
           Quarter{"capital_se",
                   {k_avenue_x, k_old_south, k_cardo_east_x, k_wall_south},
                   k_wall_clear,
                   cardo_inset,
                   k_wall_clear,
                   avenue_inset,
                   {Line{32.0F}, Line{64.0F}},
                   {Line{30.0F}, ring_south, ring_outer},
                   0.64F,
                   0.16F,
                   0.14F,
                   0.06F},
           Quarter{"capital_e",
                   {k_cardo_east_x, k_ring_south_z, k_wall_east, k_wall_south},
                   ring_inset,
                   k_wall_clear,
                   k_wall_clear,
                   cardo_inset,
                   {Line{133.0F}},
                   {ring_outer},
                   0.6F,
                   0.2F,
                   0.2F,
                   0.0F},

           Quarter{"capital_old_w",
                   {k_old_west, k_upper_south, k_avenue_x, k_old_south},
                   k_wall_clear,
                   avenue_inset,
                   k_wall_clear,
                   k_wall_clear,
                   {Line{-60.0F}, Line{-30.0F}},
                   {decumanus},
                   0.5F,
                   0.28F,
                   0.22F,
                   0.0F},
           Quarter{"capital_old_e",
                   {k_avenue_x, k_upper_south, k_old_east, k_old_south},
                   k_wall_clear,
                   k_wall_clear,
                   k_wall_clear,
                   avenue_inset,
                   {Line{30.0F}, Line{60.0F}},
                   {decumanus},
                   0.5F,
                   0.28F,
                   0.22F,
                   0.0F},
           Quarter{"capital_villa_w",
                   {k_old_west, k_old_north, -40.0F, k_upper_south},
                   k_wall_clear,
                   k_setback,
                   k_wall_clear,
                   k_wall_clear,
                   {Line{-60.0F, 3.4F, true}},
                   {Line{-94.0F, 3.4F, true}},
                   0.0F,
                   0.7F,
                   0.0F,
                   0.15F},
           Quarter{"capital_villa_e",
                   {40.0F, k_old_north, k_old_east, k_upper_south},
                   k_wall_clear,
                   k_wall_clear,
                   k_wall_clear,
                   k_setback,
                   {Line{60.0F, 3.4F, true}},
                   {Line{-94.0F, 3.4F, true}},
                   0.0F,
                   0.7F,
                   0.0F,
                   0.15F},

           Quarter{"capital_vicus_w",
                   {-160.0F, k_wall_south, -7.0F, k_vicus_lane_z},
                   k_wall_clear,
                   avenue_inset,
                   lane_inset,
                   0.0F,
                   {},
                   {},
                   0.85F,
                   0.0F,
                   0.15F,
                   0.0F,
                   180.0F},
           Quarter{"capital_vicus_e",
                   {7.0F, k_wall_south, 160.0F, k_vicus_lane_z},
                   k_wall_clear,
                   0.0F,
                   lane_inset,
                   avenue_inset,
                   {},
                   {},
                   0.85F,
                   0.0F,
                   0.15F,
                   0.0F,
                   180.0F},
       }) {
    lay_quarter(planner, quarter, seed);
    seed += 17;
  }

  add_ring(planner,
           QStringLiteral("capital_circus"),
           {k_circus_x, 0.0F, k_circus_z},
           28.0F,
           19.0F,
           14.0F);
  add_ring(planner,
           QStringLiteral("capital_theatre"),
           {k_theatre_x, 0.0F, k_theatre_z},
           24.0F,
           17.0F,
           8.0F);
}

void add_roads(ArenaScenarioDefinition& scenario) {
  scenario.roads = {
      street({k_avenue_x, 0.0F, k_bridge_north_z}, {k_avenue_x, 0.0F, -68.0F}, 6.1F),
      street({k_avenue_x, 0.0F, -114.0F}, {k_avenue_x, 0.0F, -258.0F}, 6.1F),
      street({k_avenue_x, 0.0F, k_bridge_south_z}, {k_avenue_x, 0.0F, 300.0F}, 4.8F),
      street({164.0F, 0.0F, k_decumanus_z}, {-164.0F, 0.0F, k_decumanus_z}, 5.4F),
      street({k_wall_west, 0.0F, k_militaris_z},
             {k_cardo_west_x, 0.0F, k_militaris_z},
             4.1F),
      street({k_cardo_east_x, 0.0F, k_militaris_z},
             {k_wall_east, 0.0F, k_militaris_z},
             4.1F),
      street({k_cardo_west_x, 0.0F, k_north_lane_z},
             {k_cardo_east_x, 0.0F, k_north_lane_z},
             k_street_width),
      street({k_cardo_west_x, 0.0F, k_wall_north},
             {k_cardo_west_x, 0.0F, k_wall_south},
             4.1F),
      street({k_cardo_east_x, 0.0F, k_wall_north},
             {k_cardo_east_x, 0.0F, k_wall_south},
             4.1F),
      street({-164.0F, 0.0F, k_ring_south_z}, {164.0F, 0.0F, k_ring_south_z}, 4.0F),
      street({-104.0F, 0.0F, k_ring_outer_z}, {164.0F, 0.0F, k_ring_outer_z}, 3.4F),

      street({-88.0F, 0.0F, -94.0F}, {-40.0F, 0.0F, -94.0F}, 3.4F),
      street({40.0F, 0.0F, -94.0F}, {88.0F, 0.0F, -94.0F}, 3.4F),
      street({-60.0F, 0.0F, k_old_north}, {-60.0F, 0.0F, k_upper_south}, 3.4F),
      street({60.0F, 0.0F, k_old_north}, {60.0F, 0.0F, k_upper_south}, 3.4F),

      street({-160.0F, 0.0F, k_vicus_lane_z},
             {160.0F, 0.0F, k_vicus_lane_z},
             k_street_width),
      street({-150.0F, 0.0F, k_quay_z}, {150.0F, 0.0F, k_quay_z}, 5.0F),

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
      street({-240.0F, 0.0F, -150.0F}, {-240.0F, 0.0F, 60.0F}, 3.0F),
      street({206.0F, 0.0F, -140.0F}, {206.0F, 0.0F, 60.0F}, 3.0F),
      street({-300.0F, 0.0F, -200.0F}, {-200.0F, 0.0F, -200.0F}, 3.0F),
      street({200.0F, 0.0F, -200.0F}, {300.0F, 0.0F, -200.0F}, 3.0F),
  };
}

void add_plazas(CityPlanner& planner) {
  planner.pave({k_avenue_x, 0.0F, k_forum_z}, 72.0F, 46.0F);
  planner.pave({k_avenue_x, 0.0F, k_mountain_z - 2.0F}, 84.0F, 80.0F);
  planner.pave({k_avenue_x, 0.0F, -228.0F}, 28.0F, 18.0F);
  planner.pave({k_avenue_x, 0.0F, 108.0F}, 44.0F, 16.0F);
  planner.pave({k_avenue_x, 0.0F, -140.0F}, 30.0F, 16.0F);
  planner.pave({k_avenue_x, 0.0F, k_decumanus_z}, 28.0F, 22.0F);
  planner.pave({k_circus_x, 0.0F, k_circus_z}, 40.0F, 22.0F);
  planner.pave({k_theatre_x, 0.0F, k_theatre_z}, 32.0F, 18.0F);
  planner.pave({133.0F, 0.0F, -122.0F}, 62.0F, 32.0F);
  planner.pave({133.0F, 0.0F, -58.0F}, 62.0F, 26.0F);
  planner.pave({-132.0F, 0.0F, -2.0F}, 22.0F, 18.0F);
  planner.pave({k_cardo_west_x, 0.0F, k_decumanus_z}, 18.0F, 18.0F);
  planner.pave({k_cardo_east_x, 0.0F, k_decumanus_z}, 18.0F, 18.0F);
  planner.pave({k_avenue_x, 0.0F, k_ring_south_z}, 22.0F, 18.0F);
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

      {{k_avenue_x, 0.0F, -70.0F}, 190.0F, 7.0F, 76.0F},
  };
}

struct Crag {
  float x;
  float z;
  float width;
  float depth;
  float height;
  float rotation;
};

void add_crags(ArenaScenarioDefinition& scenario) {
  for (auto const& crag : {
           Crag{-104.0F, -318.0F, 100.0F, 116.0F, 72.0F, 78.0F},
           Crag{104.0F, -322.0F, 100.0F, 116.0F, 74.0F, -76.0F},
           Crag{0.0F, -374.0F, 170.0F, 52.0F, 84.0F, 0.0F},
           Crag{-146.0F, -280.0F, 70.0F, 44.0F, 44.0F, 40.0F},
           Crag{126.0F, -320.0F, 60.0F, 40.0F, 44.0F, -35.0F},
           Crag{-70.0F, -236.0F, 66.0F, 36.0F, 34.0F, 20.0F},
           Crag{72.0F, -240.0F, 66.0F, 36.0F, 36.0F, -20.0F},
           Crag{-44.0F, -196.0F, 40.0F, 22.0F, 20.0F, 30.0F},
           Crag{46.0F, -198.0F, 40.0F, 22.0F, 21.0F, -30.0F},
           Crag{-86.0F, -262.0F, 46.0F, 28.0F, 30.0F, 25.0F},
           Crag{88.0F, -266.0F, 46.0F, 28.0F, 31.0F, -25.0F},

           Crag{-340.0F, -262.0F, 150.0F, 110.0F, 88.0F, 25.0F},
           Crag{356.0F, -276.0F, 140.0F, 100.0F, 96.0F, -25.0F},
           Crag{-360.0F, -96.0F, 130.0F, 90.0F, 70.0F, -10.0F},
           Crag{364.0F, 36.0F, 140.0F, 110.0F, 64.0F, 15.0F},
           Crag{-238.0F, -356.0F, 160.0F, 90.0F, 62.0F, 5.0F},
           Crag{230.0F, -356.0F, 150.0F, 84.0F, 58.0F, -5.0F},
           Crag{214.0F, 350.0F, 160.0F, 80.0F, 40.0F, 10.0F},
           Crag{-334.0F, 336.0F, 150.0F, 80.0F, 34.0F, -10.0F},
       }) {
    Game::Map::TerrainFeature feature;
    feature.type = Game::Map::TerrainType::Mountain;
    feature.center_x = crag.x;
    feature.center_z = crag.z;
    feature.width = crag.width;
    feature.depth = crag.depth;
    feature.radius = std::max(crag.width, crag.depth) * 0.5F;
    feature.height = crag.height / k_campaign_mountain_scale;
    feature.rotation_deg = crag.rotation;
    scenario.terrain_features.push_back(feature);
  }
}

void add_dressing(ArenaScenarioDefinition& scenario) {
  scenario.resource_patches = {
      patch("magic_shrine", 1, {k_avenue_x, 0.0F, k_mountain_z - 56.0F}, {}, 1.8F),
      patch("magic_shrine",
            2,
            {k_avenue_x - 17.5F, 0.0F, k_mountain_z + 4.5F},
            {35.0F, 0.0F, 0.0F},
            1.5F),

      patch("statue", 5, {-6.5F, 0.0F, -128.0F}, {0.0F, 0.0F, -5.0F}, 1.3F),
      patch("statue", 5, {6.5F, 0.0F, -128.0F}, {0.0F, 0.0F, -5.0F}, 1.3F),
      patch("statue", 5, {-32.0F, 0.0F, k_forum_z - 16.0F}, {0.0F, 0.0F, 8.0F}, 1.3F),
      patch("statue", 5, {32.0F, 0.0F, k_forum_z - 16.0F}, {0.0F, 0.0F, 8.0F}, 1.3F),
      patch("statue",
            7,
            {-10.0F, 0.0F, k_mountain_z + 40.0F},
            {0.0F, 0.0F, -10.0F},
            1.5F),
      patch(
          "statue", 7, {10.0F, 0.0F, k_mountain_z + 40.0F}, {0.0F, 0.0F, -10.0F}, 1.5F),
      patch("statue", 2, {-9.0F, 0.0F, -226.0F}, {18.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-9.0F, 0.0F, -196.0F}, {18.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-9.0F, 0.0F, -166.0F}, {18.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-9.0F, 0.0F, k_decumanus_z}, {18.0F, 0.0F, 0.0F}, 1.5F),
      patch("statue", 2, {-9.0F, 0.0F, 16.0F}, {18.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-9.0F, 0.0F, k_ring_south_z}, {18.0F, 0.0F, 0.0F}, 1.3F),
      patch("statue", 2, {-14.0F, 0.0F, 108.0F}, {28.0F, 0.0F, 0.0F}, 1.4F),
      patch("statue", 2, {-14.0F, 0.0F, 156.0F}, {28.0F, 0.0F, 0.0F}, 1.5F),
      patch(
          "statue", 3, {k_circus_x - 6.0F, 0.0F, k_circus_z}, {6.0F, 0.0F, 0.0F}, 1.2F),
      patch("statue", 1, {k_theatre_x, 0.0F, k_theatre_z}, {}, 1.2F),
      patch("statue", 2, {-124.0F, 0.0F, -64.0F}, {0.0F, 0.0F, 4.0F}, 1.2F),
      patch("statue", 2, {-132.0F, 0.0F, -2.0F}, {8.0F, 0.0F, 0.0F}, 1.2F),

      patch("fire_camp", 2, {-14.0F, 0.0F, 110.0F}, {28.0F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 2, {-14.0F, 0.0F, -140.0F}, {28.0F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 2, {-164.0F, 0.0F, -40.0F}, {0.0F, 0.0F, 16.0F}, 1.0F),
      patch("fire_camp", 2, {164.0F, 0.0F, -40.0F}, {0.0F, 0.0F, 16.0F}, 1.0F),
      patch("fire_camp", 3, {112.0F, 0.0F, -132.0F}, {14.0F, 0.0F, 0.0F}, 0.95F),
      patch("fire_camp", 2, {-12.0F, 0.0F, -250.0F}, {24.0F, 0.0F, 0.0F}, 1.1F),

      patch("supply_cart",
            8,
            {-116.0F, 0.0F, k_quay_z + 4.0F},
            {12.0F, 0.0F, 0.0F},
            0.95F),
      patch(
          "supply_cart", 8, {28.0F, 0.0F, k_quay_z + 4.0F}, {12.0F, 0.0F, 0.0F}, 0.95F),
      patch("supply_cart", 4, {-124.0F, 0.0F, -132.0F}, {6.0F, 0.0F, 0.0F}, 0.95F),
      patch("supply_cart", 4, {-60.0F, 0.0F, -68.0F}, {6.0F, 0.0F, 0.0F}, 0.95F),
      patch("weapon_rack", 8, {106.0F, 0.0F, -136.0F}, {6.0F, 0.0F, 0.0F}, 1.0F),
      patch("weapon_rack", 6, {106.0F, 0.0F, -108.0F}, {6.0F, 0.0F, 0.0F}, 1.0F),
      patch("tent", 6, {104.0F, 0.0F, -70.0F}, {8.0F, 0.0F, 0.0F}, 0.9F),
      patch("tent", 6, {104.0F, 0.0F, -60.0F}, {8.0F, 0.0F, 0.0F}, 0.9F),
  };

  struct Rows {
    const char* type;
    float x;
    float z;
    int columns;
    int rows;
    float pitch;
    float rotation;
    float scale;
  };
  int seed = 9100;
  for (auto const& item : {
           Rows{"olive_tree", -212.0F, -104.0F, 8, 6, 7.5F, 0.0F, 1.15F},
           Rows{"olive_tree", 212.0F, -100.0F, 8, 6, 7.5F, 0.0F, 1.15F},
           Rows{"olive_tree", -246.0F, 196.0F, 9, 5, 7.5F, 4.0F, 1.2F},
           Rows{"olive_tree", 200.0F, 200.0F, 9, 5, 7.5F, -6.0F, 1.2F},
           Rows{"olive_tree", -272.0F, 132.0F, 6, 6, 7.5F, 8.0F, 1.2F},
           Rows{"olive_tree", 232.0F, 138.0F, 6, 6, 7.5F, -8.0F, 1.2F},
           Rows{"olive_tree", -150.0F, 268.0F, 8, 5, 7.5F, 0.0F, 1.2F},
           Rows{"olive_tree", 140.0F, 272.0F, 8, 5, 7.5F, 0.0F, 1.2F},
           Rows{"olive_tree", -58.0F, 300.0F, 8, 4, 7.5F, 0.0F, 1.2F},
           Rows{"olive_tree", 56.0F, 306.0F, 8, 4, 7.5F, 0.0F, 1.2F},
           Rows{"olive_tree", -196.0F, 146.0F, 6, 4, 7.5F, 0.0F, 1.15F},
           Rows{"olive_tree", 180.0F, 150.0F, 6, 4, 7.5F, 0.0F, 1.15F},
           Rows{"olive_tree", -40.0F, k_mountain_z + 34.0F, 6, 3, 7.0F, 0.0F, 1.1F},
           Rows{"olive_tree", 40.0F, k_mountain_z + 34.0F, 6, 3, 7.0F, 0.0F, 1.1F},
           Rows{"pine_tree", -14.0F, -100.0F, 1, 6, 7.0F, 0.0F, 1.15F},
           Rows{"pine_tree", 14.0F, -100.0F, 1, 6, 7.0F, 0.0F, 1.15F},
           Rows{"pine_tree", -12.0F, -184.0F, 1, 9, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", 12.0F, -184.0F, 1, 9, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", -36.0F, -280.0F, 1, 4, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", 36.0F, -280.0F, 1, 4, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", -20.0F, 170.0F, 1, 1, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", 20.0F, 170.0F, 1, 1, 7.0F, 0.0F, 1.2F},
           Rows{"pine_tree", -104.0F, 232.0F, 10, 1, 8.0F, 0.0F, 1.25F},
           Rows{"pine_tree", 100.0F, 232.0F, 10, 1, 8.0F, 0.0F, 1.25F},
           Rows{"pine_tree", -220.0F, -36.0F, 1, 12, 8.0F, 0.0F, 1.25F},
           Rows{"pine_tree", 220.0F, -36.0F, 1, 12, 8.0F, 0.0F, 1.25F},
       }) {
    orchard(scenario,
            item.type,
            {item.x, 0.0F, item.z},
            item.columns,
            item.rows,
            item.pitch,
            item.rotation,
            item.scale,
            seed);
    seed += 13;
  }

  struct Scatter {
    const char* type;
    int count;
    float x;
    float z;
    float radius_x;
    float radius_z;
    float scale;
  };
  for (auto const& item : {
           Scatter{"pine_tree", 22, -238.0F, -66.0F, 22.0F, 24.0F, 1.3F},
           Scatter{"pine_tree", 22, 238.0F, -60.0F, 22.0F, 24.0F, 1.3F},
           Scatter{"dead_tree", 12, -34.0F, k_mountain_z - 40.0F, 26.0F, 16.0F, 1.25F},
           Scatter{"ruins", 8, 30.0F, k_mountain_z - 40.0F, 24.0F, 14.0F, 1.3F},

           Scatter{"pine_tree", 40, -300.0F, -270.0F, 44.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 40, 300.0F, -284.0F, 44.0F, 40.0F, 1.4F},
           Scatter{"pine_tree", 34, -318.0F, -140.0F, 34.0F, 44.0F, 1.4F},
           Scatter{"pine_tree", 34, 322.0F, -140.0F, 34.0F, 44.0F, 1.4F},
           Scatter{"pine_tree", 30, -126.0F, -336.0F, 40.0F, 28.0F, 1.4F},
           Scatter{"pine_tree", 30, 118.0F, -340.0F, 40.0F, 28.0F, 1.4F},
           Scatter{"pine_tree", 26, 328.0F, 120.0F, 32.0F, 44.0F, 1.4F},
           Scatter{"pine_tree", 26, -328.0F, 170.0F, 32.0F, 44.0F, 1.4F},
           Scatter{"pine_tree", 26, 60.0F, 340.0F, 46.0F, 26.0F, 1.4F},
           Scatter{"pine_tree", 26, -160.0F, 336.0F, 46.0F, 26.0F, 1.4F},
           Scatter{"pine_tree", 18, -80.0F, -250.0F, 22.0F, 18.0F, 1.3F},
           Scatter{"pine_tree", 18, 82.0F, -254.0F, 22.0F, 18.0F, 1.3F},
           Scatter{"pine_tree", 16, -168.0F, -300.0F, 22.0F, 22.0F, 1.35F},
           Scatter{"pine_tree", 16, 172.0F, -310.0F, 22.0F, 22.0F, 1.35F},
           Scatter{"boulder", 12, -100.0F, -350.0F, 30.0F, 18.0F, 1.2F},
           Scatter{"boulder", 12, 98.0F, -356.0F, 30.0F, 18.0F, 1.2F},
           Scatter{"boulder", 8, -132.0F, -290.0F, 18.0F, 14.0F, 1.1F},
           Scatter{"boulder", 8, 134.0F, -300.0F, 18.0F, 14.0F, 1.1F},

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
           Scatter{"plant", 24, -20.0F, -270.0F, 12.0F, 14.0F, 1.0F},
           Scatter{"plant", 24, 20.0F, -270.0F, 12.0F, 14.0F, 1.0F},

           Scatter{"tent", 22, -212.0F, -160.0F, 26.0F, 18.0F, 0.95F},
           Scatter{"tent", 22, 216.0F, -156.0F, 26.0F, 18.0F, 0.95F},
           Scatter{"tent", 18, -196.0F, 160.0F, 24.0F, 12.0F, 0.95F},
           Scatter{"tent", 18, 190.0F, 162.0F, 24.0F, 12.0F, 0.95F},
           Scatter{"tent", 18, -46.0F, -206.0F, 24.0F, 14.0F, 0.95F},
           Scatter{"tent", 18, 52.0F, -208.0F, 24.0F, 14.0F, 0.95F},
           Scatter{"tent", 16, 168.0F, -262.0F, 22.0F, 16.0F, 0.95F},
           Scatter{"tent", 16, -242.0F, -258.0F, 22.0F, 16.0F, 0.95F},
           Scatter{"tent", 12, 132.0F, -176.0F, 26.0F, 10.0F, 0.95F},
           Scatter{"fire_camp", 10, -210.0F, -158.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"fire_camp", 10, 214.0F, -154.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"fire_camp", 8, -194.0F, 162.0F, 22.0F, 10.0F, 1.0F},
           Scatter{"fire_camp", 8, 188.0F, 164.0F, 22.0F, 10.0F, 1.0F},
           Scatter{"fire_camp", 8, 170.0F, -260.0F, 20.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 8, -240.0F, -256.0F, 20.0F, 14.0F, 1.0F},
           Scatter{"fire_camp", 8, -50.0F, -204.0F, 22.0F, 12.0F, 1.0F},
           Scatter{"fire_camp", 8, 56.0F, -206.0F, 22.0F, 12.0F, 1.0F},
           Scatter{"fire_camp", 5, 132.0F, -170.0F, 24.0F, 8.0F, 1.0F},
           Scatter{"weapon_rack", 14, -206.0F, -166.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"weapon_rack", 14, 210.0F, -162.0F, 22.0F, 14.0F, 1.0F},
           Scatter{"weapon_rack", 10, -190.0F, 156.0F, 20.0F, 10.0F, 1.0F},
           Scatter{"supply_cart", 14, -180.0F, -92.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 14, 184.0F, -88.0F, 24.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 12, 164.0F, -244.0F, 22.0F, 16.0F, 1.0F},
           Scatter{"supply_cart", 12, -120.0F, 190.0F, 24.0F, 12.0F, 1.0F},
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
       k_decumanus_z,
       {{-60.0F, 0.0F, k_decumanus_z},
        {60.0F, 0.0F, k_decumanus_z},
        {150.0F, 0.0F, k_decumanus_z},
        {40.0F, 0.0F, k_decumanus_z},
        {-150.0F, 0.0F, k_decumanus_z}}},
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
       k_north_lane_z,
       {{20.0F, 0.0F, k_north_lane_z},
        {90.0F, 0.0F, k_north_lane_z},
        {60.0F, 0.0F, k_north_lane_z},
        {-90.0F, 0.0F, k_north_lane_z}}},
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
       k_ring_outer_z,
       {{0.0F, 0.0F, k_ring_outer_z},
        {150.0F, 0.0F, k_ring_outer_z},
        {0.0F, 0.0F, k_ring_outer_z},
        {-150.0F, 0.0F, k_ring_outer_z}}},
      {"capital_patrol_docks",
       Troop::Swordsman,
       120.0F,
       k_quay_z,
       {{-60.0F, 0.0F, k_quay_z},
        {-118.0F, 0.0F, k_quay_z},
        {-40.0F, 0.0F, k_quay_z},
        {120.0F, 0.0F, k_quay_z}}},
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
       -60.0F,
       -70.0F,
       {{-60.0F, 0.0F, -118.0F},
        {-60.0F, 0.0F, -70.0F},
        {-60.0F, 0.0F, -100.0F},
        {-60.0F, 0.0F, -70.0F}}},
  };
}

void add_people(ArenaScenarioDefinition& scenario) {
  struct Crowd {
    const char* name;
    int count;
    float x;
    float z;
    bool along_x;
    float roam;
  };
  for (auto const& crowd :
       {Crowd{"capital_forum_crowd", 18, 0.0F, k_forum_z + 4.0F, true, 24.0F},
        Crowd{"capital_market_crowd", 14, 0.0F, k_decumanus_z, true, 20.0F},
        Crowd{"capital_core_folk", 14, 30.0F, -48.0F, false, 22.0F},
        Crowd{"capital_north_quarter_folk", 12, -132.0F, -110.0F, false, 22.0F},
        Crowd{"capital_west_quarter_folk", 12, -132.0F, 12.0F, false, 22.0F},
        Crowd{"capital_east_quarter_folk", 12, 133.0F, -60.0F, true, 22.0F},
        Crowd{"capital_south_quarter_folk", 16, -48.0F, 30.0F, true, 24.0F},
        Crowd{"capital_circus_crowd", 16, k_circus_x, k_circus_z, true, 18.0F},
        Crowd{"capital_theatre_crowd", 12, k_theatre_x, k_theatre_z, true, 14.0F},
        Crowd{"capital_dock_hands", 16, 60.0F, k_quay_z, true, 22.0F},
        Crowd{"capital_temple_pilgrims", 14, 0.0F, -270.0F, true, 20.0F},
        Crowd{"capital_farm_folk", 18, -120.0F, 236.0F, true, 26.0F},
        Crowd{"capital_farm_folk_east", 16, 140.0F, 236.0F, true, 26.0F},

        Crowd{"capital_gate_throng", 22, 0.0F, 108.0F, true, 18.0F},
        Crowd{"capital_gate_throng_west", 16, -24.0F, k_vicus_lane_z, true, 16.0F},
        Crowd{"capital_gate_throng_east", 16, 24.0F, k_vicus_lane_z, true, 16.0F},
        Crowd{"capital_avenue_folk_s", 20, 0.0F, 82.0F, false, 20.0F},
        Crowd{"capital_avenue_folk_m", 20, 0.0F, 44.0F, false, 20.0F},
        Crowd{"capital_avenue_folk_c", 20, 0.0F, 6.0F, false, 20.0F},
        Crowd{"capital_avenue_folk_n", 18, 0.0F, -50.0F, false, 20.0F},
        Crowd{"capital_sacred_way_folk", 18, 0.0F, -150.0F, false, 20.0F},
        Crowd{"capital_sacred_way_upper", 16, 0.0F, -196.0F, false, 18.0F},
        Crowd{"capital_upper_folk_west", 16, -60.0F, -94.0F, false, 20.0F},
        Crowd{"capital_upper_folk_east", 16, 60.0F, -94.0F, false, 20.0F},
        Crowd{"capital_upper_garden", 14, -20.0F, k_forum_z - 10.0F, false, 16.0F},
        Crowd{"capital_upper_garden_east", 14, 20.0F, k_forum_z - 10.0F, false, 16.0F},
        Crowd{"capital_insula_nw_folk", 16, -132.0F, -62.0F, true, 22.0F},
        Crowd{"capital_insula_ne_folk", 16, 133.0F, -122.0F, true, 22.0F},
        Crowd{"capital_insula_sw_folk", 16, -64.0F, 60.0F, true, 22.0F},
        Crowd{"capital_insula_se_folk", 16, 64.0F, 60.0F, true, 22.0F},
        Crowd{"capital_baths_folk", 14, -60.0F, k_decumanus_z, true, 16.0F},
        Crowd{"capital_library_folk", 14, 60.0F, k_decumanus_z, true, 16.0F},
        Crowd{"capital_mint_folk", 14, -30.0F, -46.0F, false, 14.0F},
        Crowd{"capital_granary_folk", 14, 30.0F, -20.0F, false, 14.0F},
        Crowd{"capital_circus_stands", 18, k_circus_x, k_ring_south_z, true, 18.0F},
        Crowd{"capital_dock_market", 16, -60.0F, k_quay_z, true, 20.0F},
        Crowd{"capital_wall_west_folk", 14, -132.0F, -32.0F, true, 22.0F},
        Crowd{"capital_wall_east_folk", 14, 133.0F, k_ring_outer_z, true, 22.0F},
        Crowd{"capital_outskirt_south", 16, -90.0F, k_vicus_lane_z, true, 20.0F},
        Crowd{"capital_outskirt_south_east", 16, 90.0F, k_vicus_lane_z, true, 20.0F},
        Crowd{"capital_outskirt_north", 14, -240.0F, -140.0F, false, 24.0F},
        Crowd{"capital_outskirt_north_east", 14, 206.0F, -130.0F, false, 24.0F}}) {
    scenario.groups.push_back(residents(QString::fromLatin1(crowd.name),
                                        crowd.count,
                                        {crowd.x, 0.0F, crowd.z},
                                        crowd.along_x ? QVector3D(1.8F, 0.0F, 0.0F)
                                                      : QVector3D(0.0F, 0.0F, 1.8F),
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
       {Congregation{"capital_temple_healers", -20.0F, k_mountain_z + 36.0F, 4, 6},
        Congregation{"capital_hospice_healers", 20.0F, k_mountain_z - 40.0F, 3, 6},
        Congregation{"capital_sanctum_choir_s", 0.0F, k_mountain_z + 38.0F, 4, 7},
        Congregation{"capital_sanctum_choir_n", 0.0F, k_mountain_z - 14.0F, 4, 7},
        Congregation{"capital_sanctum_choir_w", -16.0F, k_mountain_z + 4.0F, 3, 7},
        Congregation{"capital_sanctum_choir_e", 16.0F, k_mountain_z + 4.0F, 3, 7},
        Congregation{"capital_sanctum_choir_sw", -16.0F, k_mountain_z + 22.0F, 3, 6},
        Congregation{"capital_sanctum_choir_se", 16.0F, k_mountain_z + 22.0F, 3, 6},
        Congregation{"capital_sanctum_choir_nw", -16.0F, k_mountain_z - 40.0F, 3, 6},
        Congregation{"capital_sanctum_choir_ne", 16.0F, k_mountain_z - 40.0F, 3, 6},
        Congregation{"capital_oracle_choir", 0.0F, k_mountain_z - 42.0F, 3, 6}}) {
    auto congregation = group(QString::fromLatin1(row.name),
                              Troop::Healer,
                              1,
                              row.units,
                              {row.x, 0.0F, row.z},
                              row.per_unit,
                              {8.0F, 0.0F, 0.0F});
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
                                  {126.0F, 0.0F, -130.0F},
                                  16,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_spears"),
                                  Troop::Spearman,
                                  1,
                                  4,
                                  {126.0F, 0.0F, -118.0F},
                                  16,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_archers"),
                                  Troop::Archer,
                                  1,
                                  3,
                                  {126.0F, 0.0F, -108.0F},
                                  12,
                                  {13.0F, 0.0F, 0.0F}));
  scenario.groups.push_back(group(QStringLiteral("capital_garrison_horse"),
                                  Troop::HorseSpearman,
                                  1,
                                  2,
                                  {148.0F, 0.0F, -58.0F},
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
                                  {k_avenue_x, 0.0F, k_north_lane_z},
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
        Crew{"capital_reapers_west", Troop::Builder, -200.0F, 236.0F, 3, false, 8},
        Crew{"capital_reapers_east", Troop::Builder, 132.0F, 236.0F, 3, false, 8},
        Crew{"capital_shepherds", Troop::Builder, -246.0F, 262.0F, 2, false, 6},
        Crew{"capital_wardens", Troop::Builder, 96.0F, 244.0F, 2, false, 6},
        Crew{"capital_masons_forum",
             Troop::Builder,
             -30.0F,
             k_forum_z + 10.0F,
             4,
             false,
             10},
        Crew{"capital_masons_upper",
             Troop::Builder,
             30.0F,
             k_forum_z - 8.0F,
             4,
             false,
             10},
        Crew{"capital_masons_docks", Troop::Builder, 8.0F, k_quay_z, 4, false, 10},
        Crew{"capital_masons_wall",
             Troop::Builder,
             -140.0F,
             k_north_lane_z,
             3,
             false,
             10},
        Crew{"capital_masons_circus", Troop::Builder, 112.0F, k_circus_z, 3, false, 10},
        Crew{
            "capital_repair_crew", Troop::Builder, -110.0F, k_decumanus_z, 3, false, 8},
        Crew{"capital_repair_crew_east",
             Troop::Builder,
             118.0F,
             k_decumanus_z,
             3,
             false,
             8}}) {
    scenario.groups.push_back(worker(QString::fromLatin1(crew.name),
                                     crew.troop,
                                     {crew.x, 0.0F, crew.z},
                                     crew.count,
                                     crew.ai,
                                     crew.crew));
  }

  for (auto const& carriers :
       {Crew{"capital_carriers_forum",
             Troop::Civilian,
             -20.0F,
             k_forum_z + 12.0F,
             4,
             false,
             9},
        Crew{"capital_carriers_docks", Troop::Civilian, 24.0F, k_quay_z, 4, false, 10},
        Crew{"capital_carriers_barracks", Troop::Civilian, 110.0F, -58.0F, 4, false, 9},
        Crew{"capital_carriers_upper",
             Troop::Civilian,
             20.0F,
             k_forum_z - 6.0F,
             4,
             false,
             9},
        Crew{"capital_carriers_farm", Troop::Builder, -144.0F, 236.0F, 4, false, 9},
        Crew{"capital_carriers_quarry", Troop::Builder, 150.0F, -234.0F, 4, false, 9},
        Crew{"capital_carriers_timber", Troop::Builder, -180.0F, -80.0F, 4, false, 9},
        Crew{"capital_carriers_stone", Troop::Builder, 158.0F, -238.0F, 4, false, 9},
        Crew{"capital_carriers_iron", Troop::Builder, 186.0F, -262.0F, 4, false, 9},
        Crew{"capital_carriers_grain", Troop::Builder, -110.0F, 236.0F, 4, false, 9}}) {
    scenario.groups.push_back(worker(QString::fromLatin1(carriers.name),
                                     carriers.troop,
                                     {carriers.x, 0.0F, carriers.z},
                                     carriers.count,
                                     false,
                                     carriers.crew));
  }

  for (auto const& healers :
       {Crew{"capital_upper_healers_west", Troop::Healer, -60.0F, -80.0F, 3, false, 5},
        Crew{"capital_upper_healers_east", Troop::Healer, 60.0F, -80.0F, 3, false, 5},
        Crew{"capital_upper_healers_court",
             Troop::Healer,
             0.0F,
             k_forum_z - 4.0F,
             3,
             false,
             5},
        Crew{"capital_upper_healers_walk",
             Troop::Healer,
             -22.0F,
             k_forum_z + 8.0F,
             2,
             false,
             5},
        Crew{"capital_forum_healers",
             Troop::Healer,
             24.0F,
             k_forum_z + 8.0F,
             2,
             false,
             5}}) {
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

} // namespace

auto authored_city_plan() -> CityPlan {
  CityPlan plan;
  ArenaScenarioDefinition& s = plan.definition;

  s.id = QStringLiteral("aurelia_magna");
  s.label = QStringLiteral("Aurelia Magna");
  s.description = QStringLiteral(
      "An enormous capital on a 768 m stage, built in rings. Farmland, orchards, "
      "quarries, lakes and a ruined old town fill the countryside; a great "
      "eight-gated wall encloses the new city with its circus, theatre, docks and "
      "insulae; an older Republican circuit holds the forum and the ancient core; "
      "and a walled citadel crowns the sacred mountain behind the whole thing, "
      "where the healers keep the temples.");
  s.arena_floor_half_extent = k_content_half;
  s.terrain_grid_extent = k_grid_extent;
  s.ground_type = QStringLiteral("soil_fertile");
  s.terrain_seed_override = 20260819;
  s.terrain_height_scale_override = 42.0F;
  s.suppress_boundary_mountains = true;
  s.camera = {470.0F, 52.0F, 16.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, -120.0F);
  s.environment.start_time = 11.4F;
  s.environment.time_mode = Game::Map::TimeMode::Locked;

  add_roads(s);
  add_water(s);
  add_relief(s);
  add_crags(s);
  add_walls(s);

  CityPlanner planner(s);
  planner.note_existing();
  add_monuments(s, planner);
  add_plazas(planner);
  add_quarters(planner);

  add_dressing(s);
  add_people(s);

  for (const auto& patrol : city_patrols()) {
    plan.patrols.push_back({QString::fromLatin1(patrol.name), patrol.route});
  }

  s.wildlife = Game::Wildlife::default_settings();
  s.wildlife.enabled = true;
  s.wildlife.seed = 20260819U;
  s.wildlife.near_simulation_radius = 200.0F;
  s.wildlife.far_simulation_radius = 380.0F;
  s.wildlife.sheep.enabled = true;
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

  return plan;
}

} // namespace Arena::Scenarios
