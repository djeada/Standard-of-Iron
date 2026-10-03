#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

auto describe_window(const ArenaExpectation& expectation) -> QString {
  if (expectation.start_seconds <= 0.0F && expectation.end_seconds <= 0.0F) {
    return {};
  }
  return QStringLiteral(" between %1 s and %2 s")
      .arg(expectation.start_seconds, 0, 'f', 1)
      .arg(expectation.end_seconds > 0.0F
               ? QString::number(expectation.end_seconds, 'f', 1)
               : QStringLiteral("the end"));
}

} // namespace

void ArenaScenarioRunner::Impl::check_battle_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::BattleReachesDecision: {
    if (battle_sides.size() < 2U) {
      add_issue(QStringLiteral("battle_not_tracked"),
                QStringLiteral("BattleReachesDecision needs at least two "
                               "authored battle sides"));
      break;
    }
    if (!battle_decided) {
      QStringList standing;
      for (auto const& side : battle_sides) {
        if (side.eliminated_at < 0.0F) {
          standing.push_back(QStringLiteral("%1 (%2 units, %3 buildings)")
                                 .arg(side.label)
                                 .arg(side.living_units)
                                 .arg(side.living_buildings));
        }
      }
      add_issue(QStringLiteral("battle_undecided"),
                QStringLiteral("no side was eliminated within %1 s; still "
                               "standing: %2")
                    .arg(elapsed, 0, 'f', 1)
                    .arg(standing.join(QStringLiteral(", "))));
    }
    break;
  }
  case ArenaExpectationKind::SideSurvives: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else if (side->eliminated_at >= 0.0F) {
      add_issue(QStringLiteral("battle_side_eliminated"),
                QStringLiteral("%1 was wiped out at %2 s")
                    .arg(side->label)
                    .arg(side->eliminated_at, 0, 'f', 1));
    }
    break;
  }
  case ArenaExpectationKind::SideAdvanceAtLeast: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else {
      auto const peak = windowed_peak_advance(*side, expectation);
      if (!peak.has_value()) {
        add_issue(QStringLiteral("side_advance_unsampled"),
                  QStringLiteral("%1 produced no army position samples in its "
                                 "measurement window")
                      .arg(side->label));
      } else if (*peak < expectation.threshold) {
        add_issue(QStringLiteral("side_advance_too_small"),
                  QStringLiteral("%1 pushed only %2 of the way to the enemy "
                                 "base%3, expected at least %4")
                      .arg(side->label)
                      .arg(*peak, 0, 'f', 3)
                      .arg(describe_window(expectation))
                      .arg(expectation.threshold, 0, 'f', 3));
      }
    }
    break;
  }
  case ArenaExpectationKind::SideAdvanceAtMost: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else {
      auto const peak = windowed_peak_advance(*side, expectation);
      if (peak.has_value() && *peak > expectation.threshold) {
        add_issue(QStringLiteral("side_advance_too_large"),
                  QStringLiteral("%1 pushed %2 of the way to the enemy base%3, "
                                 "expected at most %4")
                      .arg(side->label)
                      .arg(*peak, 0, 'f', 3)
                      .arg(describe_window(expectation))
                      .arg(expectation.threshold, 0, 'f', 3));
      }
    }
    break;
  }
  case ArenaExpectationKind::SideProducesReinforcements: {
    auto const* side = battle_side(expectation.side);
    int const required =
        std::max(1, static_cast<int>(std::lround(expectation.threshold)));
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else {
      int const produced =
          static_cast<int>(side->seen_units.size()) - side->initial_units;
      if (produced < required) {
        add_issue(QStringLiteral("side_produced_too_few"),
                  QStringLiteral("%1 fielded %2 new units, expected at least %3")
                      .arg(side->label)
                      .arg(produced)
                      .arg(required));
      }
    }
    break;
  }
  case ArenaExpectationKind::SideBuildsAtLeast: {
    auto const* side = battle_side(expectation.side);
    int const required =
        std::max(1, static_cast<int>(std::lround(expectation.threshold)));
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else {
      int const built = static_cast<int>(side->seen_buildings.size()) -
                        static_cast<int>(side->initial_buildings.size());
      if (built < required) {
        add_issue(QStringLiteral("side_built_too_little"),
                  QStringLiteral("%1 raised %2 new buildings, expected at "
                                 "least %3")
                      .arg(side->label)
                      .arg(built)
                      .arg(required));
      }
    }
    break;
  }
  case ArenaExpectationKind::SideKeepsGarrison: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else if (side->peak_home_units < expectation.threshold) {
      add_issue(QStringLiteral("side_left_home_open"),
                QStringLiteral("%1 never held more than %2 units within %3 m of "
                               "its own base, expected at least %4")
                    .arg(side->label)
                    .arg(side->peak_home_units)
                    .arg(side->home_radius, 0, 'f', 0)
                    .arg(expectation.threshold, 0, 'f', 0));
    }
    break;
  }
  case ArenaExpectationKind::SideFieldsArmy: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else if (side->peak_forward_units < expectation.threshold) {
      add_issue(QStringLiteral("side_never_fielded_army"),
                QStringLiteral("%1 never pushed more than %2 units past the "
                               "midpoint, expected at least %3")
                    .arg(side->label)
                    .arg(side->peak_forward_units)
                    .arg(expectation.threshold, 0, 'f', 0));
    }
    break;
  }
  case ArenaExpectationKind::SideDoctrineIs: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
      break;
    }
    if (side->strategy.isEmpty()) {
      add_issue(QStringLiteral("side_doctrine_unsampled"),
                QStringLiteral("%1 never reported an AI strategy; the scenario "
                               "host did not expose one")
                    .arg(side->label));
      break;
    }
    QString const expected = expectation.counter_key;
    QString const actual = side->posture.isEmpty()
                               ? side->strategy
                               : side->strategy + QStringLiteral(":") + side->posture;
    if (expected != side->strategy && expected != actual) {
      add_issue(QStringLiteral("side_doctrine_mismatch"),
                QStringLiteral("%1 ran the %2 doctrine, expected %3")
                    .arg(side->label, actual, expected));
    }
    break;
  }
  case ArenaExpectationKind::SideCommitsToAttack: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else if (side->seconds_attacking < expectation.threshold) {
      add_issue(QStringLiteral("side_never_committed"),
                QStringLiteral("%1 spent %2 s in an attacking state, expected "
                               "at least %3 s")
                    .arg(side->label)
                    .arg(side->seconds_attacking, 0, 'f', 1)
                    .arg(expectation.threshold, 0, 'f', 1));
    }
    break;
  }
  case ArenaExpectationKind::SideHoldsPosition: {
    auto const* side = battle_side(expectation.side);
    if (side == nullptr) {
      add_issue(
          QStringLiteral("battle_side_unknown"),
          QStringLiteral("%1 is not an authored battle side").arg(expectation.side));
    } else if (side->seconds_attacking > expectation.threshold) {
      add_issue(QStringLiteral("side_did_not_hold"),
                QStringLiteral("%1 spent %2 s in an attacking state, expected "
                               "at most %3 s")
                    .arg(side->label)
                    .arg(side->seconds_attacking, 0, 'f', 1)
                    .arg(expectation.threshold, 0, 'f', 1));
    }
    break;
  }
  default:
    break;
  }
}

