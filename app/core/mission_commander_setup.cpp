#include "mission_commander_setup.h"

#include <algorithm>
#include <limits>

#include "game/map/spawn_cluster.h"
#include "game/systems/nation_id.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"

namespace App::Core {
namespace {

using WeightedPosition = Game::Map::WeightedSpawnPoint;

auto densest_cluster_position(const std::vector<WeightedPosition>& positions)
    -> std::optional<Game::Mission::Position> {
  const auto center = Game::Map::densest_spawn_cluster(positions);
  if (!center.has_value()) {
    return std::nullopt;
  }
  return Game::Mission::Position{center->x, center->z};
}

auto weighted_unit_positions(const std::vector<Game::Mission::UnitSetup>& units)
    -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(units.size());
  for (const auto& unit : units) {
    positions.push_back({.x = unit.position.x,
                         .z = unit.position.z,
                         .weight = static_cast<float>(std::max(1, unit.count))});
  }
  return positions;
}

auto building_positions(const std::vector<Game::Mission::BuildingSetup>& buildings)
    -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(buildings.size());
  for (const auto& building : buildings) {
    positions.push_back({.x = building.position.x, .z = building.position.z});
  }
  return positions;
}

auto existing_positions(const std::vector<ExistingOwnerSpawnAnchor>& anchors,
                        bool want_buildings) -> std::vector<WeightedPosition> {
  std::vector<WeightedPosition> positions;
  positions.reserve(anchors.size());
  for (const auto& anchor : anchors) {
    if (anchor.is_building == want_buildings) {
      positions.push_back({.x = anchor.position.x, .z = anchor.position.z});
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

  return Game::Units::troop_typeToQString(
      Game::Units::default_commander_troop_for_nation(
          parsed_nation ? nation_id : Game::Systems::NationID::RomanRepublic));
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
