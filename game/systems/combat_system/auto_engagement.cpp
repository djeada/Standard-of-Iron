#include "auto_engagement.h"

#include <algorithm>
#include <cmath>

#include "../../core/component_economy.h"
#include "../../core/world.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "engagement_trace.h"
#include "threat_alert.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_commander_self_defence_radius = 7.0F;

constexpr float k_quiet_rescan_interval = 0.4F;

auto besieging_structure(Engine::Core::Entity* unit,
                         const CombatQueryContext& query_context) -> bool {
  if (!unit->has_component<Engine::Core::AIControlledComponent>()) {
    return false;
  }

  auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target == nullptr || attack_target->target_id == 0) {
    return false;
  }

  const CandidateRecord* record = query_context.find_record(attack_target->target_id);
  if (record == nullptr) {
    return false;
  }

  return record->is_building;
}

auto quiet_rescan_delay(Engine::Core::EntityID unit_id) -> float {
  float const phase = static_cast<float>(unit_id % 16U) / 16.0F;
  return k_quiet_rescan_interval * (0.75F + (phase * 0.5F));
}

auto current_target_of(Engine::Core::Entity* unit) -> Engine::Core::EntityID {
  auto const* target = unit->get_component<Engine::Core::AttackTargetComponent>();
  return target == nullptr ? 0U : target->target_id;
}

void trace(Engine::Core::Entity* unit,
           EngagementOutcome outcome,
           Engine::Core::EntityID candidate_id = 0,
           Engine::Core::EntityID target_id = 0,
           float range = 0.0F) {
  if (!EngagementTrace::instance().enabled()) {
    return;
  }
  note_engagement(unit,
                  {.candidate_id = candidate_id,
                   .target_id = target_id,
                   .acquisition_range = range,
                   .outcome = outcome,
                   .source = command_source_of(unit)});
}

} // namespace

void AutoEngagement::process(Engine::Core::World* world,
                             const CombatQueryContext& query_context,
                             float delta_time) {
  for (auto it = m_scan_cooldowns.begin(); it != m_scan_cooldowns.end();) {
    it->second -= delta_time;
    if (it->second <= 0.0F) {
      it = m_scan_cooldowns.erase(it);
    } else {
      ++it;
    }
  }

  for (auto* unit : query_context.units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp == nullptr) || unit_comp->health <= 0) {
      continue;
    }

    if (!auto_acquires_targets(unit)) {
      trace(unit, EngagementOutcome::NoCombatRole);
      continue;
    }

    if (suppresses_opportunistic_combat(unit)) {
      trace(unit, EngagementOutcome::Suppressed, 0, current_target_of(unit));
      continue;
    }

    if (m_scan_cooldowns.find(unit->get_id()) != m_scan_cooldowns.end()) {
      continue;
    }

    auto* guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
    bool const in_guard_mode = (guard_mode != nullptr) && guard_mode->active;

    bool const siege_retarget = besieging_structure(unit, query_context);

    auto const* order_intent =
        unit->get_component<Engine::Core::PlayerOrderIntentComponent>();
    bool const attack_move_active =
        (order_intent != nullptr) &&
        order_intent->kind == Engine::Core::PlayerOrderIntentKind::AttackMove;

    if (!in_guard_mode && !siege_retarget && !attack_move_active &&
        !is_unit_idle(unit)) {
      auto const held = current_target_of(unit);
      trace(unit,
            held != 0 ? EngagementOutcome::HoldingTarget : EngagementOutcome::Busy,
            0,
            held);
      continue;
    }

    float detection_range = acquisition_range(unit);
    if (in_guard_mode) {
      detection_range = std::min(detection_range, guard_mode->guard_radius);
    }
    if (unit->has_component<Engine::Core::CommanderComponent>()) {
      detection_range = std::min(detection_range, k_commander_self_defence_radius);
    }

    auto const reachable = [unit](Engine::Core::Entity* candidate) {
      return may_engage(unit, candidate, EngagementTrigger::Opportunity);
    };
    Engine::Core::Entity* nearest_candidate = nullptr;
    auto* nearest_enemy = find_nearest_enemy(
        unit, query_context, detection_range, nullptr, reachable, &nearest_candidate);

    Engine::Core::EntityID const candidate_id =
        nearest_candidate != nullptr ? nearest_candidate->get_id() : 0U;

    if (nearest_enemy == nullptr) {
      m_scan_cooldowns[unit->get_id()] = quiet_rescan_delay(unit->get_id());
      trace(unit,
            candidate_id == 0 ? EngagementOutcome::NoCandidateInRange
                              : EngagementOutcome::CandidateUnreachable,
            candidate_id,
            0,
            detection_range);
      continue;
    }

    auto* attack_target =
        Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(unit);
    if (attack_target == nullptr) {
      continue;
    }

    attack_target->target_id = nearest_enemy->get_id();

    attack_target->should_chase = pursues_targets(unit) &&
                                  !opens_fire_without_closing(unit) &&
                                  !is_unit_in_hold_mode(unit);
    attack_target->is_player_command = false;

    m_scan_cooldowns[unit->get_id()] = Constants::k_engagement_cooldown;
    trace(unit,
          EngagementOutcome::Engaged,
          candidate_id,
          nearest_enemy->get_id(),
          detection_range);
    note_threat(world,
                unit,
                nearest_enemy,
                Engine::Core::ThreatAlertComponent::Kind::EnemySighted);
  }
}

} // namespace Game::Systems::Combat
