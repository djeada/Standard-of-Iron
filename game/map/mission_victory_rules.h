#pragma once

#include "game/map/mission_definition.h"
#include "game/systems/victory_service.h"

namespace Game::Mission {

[[nodiscard]] auto
build_victory_rules(const MissionDefinition& mission) -> Game::Systems::VictoryRuleSet;

}
