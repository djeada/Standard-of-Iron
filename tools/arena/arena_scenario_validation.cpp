#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

auto expectation_requires_zone(ArenaExpectationKind kind) -> bool {
  return kind == ArenaExpectationKind::UndeadZoneDormantBefore ||
         kind == ArenaExpectationKind::UndeadZoneAwakened ||
         kind == ArenaExpectationKind::UndeadZoneCleared ||
         kind == ArenaExpectationKind::UndeadZoneShrineStands ||
         kind == ArenaExpectationKind::UndeadZoneShrineDestroyed;
}

auto expectation_requires_side(ArenaExpectationKind kind) -> bool {
  switch (kind) {
  case ArenaExpectationKind::SideSurvives:
  case ArenaExpectationKind::SideAdvanceAtLeast:
  case ArenaExpectationKind::SideAdvanceAtMost:
  case ArenaExpectationKind::SideProducesReinforcements:
  case ArenaExpectationKind::SideDoctrineIs:
  case ArenaExpectationKind::SideCommitsToAttack:
  case ArenaExpectationKind::SideHoldsPosition:
  case ArenaExpectationKind::SideBuildsAtLeast:
  case ArenaExpectationKind::SideKeepsGarrison:
  case ArenaExpectationKind::SideFieldsArmy:
    return true;
  default:
    return false;
  }
}

} // namespace

