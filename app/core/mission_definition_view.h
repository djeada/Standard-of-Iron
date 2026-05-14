#pragma once

#include <QVariantMap>

#include "game/map/mission_definition.h"

[[nodiscard]] auto build_mission_definition_map(
    const Game::Mission::MissionDefinition& mission) -> QVariantMap;