void ArenaScenarioRunner::Impl::check_combat_expectation(
    const ArenaExpectation& expectation) {
  switch (expectation.kind) {
  case ArenaExpectationKind::AttackAnimationObserved:
    if (!visible_attacks.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_attack"),
                QStringLiteral("%1 never produced a visible attack animation")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RepeatedAttackAnimationObserved: {
    int const required_cycles =
        expectation.threshold > 0.0F
            ? std::max(2, static_cast<int>(expectation.threshold))
            : 2;
    int missing = 0;
    for (auto const soldier : living_soldiers_by_group.value(expectation.group)) {
      if (attack_entries_by_soldier.value(soldier, 0) < required_cycles) {
        ++missing;
      }
    }
    if (missing > 0) {
      add_issue(QStringLiteral("soldiers_did_not_repeat_attack"),
                QStringLiteral("%1 had %2 living soldiers with fewer than %3 "
                               "visible attack cycles")
                    .arg(expectation.group)
                    .arg(missing)
                    .arg(required_cycles));
    }
    break;
  }
  case ArenaExpectationKind::MovementAnimationObserved:
    if (!visible_movement.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_movement"),
                QStringLiteral("%1 never produced a visible movement animation")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::AttackHasVisibleContact:
    if (!visible_attacks.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_attack"),
                QStringLiteral("%1 never produced a visible attack animation")
                    .arg(expectation.group));
    } else if (!paired_visible_attacks.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_paired_visible_attack"),
                QStringLiteral("%1 never attacked through a physical soldier "
                               "engagement pair")
                    .arg(expectation.group));
    } else if (!expectation.target_group.isEmpty() &&
               !damage_seen.value(expectation.target_group, false) &&
               !group_destroyed(expectation.target_group)) {
      add_issue(QStringLiteral("no_visible_contact_result"),
                QStringLiteral("%1 attacked but %2 never registered contact damage")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  case ArenaExpectationKind::ProjectileFlightObserved: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (!projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("projectile_flight_not_observed"),
                QStringLiteral("%1 never showed an in-flight projectile toward %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::ProjectileImpactObserved: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (!projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("projectile_flight_not_observed"),
                QStringLiteral("%1 never showed an in-flight projectile toward %2")
                    .arg(expectation.group, expectation.target_group));
    } else if (!projectile_contacts.value(key, false)) {
      add_issue(QStringLiteral("projectile_contact_not_observed"),
                QStringLiteral("%1 produced no visible projectile contact on %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::ProjectileImpactSynchronized: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (!projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("projectile_flight_not_observed"),
                QStringLiteral("%1 never showed an in-flight projectile toward %2")
                    .arg(expectation.group, expectation.target_group));
    } else if (!projectile_impacts.value(key, false)) {
      add_issue(QStringLiteral("projectile_impact_not_synchronized"),
                QStringLiteral("%1 produced no renderer-facing impact record in the "
                               "same simulation event that damaged %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::GroupHealthUnchanged: {
    int const initial = initial_health_by_group.value(expectation.group, -1);
    int const current = group_health(expectation.group);
    if (initial < 0 || current != initial) {
      add_issue(QStringLiteral("group_health_changed"),
                QStringLiteral("%1 health changed from %2 to %3")
                    .arg(expectation.group)
                    .arg(initial)
                    .arg(current));
    }
    break;
  }
  case ArenaExpectationKind::GroupHealthReduced: {
    int const initial = initial_health_by_group.value(expectation.group, -1);
    int const current = group_health(expectation.group);
    int const required_drop = std::max(1, static_cast<int>(expectation.threshold));
    if (initial < 0 || current > initial - required_drop) {
      add_issue(QStringLiteral("group_health_not_reduced"),
                QStringLiteral("%1 health changed from %2 to %3; expected at "
                               "least %4 damage")
                    .arg(expectation.group)
                    .arg(initial)
                    .arg(current)
                    .arg(required_drop));
    }
    break;
  }
  case ArenaExpectationKind::StructureDamageCueObserved:
    if (!structure_damage_cues.value(expectation.group, false)) {
      add_issue(QStringLiteral("structure_damage_cue_not_observed"),
                QStringLiteral("%1 never exposed a localized facade damage cue")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::StructureFireObserved:
    if (!structure_fires.value(expectation.group, false)) {
      add_issue(QStringLiteral("structure_fire_not_observed"),
                QStringLiteral("%1 never caught fire").arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::StructureCollapseObserved:
    if (!structure_collapses.value(expectation.group, false)) {
      add_issue(QStringLiteral("structure_collapse_not_observed"),
                QStringLiteral("%1 never stood as a collapsing ruin; it must come "
                               "down on screen rather than vanish")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::StructureRepairObserved:
    if (!structure_repairs.value(expectation.group, false)) {
      add_issue(QStringLiteral("structure_repair_not_observed"),
                QStringLiteral("%1 never had repair scaffolding fully raised")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::StructureDismantleObserved:
    if (!structure_dismantles.value(expectation.group, false)) {
      add_issue(QStringLiteral("structure_dismantle_not_observed"),
                QStringLiteral("%1 was never taken half apart by a crew")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::NoStructureFireObserved:
    if (structure_fires.value(expectation.group, false)) {
      add_issue(QStringLiteral("unexpected_structure_fire"),
                QStringLiteral("%1 burned without taking incendiary damage")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::FlamingProjectileObserved: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (!flaming_projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("flaming_projectile_not_observed"),
                QStringLiteral("%1 never sent flaming shot at %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::NoFlamingProjectileObserved: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (flaming_projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("unexpected_flaming_projectile"),
                QStringLiteral("%1 sent flaming shot at %2")
                    .arg(expectation.group, expectation.target_group));
    } else if (!plain_projectile_flights.value(key, false)) {
      add_issue(QStringLiteral("projectile_flight_not_observed"),
                QStringLiteral("%1 never sent a shot at %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::StructureFacadeContactObserved: {
    auto const key = projectile_pair_key(expectation.group, expectation.target_group);
    if (!structure_facade_contacts.value(key, false)) {
      add_issue(QStringLiteral("structure_facade_contact_not_observed"),
                QStringLiteral("%1 never reached the visible facade of %2")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::FormationBodyOverlapObserved: {
    float const required_overlap =
        expectation.threshold > 0.0F ? expectation.threshold : 0.20F;
    auto const observed = minimum_formation_surface_gap.find(expectation.group);
    if (observed == minimum_formation_surface_gap.end() ||
        observed.value() > -required_overlap) {
      add_issue(QStringLiteral("formation_body_overlap_not_observed"),
                observed == minimum_formation_surface_gap.end()
                    ? QStringLiteral("%1 never established visible formation contact")
                          .arg(expectation.group)
                    : QStringLiteral("%1 reached only %2 m of body overlap (required "
                                     "%3 m)")
                          .arg(expectation.group)
                          .arg(-observed.value(), 0, 'f', 2)
                          .arg(required_overlap, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::DeathAnimationObserved:
    if (!visible_deaths.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_death"),
                QStringLiteral("%1 never produced a visible death animation")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::LaunchedCasualtyObserved:
    if (!launched_casualties.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_launched_casualty"),
                QStringLiteral("%1 never produced an impact-launched casualty")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::NoLaunchedCasualtyObserved:
    if (launched_casualties.value(expectation.group, false)) {
      add_issue(QStringLiteral("unexpected_launched_casualty"),
                QStringLiteral("%1 produced a launched casualty despite its "
                               "braced hold")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::ChargeImpactPrecedesMeleeLock:
    if (!charge_impacts.value(expectation.group, false)) {
      add_issue(QStringLiteral("charge_impact_not_observed"),
                QStringLiteral("%1 never entered its one-shot mounted charge "
                               "impact action")
                    .arg(expectation.group));
    } else if (!melee_locks_after_charge.value(expectation.group, false)) {
      add_issue(QStringLiteral("melee_did_not_follow_charge"),
                QStringLiteral("%1 did not enter ordinary melee lock after its "
                               "mounted charge impact")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::AttackRecoveryObserved:
    if (!visible_attack_recoveries.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_attack_recovery"),
                QStringLiteral("%1 never completed an attack and recovered to a "
                               "controlled movement or idle pose")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::NoActiveCombatAtEnd: {
    bool active_combat = false;
    for (auto entity_id : ids(expectation.group)) {
      auto* entity = world.get_entity(entity_id);
      if (entity == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
      auto const* target = entity->get_component<Engine::Core::AttackTargetComponent>();
      auto const* combat = entity->get_component<Engine::Core::CombatStateComponent>();
      auto const* action =
          entity->get_component<Engine::Core::RpgCommanderActionComponent>();
      auto const* presentation =
          entity->get_component<Engine::Core::CreaturePresentationComponent>();
      active_combat =
          active_combat || (attack != nullptr && attack->in_melee_lock) ||
          (target != nullptr && target->target_id != 0) ||
          (combat != nullptr &&
           combat->animation_state != Engine::Core::CombatAnimationState::Idle) ||
          (action != nullptr && (action->action_running || action->action_active)) ||
          (presentation != nullptr &&
           (presentation->is_attacking || presentation->combat_active));
    }
    if (active_combat) {
      add_issue(QStringLiteral("combat_continued_without_opponent"),
                QStringLiteral("%1 retained attack state at scenario end")
                    .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::HitReactionObserved:
    if (!visible_hit_reactions.value(expectation.group, false)) {
      add_issue(QStringLiteral("no_visible_hit_reaction"),
                QStringLiteral("%1 never produced a visible hit reaction")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::AllLivingSoldiersFight: {
    auto missing = living_soldiers_by_group.value(expectation.group);
    missing.subtract(attacking_soldiers_by_group.value(expectation.group));
    missing.subtract(guarding_soldiers_by_group.value(expectation.group));
    if (!missing.isEmpty()) {
      add_issue(QStringLiteral("soldiers_never_fought"),
                QStringLiteral("%1 had %2 living soldiers that never "
                               "produced a fight animation")
                    .arg(expectation.group)
                    .arg(missing.size()));
    }
    if (!engaged_soldiers_by_group.value(expectation.group).isEmpty() &&
        !staggered_attack_phases.value(expectation.group, false)) {
      add_issue(QStringLiteral("synchronized_formation_attacks"),
                QStringLiteral("%1 never produced visibly staggered per-soldier "
                               "fight phases")
                    .arg(expectation.group));
    }
    break;
  }
  case ArenaExpectationKind::TargetRetakenAfterDeath: {
    bool retaken = false;
    auto const& valid_targets = ids(expectation.target_group);
    for (auto entity_id : ids(expectation.group)) {
      auto const* target =
          world.try_get<Engine::Core::AttackTargetComponent>(entity_id);
      retaken = retaken || (target != nullptr &&
                            std::find(valid_targets.begin(),
                                      valid_targets.end(),
                                      target->target_id) != valid_targets.end());
    }
    if (!retaken) {
      add_issue(QStringLiteral("target_not_retaken"),
                QStringLiteral("%1 did not retarget %2 after its first target died")
                    .arg(expectation.group, expectation.target_group));
    }
    break;
  }
  case ArenaExpectationKind::BotIssuesUsefulCommand:
    if (!useful_bot_action.value(expectation.group, false)) {
      add_issue(QStringLiteral("bot_inactive"),
                QStringLiteral("AI group %1 never moved or acquired a target")
                    .arg(expectation.group));
    }
    break;
  case ArenaExpectationKind::RangeIndicatorObserved: {
    if (!range_ring_max_radius.contains(expectation.group)) {
      add_issue(QStringLiteral("range_indicator_missing"),
                QStringLiteral("%1 never produced a range indicator ring")
                    .arg(expectation.group));
      break;
    }
    float const observed = range_ring_max_radius.value(expectation.group, 0.0F);
    if (expectation.threshold > 0.0F &&
        std::abs(observed - expectation.threshold) > expectation.threshold * 0.02F) {
      add_issue(QStringLiteral("range_indicator_radius_mismatch"),
                QStringLiteral("%1 drew a %2 range ring, expected %3")
                    .arg(expectation.group)
                    .arg(observed, 0, 'f', 2)
                    .arg(expectation.threshold, 0, 'f', 2));
    }
    float const observed_min = range_ring_min_radius.value(expectation.group, 0.0F);
    if (expectation.distance > 0.0F &&
        std::abs(observed_min - expectation.distance) > expectation.distance * 0.02F) {
      add_issue(QStringLiteral("range_indicator_min_radius_mismatch"),
                QStringLiteral("%1 drew a %2 minimum range ring, expected %3")
                    .arg(expectation.group)
                    .arg(observed_min, 0, 'f', 2)
                    .arg(expectation.distance, 0, 'f', 2));
    }
    break;
  }
  case ArenaExpectationKind::RangeIndicatorCountAtMost:
    if (expectation.threshold > 0.0F &&
        static_cast<float>(max_range_ring_count) > expectation.threshold) {
      add_issue(QStringLiteral("range_indicator_count_exceeded"),
                QStringLiteral("range indicators peaked at %1 rings, cap is %2")
                    .arg(max_range_ring_count)
                    .arg(static_cast<int>(expectation.threshold)));
    }
    break;
  default:
    break;
  }
}

} // namespace Arena
