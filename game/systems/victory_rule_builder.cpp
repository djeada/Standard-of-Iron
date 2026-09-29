#include "victory_rule_builder.h"

#include <QDebug>
#include <QSet>

#include <algorithm>
#include <utility>
#include <variant>

#include "game/map/map_definition.h"

namespace Game::Systems {

namespace {

auto normalize_structure_types(std::vector<QString> structure_types,
                               std::vector<QString> fallback = {
                                   "barracks"}) -> std::vector<QString> {
  if (structure_types.empty()) {
    structure_types = std::move(fallback);
  }

  std::vector<QString> normalized;
  QSet<QString> seen_types;
  for (auto& type : structure_types) {
    QString normalized_type = type.trimmed().toLower();
    if (normalized_type == "village") {
      normalized_type = "barracks";
    }
    if (normalized_type.isEmpty() || seen_types.contains(normalized_type)) {
      continue;
    }
    seen_types.insert(normalized_type);
    normalized.push_back(std::move(normalized_type));
  }

  if (normalized.empty()) {
    normalized.push_back(QStringLiteral("barracks"));
  }

  return normalized;
}

void append_undead_objectives(const Game::Map::VictoryConfig& config,
                              VictoryRuleSet& rules) {
  for (const auto& objective : config.undead_objectives) {
    if (objective.zone_id.isEmpty()) {
      continue;
    }
    const std::size_t before = rules.victory_rules.size();
    if (objective.type == "clear_undead_zone") {
      rules.victory_rules.emplace_back(ClearUndeadZoneVictoryRule{objective.zone_id});
    } else if (objective.type == "purify_shrine") {
      rules.victory_rules.emplace_back(PurifyShrineVictoryRule{objective.zone_id});
    } else if (objective.type == "survive_undead_wave") {
      rules.victory_rules.emplace_back(SurviveUndeadWaveVictoryRule{
          objective.zone_id, std::max(1, objective.wave_count)});
    } else {
      qWarning() << "Unknown undead victory objective" << objective.type
                 << "- ignoring";
      continue;
    }
    for (std::size_t index = before; index < rules.victory_rules.size(); ++index) {
      rules.victory_rules[index].id = objective.zone_id;
    }
  }
}

} // namespace

auto build_rule_set_from_config(const Game::Map::VictoryConfig& config)
    -> VictoryRuleSet {
  VictoryRuleSet rules;

  QString const victory_type = config.victory_type.trimmed().toLower();
  if (victory_type == "undead_zones") {
    append_undead_objectives(config, rules);
    if (rules.victory_rules.empty()) {
      qWarning() << "Victory type undead_zones declares no undead_objectives - "
                    "defaulting to elimination";
      rules.victory_rules.emplace_back(
          EliminationVictoryRule{{QStringLiteral("barracks")}});
    }
  } else if (victory_type == "elimination") {
    rules.victory_rules.emplace_back(
        EliminationVictoryRule{normalize_structure_types(config.key_structures)});
  } else if (victory_type == "control_structures") {
    rules.victory_rules.emplace_back(ControlStructuresVictoryRule{
        StructureRequirement{normalize_structure_types(config.key_structures),
                             std::max(1, config.required_key_structures)}});
  } else if (victory_type == "capture_structures") {
    rules.victory_rules.emplace_back(CaptureStructuresVictoryRule{
        StructureRequirement{normalize_structure_types(config.key_structures),
                             std::max(1, config.required_key_structures)}});
  } else if (victory_type == "survive_time") {
    rules.victory_rules.emplace_back(
        SurviveTimeVictoryRule{std::max(0.0F, config.survive_time_duration)});
  } else {
    qWarning() << "Unknown victory type" << config.victory_type
               << "- defaulting to elimination";
    rules.victory_rules.emplace_back(
        EliminationVictoryRule{{QStringLiteral("barracks")}});
  }

  if (victory_type != "undead_zones") {
    append_undead_objectives(config, rules);
  }

  std::vector<QString> const default_defeat_structures =
      normalize_structure_types(config.key_structures);
  bool has_commander_defeat = false;
  for (const auto& condition : config.defeat_conditions) {
    QString const normalized_condition = condition.trimmed().toLower();
    if (normalized_condition == "no_units") {
      rules.defeat_rules.emplace_back(NoUnitsDefeatRule{});
      continue;
    }
    if (normalized_condition == "no_key_structures") {
      rules.defeat_rules.emplace_back(
          NoKeyStructuresDefeatRule{default_defeat_structures});
      continue;
    }
    if (normalized_condition == "no_commander") {
      rules.defeat_rules.emplace_back(NoCommanderDefeatRule{});
      has_commander_defeat = true;
      continue;
    }
    if (normalized_condition == "only_commander_remaining") {
      rules.defeat_rules.emplace_back(
          OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
      continue;
    }
    qWarning() << "Unknown defeat condition" << condition << "- ignoring";
  }

  if (rules.defeat_rules.empty()) {
    rules.defeat_rules.emplace_back(
        OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
  }

  if (!has_commander_defeat) {
    rules.defeat_rules.emplace_back(NoCommanderDefeatRule{});
  }

  bool const already_wins_on_commanders = std::any_of(
      rules.victory_rules.begin(),
      rules.victory_rules.end(),
      [](const VictoryObjective& objective) {
        return std::holds_alternative<EliminateCommandersVictoryRule>(objective.rule);
      });
  if (!already_wins_on_commanders) {
    rules.victory_rules.emplace_back(EliminateCommandersVictoryRule{});
  }

  return rules;
}

} // namespace Game::Systems
