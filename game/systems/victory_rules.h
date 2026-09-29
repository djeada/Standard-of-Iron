#pragma once

#include <QString>

#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "game/systems/resource_types.h"

namespace Game::Systems {

struct StructureRequirement {
  std::vector<QString> structure_types;
  int required_count = 1;
};

struct EliminationVictoryRule {
  std::vector<QString> structure_types;
};

struct SurviveTimeVictoryRule {
  float duration = 0.0F;
};

struct ControlStructuresVictoryRule {
  StructureRequirement target;
};

struct CaptureStructuresVictoryRule {
  StructureRequirement target;
};

struct ClearUndeadZoneVictoryRule {
  QString zone_id;
};

struct PurifyShrineVictoryRule {
  QString zone_id;
};

struct SurviveUndeadWaveVictoryRule {
  QString zone_id;
  int required_wave_count = 1;
};

struct SurviveWavesVictoryRule {
  int required_wave_count = 1;
};

struct AccumulateResourcesVictoryRule {
  ResourceAmounts required;
};

struct EliminateCommandersVictoryRule {};

using VictoryRule = std::variant<EliminationVictoryRule,
                                 SurviveTimeVictoryRule,
                                 ControlStructuresVictoryRule,
                                 CaptureStructuresVictoryRule,
                                 ClearUndeadZoneVictoryRule,
                                 PurifyShrineVictoryRule,
                                 SurviveUndeadWaveVictoryRule,
                                 SurviveWavesVictoryRule,
                                 AccumulateResourcesVictoryRule,
                                 EliminateCommandersVictoryRule>;

struct NoUnitsDefeatRule {};

struct NoKeyStructuresDefeatRule {
  std::vector<QString> structure_types;
};

struct NoCommanderDefeatRule {};

struct OnlyCommanderRemainingDefeatRule {
  std::vector<QString> structure_types;
};

struct TimeLimitDefeatRule {
  float duration = 0.0F;
};

using DefeatRule = std::variant<NoUnitsDefeatRule,
                                NoKeyStructuresDefeatRule,
                                NoCommanderDefeatRule,
                                OnlyCommanderRemainingDefeatRule,
                                TimeLimitDefeatRule>;

struct VictoryObjective {
  VictoryObjective() = default;

  template <
      typename T,
      typename = std::enable_if_t<std::is_constructible_v<VictoryRule, T&&> &&
                                  !std::is_same_v<std::decay_t<T>, VictoryObjective>>>

  VictoryObjective(T&& authored_rule)
      : rule(std::forward<T>(authored_rule)) {}

  VictoryObjective(VictoryRule authored_rule, QString authored_id, QString text)
      : rule(std::move(authored_rule))
      , id(std::move(authored_id))
      , description(std::move(text)) {}

  VictoryRule rule;
  QString id;
  QString description;

  int source_index = -1;
};

struct ObjectiveStatus {
  QString id;
  QString description;
  int source_index = -1;

  QString detail;
  QString compact_detail;

  double fraction = 0.0;
  int progress = 0;
  int required = 1;
  bool complete = false;
};

struct DefeatCondition {
  DefeatCondition() = default;

  template <
      typename T,
      typename = std::enable_if_t<std::is_constructible_v<DefeatRule, T&&> &&
                                  !std::is_same_v<std::decay_t<T>, DefeatCondition>>>

  DefeatCondition(T&& authored_rule)
      : rule(std::forward<T>(authored_rule)) {}

  DefeatCondition(DefeatRule authored_rule, QString text)
      : rule(std::move(authored_rule))
      , description(std::move(text)) {}

  DefeatRule rule;
  QString description;
};

struct VictoryRuleSet {
  std::vector<VictoryObjective> victory_rules;

  std::vector<VictoryObjective> optional_rules;
  std::vector<DefeatCondition> defeat_rules;
  bool include_ambient_undead = false;
  bool require_all_victory_rules = false;
};

} // namespace Game::Systems
