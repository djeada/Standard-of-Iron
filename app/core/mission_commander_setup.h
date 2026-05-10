#pragma once

#include "game/map/mission_definition.h"
#include <QString>
#include <optional>
#include <vector>

namespace App::Core {

struct ExistingOwnerSpawnAnchor {
  Game::Mission::Position position;
  bool is_building = false;
};

[[nodiscard]] auto resolve_commander_troop(
    const QString &nation,
    const std::optional<QString> &configured_commander) -> QString;

[[nodiscard]] auto resolve_commander_position(
    const std::vector<Game::Mission::UnitSetup> &units,
    const std::vector<Game::Mission::BuildingSetup> &buildings,
    const std::vector<ExistingOwnerSpawnAnchor> &existing_owner_spawns,
    const Game::Mission::Position &fallback) -> Game::Mission::Position;

} // namespace App::Core
