#pragma once

#include <QSet>
#include <QString>

#include <vector>

#include "game/systems/victory_rules.h"

namespace Game::Systems {

struct VictoryRuleTraits {
  QSet<QString> tracked_enemy_structure_types;
  QSet<QString> tracked_local_structure_types;
  std::vector<QString> only_commander_structure_types;
  bool has_time_based_victory = false;
  bool has_undead_zone_rules = false;
  bool has_world_based_rules = false;
  bool has_resource_victory = false;
  bool has_wave_victory = false;
  bool has_time_limit_defeat = false;
  bool requires_captured_structure_tracking = false;
  bool has_only_commander_defeat_rule = false;
  bool has_eliminate_commanders_rule = false;
};

[[nodiscard]] auto
analyze_rule_traits(const VictoryRuleSet& rule_set) -> VictoryRuleTraits;

} // namespace Game::Systems
