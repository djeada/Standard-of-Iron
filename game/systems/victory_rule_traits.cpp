#include "victory_rule_traits.h"

#include <algorithm>
#include <variant>

namespace Game::Systems {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

void collect_victory_traits(const VictoryRuleSet& rule_set, VictoryRuleTraits& traits) {
  std::vector<const VictoryObjective*> tracked;
  tracked.reserve(rule_set.victory_rules.size() + rule_set.optional_rules.size());
  for (const auto& objective : rule_set.victory_rules) {
    tracked.push_back(&objective);
  }
  for (const auto& objective : rule_set.optional_rules) {
    tracked.push_back(&objective);
  }
  for (const auto* tracked_objective : tracked) {
    const auto& objective = *tracked_objective;
    std::visit(
        Overloaded{
            [&traits](const EliminationVictoryRule& elimination_rule) {
              traits.has_world_based_rules = true;
              for (const auto& structure_type : elimination_rule.structure_types) {
                traits.tracked_enemy_structure_types.insert(structure_type);
              }
            },
            [&traits](const ControlStructuresVictoryRule& control_rule) {
              traits.has_world_based_rules = true;
              for (const auto& structure_type : control_rule.target.structure_types) {
                traits.tracked_local_structure_types.insert(structure_type);
              }
            },
            [&traits](const CaptureStructuresVictoryRule& capture_rule) {
              traits.has_world_based_rules = true;
              traits.requires_captured_structure_tracking = true;
              for (const auto& structure_type : capture_rule.target.structure_types) {
                traits.tracked_local_structure_types.insert(structure_type);
              }
            },
            [&traits](const ClearUndeadZoneVictoryRule&) {
              traits.has_undead_zone_rules = true;
            },
            [&traits](const PurifyShrineVictoryRule&) {
              traits.has_undead_zone_rules = true;
            },
            [&traits](const SurviveUndeadWaveVictoryRule&) {
              traits.has_undead_zone_rules = true;
            },
            [&traits](const SurviveTimeVictoryRule&) {
              traits.has_time_based_victory = true;
            },
            [&traits](const SurviveWavesVictoryRule&) {
              traits.has_wave_victory = true;
            },
            [&traits](const AccumulateResourcesVictoryRule&) {
              traits.has_resource_victory = true;
            },
            [&traits](const EliminateCommandersVictoryRule&) {
              traits.has_world_based_rules = true;
              traits.has_eliminate_commanders_rule = true;
            }},
        objective.rule);
  }
}

void collect_defeat_traits(const VictoryRuleSet& rule_set, VictoryRuleTraits& traits) {
  for (const auto& condition : rule_set.defeat_rules) {
    traits.has_world_based_rules = true;
    std::visit(
        Overloaded{
            [](const NoUnitsDefeatRule&) {},
            [](const NoCommanderDefeatRule&) {},
            [&traits](const NoKeyStructuresDefeatRule& no_structures_rule) {
              for (const auto& structure_type : no_structures_rule.structure_types) {
                traits.tracked_local_structure_types.insert(structure_type);
              }
            },
            [&traits](const OnlyCommanderRemainingDefeatRule& isolated_commander_rule) {
              traits.has_only_commander_defeat_rule = true;
              for (const auto& structure_type :
                   isolated_commander_rule.structure_types) {
                if (std::find(traits.only_commander_structure_types.begin(),
                              traits.only_commander_structure_types.end(),
                              structure_type) ==
                    traits.only_commander_structure_types.end()) {
                  traits.only_commander_structure_types.push_back(structure_type);
                }
                traits.tracked_local_structure_types.insert(structure_type);
              }
            },
            [&traits](const TimeLimitDefeatRule&) {
              traits.has_time_limit_defeat = true;
            }},
        condition.rule);
  }
}

} // namespace

auto analyze_rule_traits(const VictoryRuleSet& rule_set) -> VictoryRuleTraits {
  VictoryRuleTraits traits;
  collect_victory_traits(rule_set, traits);
  collect_defeat_traits(rule_set, traits);
  return traits;
}

} // namespace Game::Systems
