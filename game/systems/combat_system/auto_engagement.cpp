#include "auto_engagement.h"

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "combat_types.h"
#include "combat_utils.h"
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

    if (!auto_acquires_targets(unit)) {
      continue;
    }

    if (suppresses_opportunistic_combat(unit)) {
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
    auto* nearest_enemy =
        find_nearest_enemy(unit, query_context, detection_range, nullptr, reachable);

    if (nearest_enemy == nullptr) {
      m_scan_cooldowns[unit->get_id()] = quiet_rescan_delay(unit->get_id());
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

    m_scan_cooldowns[unit->get_id()] = Constants::k_engagement_cooldown;
    note_threat(world,
                unit,
                nearest_enemy,
                Engine::Core::ThreatAlertComponent::Kind::EnemySighted);
  }
}

} // namespace Game::Systems::Combat
