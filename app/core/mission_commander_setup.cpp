#include "mission_commander_setup.h"

#include <algorithm>

#include "game/systems/nation_id.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"

namespace App::Core {
namespace {

struct WeightedPosition {
  Game::Mission::Position position;
  float weight = 1.0F;
};

constexpr float k_cluster_radius = 12.0F;

auto densest_cluster_position(const std::vector<WeightedPosition>& positions)
    -> std::optional<Game::Mission::Position> {
  if (positions.empty()) {
    return std::nullopt;
  }

  const float cluster_radius_sq = k_cluster_radius * k_cluster_radius;
  float best_weight = -1.0F;
  float best_distance_sum = std::numeric_limits<float>::infinity();
  Game::Mission::Position best_center{};

  for (const auto& candidate : positions) {
    float cluster_weight = 0.0F;
    float weighted_sum_x = 0.0F;
    float weighted_sum_z = 0.0F;
    float distance_sum = 0.0F;

    for (const auto& position : positions) {
      const float dx = position.position.x - candidate.position.x;
      const float dz = position.position.z - candidate.position.z;
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq > cluster_radius_sq) {
        continue;
      }

      cluster_weight += position.weight;
      weighted_sum_x += position.position.x * position.weight;
      weighted_sum_z += position.position.z * position.weight;
      distance_sum += distance_sq * position.weight;
    }

    if (cluster_weight <= 0.0F) {
      continue;
    }

    if (cluster_weight > best_weight ||
        (cluster_weight == best_weight && distance_sum < best_distance_sum)) {
      best_weight = cluster_weight;
      best_distance_sum = distance_sum;
      const float inverse_weight = 1.0F / cluster_weight;
      best_center = {weighted_sum_x * inverse_weight, weighted_sum_z * inverse_weight};
    }
  }

  if (best_weight <= 0.0F) {
    return std::nullopt;
  }
  return best_center;
}

auto weighted_unit_positions(const std::vector<Game::Mission::UnitSetup>& units)
    -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(units.size());
  for (const auto& unit : units) {
    positions.push_back({.position = unit.position,
                         .weight = static_cast<float>(std::max(1, unit.count))});
  }
  return positions;
}

auto building_positions(const std::vector<Game::Mission::BuildingSetup>& buildings)
    -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(buildings.size());
  for (const auto& building : buildings) {
    positions.push_back({.position = building.position});
  }
  return positions;
}

auto existing_positions(const std::vector<ExistingOwnerSpawnAnchor>& anchors,
                        bool want_buildings) -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(anchors.size());
  for (const auto& anchor : anchors) {
    if (anchor.is_building == want_buildings) {
      positions.push_back({.position = anchor.position});
    }
  }
  return positions;
}

} // namespace

auto resolve_commander_troop(const QString& nation,
                             const std::optional<QString>& configured_commander)
    -> QString {
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  const bool parsed_nation = Game::Systems::try_parse_nation_id(nation, nation_id);

  if (configured_commander.has_value()) {
    const QString configured = configured_commander->trimmed();
    if (!configured.isEmpty()) {
      const auto spawn_type =
          Game::Units::spawn_typeFromString(configured.toStdString());
      if (spawn_type.has_value()) {
        const auto troop_type = Game::Units::spawn_typeToTroopType(*spawn_type);
        if (troop_type.has_value()) {
          if (const auto* definition = Game::Units::commander_definition(*troop_type)) {
            if (!parsed_nation || definition->nation_id == nation_id) {
              return configured;
            }
          }
        }
      }
    }
  }

  if (parsed_nation && nation_id == Game::Systems::NationID::Carthage) {
    return QStringLiteral("carthage_elephant_master");
  }

  return QStringLiteral("roman_veteran_consul");
}

auto resolve_commander_position(
    const std::vector<Game::Mission::UnitSetup>& units,
    const std::vector<Game::Mission::BuildingSetup>& buildings,
    const std::vector<ExistingOwnerSpawnAnchor>& existing_owner_spawns,
    const Game::Mission::Position& fallback) -> ResolvedCommanderPosition {
  if (const auto authored_units =
          densest_cluster_position(weighted_unit_positions(units));
      authored_units.has_value()) {
    return {.position = authored_units.value(),
            .space = CommanderPositionSpace::Mission};
  }
  if (const auto authored_buildings =
          densest_cluster_position(building_positions(buildings));
      authored_buildings.has_value()) {
    return {.position = authored_buildings.value(),
            .space = CommanderPositionSpace::Mission};
  }
  if (const auto existing_units =
          densest_cluster_position(existing_positions(existing_owner_spawns, false));
      existing_units.has_value()) {
    return {.position = existing_units.value(), .space = CommanderPositionSpace::World};
  }
  if (const auto existing_buildings =
          densest_cluster_position(existing_positions(existing_owner_spawns, true));
      existing_buildings.has_value()) {
    return {.position = existing_buildings.value(),
            .space = CommanderPositionSpace::World};
  }
  return {.position = fallback, .space = CommanderPositionSpace::Mission};
}

} // namespace App::Core
