#pragma once

#include "game/map/mission_definition.h"
#include <QVariantMap>

[[nodiscard]] auto build_mission_definition_map(
    const Game::Mission::MissionDefinition &mission) -> QVariantMap;