auto validate_scenario(const ArenaScenarioDefinition& definition)
    -> std::vector<ArenaScenarioValidationError> {
  std::vector<ArenaScenarioValidationError> errors;
  if (definition.id.trimmed().isEmpty()) {
    errors.push_back({QStringLiteral("id"), QStringLiteral("scenario id is empty")});
  }
  if (!(definition.duration_seconds > 0.0F)) {
    errors.push_back({QStringLiteral("duration_seconds"),
                      QStringLiteral("duration must be positive")});
  }
  if (definition.groups.empty()) {
    errors.push_back(
        {QStringLiteral("groups"), QStringLiteral("at least one group is required")});
  }
  if (definition.expectations.empty()) {
    errors.push_back({QStringLiteral("expectations"),
                      QStringLiteral("scenario must declare acceptance expectations")});
  }

  QSet<QString> group_names;
  for (std::size_t i = 0; i < definition.groups.size(); ++i) {
    auto const& group = definition.groups[i];
    QString const field = QStringLiteral("groups[%1]").arg(i);
    if (group.name.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("group name is empty")});
    } else if (group_names.contains(group.name)) {
      errors.push_back({field, QStringLiteral("duplicate group '%1'").arg(group.name)});
    } else {
      group_names.insert(group.name);
    }
    if (group.count <= 0) {
      errors.push_back({field, QStringLiteral("group count must be positive")});
    }
    if (group.individuals_per_unit < 0) {
      errors.push_back(
          {field, QStringLiteral("individuals_per_unit cannot be negative")});
    }
  }
  QString const wildlife_group = QString::fromLatin1(k_wildlife_group);
  if (definition.wildlife.enabled && !group_names.contains(wildlife_group)) {
    group_names.insert(wildlife_group);
  }

  auto check_group = [&](const QString& value, const QString& field, bool required) {
    if (value.isEmpty()) {
      if (required) {
        errors.push_back({field, QStringLiteral("group reference is required")});
      }
      return;
    }
    if (!group_names.contains(value)) {
      errors.push_back(
          {field, QStringLiteral("unknown group reference '%1'").arg(value)});
    }
  };

  if (definition.rpg_mode) {
    check_group(
        definition.rpg_commander_group, QStringLiteral("rpg_commander_group"), true);
    auto const commander_group = std::find_if(
        definition.groups.begin(), definition.groups.end(), [&](auto const& group) {
          return group.name == definition.rpg_commander_group;
        });
    if (commander_group != definition.groups.end() && commander_group->count != 1) {
      errors.push_back(
          {QStringLiteral("rpg_commander_group"),
           QStringLiteral("RPG commander group must contain exactly one unit")});
    }
  } else if (!definition.rpg_commander_group.isEmpty()) {
    errors.push_back({QStringLiteral("rpg_commander_group"),
                      QStringLiteral("RPG commander group requires rpg_mode")});
  }

  QSet<QString> zone_ids;
  for (std::size_t i = 0; i < definition.undead_zones.size(); ++i) {
    auto const& zone = definition.undead_zones[i];
    QString const field = QStringLiteral("undead_zones[%1]").arg(i);
    if (zone.id.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("undead zone id is empty")});
    } else if (zone_ids.contains(zone.id)) {
      errors.push_back(
          {field, QStringLiteral("duplicate undead zone '%1'").arg(zone.id)});
    } else {
      zone_ids.insert(zone.id);
    }
    if (!(zone.radius > 0.0F)) {
      errors.push_back({field, QStringLiteral("undead zone radius must be positive")});
    }
  }

  QSet<QString> rockfall_ids;
  for (std::size_t i = 0; i < definition.rockfall_traps.size(); ++i) {
    auto const& trap = definition.rockfall_traps[i];
    QString const field = QStringLiteral("rockfall_traps[%1]").arg(i);
    if (trap.id.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("rockfall trap id is empty")});
    } else if (rockfall_ids.contains(trap.id)) {
      errors.push_back(
          {field, QStringLiteral("duplicate rockfall trap '%1'").arg(trap.id)});
    } else {
      rockfall_ids.insert(trap.id);
    }
    if (trap.boulder_count <= 0) {
      errors.push_back({field, QStringLiteral("rockfall trap needs boulders")});
    }
  }

  QSet<QString> raft_ids;
  for (std::size_t i = 0; i < definition.rafts.size(); ++i) {
    auto const& raft = definition.rafts[i];
    QString const field = QStringLiteral("rafts[%1]").arg(i);
    if (raft.id.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("raft id is empty")});
    } else if (raft_ids.contains(raft.id)) {
      errors.push_back({field, QStringLiteral("duplicate raft '%1'").arg(raft.id)});
    } else {
      raft_ids.insert(raft.id);
    }
    if (definition.rivers.empty()) {
      errors.push_back({field, QStringLiteral("a raft needs a river to float on")});
    }
  }

  for (std::size_t i = 0; i < definition.fords.size(); ++i) {
    if (definition.rivers.empty()) {
      errors.push_back({QStringLiteral("fords[%1]").arg(i),
                        QStringLiteral("a ford needs a river to cross")});
    }
  }

  for (std::size_t i = 0; i < definition.steps.size(); ++i) {
    auto const& step = definition.steps[i];
    QString const field = QStringLiteral("steps[%1]").arg(i);
    if (step.trigger.time_seconds < 0.0F) {
      errors.push_back({field, QStringLiteral("trigger time cannot be negative")});
    }
    bool const rockfall_step = step.command == ScenarioCommandKind::TriggerRockfall;
    bool const command_needs_group =
        step.zone_id.isEmpty() && step.command != ScenarioCommandKind::SetCamera &&
        step.command != ScenarioCommandKind::SetFullCreatureLod &&
        step.command != ScenarioCommandKind::ReloadUndeadZoneState && !rockfall_step;
    check_group(step.group, field + QStringLiteral(".group"), command_needs_group);
    if (rockfall_step && !rockfall_ids.contains(step.zone_id)) {
      errors.push_back(
          {field + QStringLiteral(".zone_id"),
           QStringLiteral("unknown rockfall trap reference '%1'").arg(step.zone_id)});
    }
    if (!rockfall_step && !step.zone_id.isEmpty() && !zone_ids.contains(step.zone_id)) {
      errors.push_back(
          {field + QStringLiteral(".zone_id"),
           QStringLiteral("unknown undead zone reference '%1'").arg(step.zone_id)});
    }
    bool const command_needs_target =
        step.command == ScenarioCommandKind::Attack ||
        step.command == ScenarioCommandKind::AttackMove ||
        step.command == ScenarioCommandKind::Charge ||
        step.command == ScenarioCommandKind::ReleaseReserve ||
        step.command == ScenarioCommandKind::MeleeLock;
    check_group(step.target_group,
                field + QStringLiteral(".target_group"),
                command_needs_target);
    if (step.trigger.kind != ScenarioTriggerKind::AtTime &&
        step.trigger.kind != ScenarioTriggerKind::PreviousStepComplete) {
      check_group(step.trigger.group, field + QStringLiteral(".trigger.group"), true);
    }
    if (step.trigger.kind == ScenarioTriggerKind::FirstContact ||
        step.trigger.kind == ScenarioTriggerKind::GroupsWithinDistance) {
      check_group(step.trigger.target_group,
                  field + QStringLiteral(".trigger.target_group"),
                  true);
    }

    for (int member = 0; member < step.formation.groups.size(); ++member) {
      check_group(step.formation.groups.at(member),
                  field + QStringLiteral(".formation.groups[%1]").arg(member),
                  true);
    }
    if (step.command == ScenarioCommandKind::FormArmy &&
        step.formation.frontage < 0.0F) {
      errors.push_back(
          {field, QStringLiteral("formation frontage cannot be negative")});
    }
  }

  QSet<QString> battle_side_labels;
  for (std::size_t i = 0; i < definition.battle_sides.size(); ++i) {
    auto const& side = definition.battle_sides[i];
    QString const field = QStringLiteral("battle_sides[%1]").arg(i);
    if (side.label.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("battle side label is empty")});
    } else if (battle_side_labels.contains(side.label)) {
      errors.push_back(
          {field, QStringLiteral("duplicate battle side '%1'").arg(side.label)});
    } else {
      battle_side_labels.insert(side.label);
    }
  }

  auto check_side = [&](const QString& value, const QString& field) {
    if (value.isEmpty()) {
      errors.push_back({field, QStringLiteral("battle side reference is required")});
      return;
    }
    if (!battle_side_labels.contains(value)) {
      errors.push_back(
          {field, QStringLiteral("unknown battle side reference '%1'").arg(value)});
    }
  };

  for (std::size_t i = 0; i < definition.expectations.size(); ++i) {
    auto const& expectation = definition.expectations[i];
    QString const field = QStringLiteral("expectations[%1]").arg(i);
    if (expectation_requires_side(expectation.kind)) {
      check_side(expectation.side, field + QStringLiteral(".side"));
    }
    if (expectation.kind == ArenaExpectationKind::BattleReachesDecision &&
        definition.battle_sides.size() < 2U) {
      errors.push_back({field,
                        QStringLiteral("BattleReachesDecision requires at least two "
                                       "authored battle sides")});
    }
    if (expectation_requires_zone(expectation.kind)) {
      if (expectation.zone_id.isEmpty()) {
        errors.push_back({field + QStringLiteral(".zone_id"),
                          QStringLiteral("undead zone reference is required")});
      } else if (!zone_ids.contains(expectation.zone_id)) {
        errors.push_back({field + QStringLiteral(".zone_id"),
                          QStringLiteral("unknown undead zone reference '%1'")
                              .arg(expectation.zone_id)});
      }
    }
    check_group(expectation.group, field + QStringLiteral(".group"), false);
    check_group(
        expectation.target_group, field + QStringLiteral(".target_group"), false);
    if (expectation.end_seconds > 0.0F &&
        expectation.end_seconds < expectation.start_seconds) {
      errors.push_back({field, QStringLiteral("expectation end precedes its start")});
    }
  }
  return errors;
}

} // namespace Arena
