#include "mission_victory_rules.h"

#include <QDebug>

#include <algorithm>

namespace Game::Mission {

namespace {

auto normalize_structure_types(const Condition& condition,
                               std::vector<QString> fallback = {
                                   "barracks"}) -> std::vector<QString> {
  std::vector<QString> structure_types;
  if (!condition.structure_types.empty()) {
    structure_types = condition.structure_types;
  } else if (condition.structure_type.has_value()) {
    structure_types.push_back(*condition.structure_type);
  } else {
    structure_types = std::move(fallback);
  }

  std::vector<QString> normalized;
  for (auto& structure_type : structure_types) {
    QString normalized_type = structure_type.trimmed().toLower();
    if (normalized_type == "village") {
      normalized_type = "barracks";
    }
    if (normalized_type.isEmpty()) {
      continue;
    }
    if (std::find(normalized.begin(), normalized.end(), normalized_type) ==
        normalized.end()) {
      normalized.push_back(std::move(normalized_type));
    }
  }

  if (normalized.empty()) {
    normalized.push_back(QStringLiteral("barracks"));
  }

  return normalized;
}

} // namespace

auto build_victory_rules(const MissionDefinition& mission)
    -> Game::Systems::VictoryRuleSet {
  Game::Systems::VictoryRuleSet rules;
  rules.include_ambient_undead = mission.include_ambient_undead;

  for (const auto& condition : mission.victory_conditions) {
    QString const type = condition.type.trimmed().toLower();
    if (type == "destroy_all_enemies") {
      rules.victory_rules.emplace_back(
          Game::Systems::EliminationVictoryRule{{QStringLiteral("barracks")}});
      continue;
    }

    if (type == "survive_duration") {
      if (!condition.duration.has_value()) {
        qWarning() << "Mission victory condition survive_duration is missing duration";
        continue;
      }
      rules.victory_rules.emplace_back(
          Game::Systems::SurviveTimeVictoryRule{std::max(0.0F, *condition.duration)});
      continue;
    }

    if (type == "control_structures" || type == "capture_structures") {
      Game::Systems::StructureRequirement const target{
          normalize_structure_types(condition), condition.min_count.value_or(1)};
      if (type == "control_structures") {
        rules.victory_rules.emplace_back(
            Game::Systems::ControlStructuresVictoryRule{target});
      } else {
        rules.victory_rules.emplace_back(
            Game::Systems::CaptureStructuresVictoryRule{target});
      }
      continue;
    }

    if (type == "clear_undead_zone" || type == "purify_shrine") {
      if (!condition.zone_id.has_value() || condition.zone_id->isEmpty()) {
        qWarning() << "Mission victory condition" << condition.type
                   << "is missing zone_id";
        continue;
      }
      if (type == "clear_undead_zone") {
        rules.victory_rules.emplace_back(
            Game::Systems::ClearUndeadZoneVictoryRule{*condition.zone_id});
      } else {
        rules.victory_rules.emplace_back(
            Game::Systems::PurifyShrineVictoryRule{*condition.zone_id});
      }
      continue;
    }

    if (type == "survive_undead_wave") {
      if (!condition.zone_id.has_value() || condition.zone_id->isEmpty()) {
        qWarning()
            << "Mission victory condition survive_undead_wave is missing zone_id";
        continue;
      }
      rules.victory_rules.emplace_back(Game::Systems::SurviveUndeadWaveVictoryRule{
          *condition.zone_id, std::max(1, condition.wave_count.value_or(1))});
      continue;
    }

    qWarning() << "Unsupported mission victory condition type" << condition.type;
  }

  if (rules.victory_rules.empty()) {
    qWarning()
        << "Mission has no supported victory conditions - defaulting to elimination";
    rules.victory_rules.emplace_back(
        Game::Systems::EliminationVictoryRule{{QStringLiteral("barracks")}});
  }

  for (const auto& condition : mission.defeat_conditions) {
    QString const type = condition.type.trimmed().toLower();
    if (type == "lose_all_units") {
      rules.defeat_rules.emplace_back(Game::Systems::NoUnitsDefeatRule{});
      continue;
    }

    if (type == "lose_commander") {
      rules.defeat_rules.emplace_back(Game::Systems::NoCommanderDefeatRule{});
      continue;
    }

    if (type == "only_commander_remaining") {
      rules.defeat_rules.emplace_back(Game::Systems::OnlyCommanderRemainingDefeatRule{
          normalize_structure_types(condition)});
      continue;
    }

    if (type == "lose_structure") {
      rules.defeat_rules.emplace_back(Game::Systems::NoKeyStructuresDefeatRule{
          normalize_structure_types(condition)});
      continue;
    }

    qWarning() << "Unsupported mission defeat condition type" << condition.type;
  }

  if (rules.defeat_rules.empty()) {
    rules.defeat_rules.emplace_back(Game::Systems::NoCommanderDefeatRule{});
    rules.defeat_rules.emplace_back(
        Game::Systems::OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
  }

  return rules;
}

} // namespace Game::Mission
