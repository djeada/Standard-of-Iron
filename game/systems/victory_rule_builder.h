#pragma once

#include "game/systems/victory_rules.h"

namespace Game::Map {
struct VictoryConfig;
}

namespace Game::Systems {

[[nodiscard]] auto
build_rule_set_from_config(const Game::Map::VictoryConfig& config) -> VictoryRuleSet;

} // namespace Game::Systems
