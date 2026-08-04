#include "auto_engagement.h"

#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "combat_types.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

namespace {

auto besieging_structure(Engine::Core::Entity* unit,
                         const CombatQueryContext& query_context) -> bool {
  if (!unit->has_component<Engine::Core::AIControlledComponent>()) {
    return false;
  }

  auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target == nullptr || attack_target->target_id == 0) {
    return false;
  }

  auto it = query_context.entities_by_id.find(attack_target->target_id);
  if (it == query_context.entities_by_id.end()) {
    return false;
  }

  return is_building(it->second);
}

} // namespace

void AutoEngagement::process(Engine::Core::World*,
                             const CombatQueryContext& query_context,
                             float delta_time) {
  for (auto it = m_engagement_cooldowns.begin(); it != m_engagement_cooldowns.end();) {
    it->second -= delta_time;
    if (it->second <= 0.0F) {
      it = m_engagement_cooldowns.erase(it);
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

    if (unit->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    if (unit->has_component<Engine::Core::WildlifeComponent>()) {
      continue;
    }

    auto* attack_comp = unit->get_component<Engine::Core::AttackComponent>();
    if (attack_comp == nullptr) {
      continue;
    }

    bool const shoots_without_closing =
        attack_comp->can_ranged &&
        attack_comp->preferred_mode != Engine::Core::AttackComponent::CombatMode::Melee;

    if (!shoots_without_closing) {
      if (!attack_comp->can_melee || !should_auto_engage_melee(unit)) {
        continue;
      }
    }

    if (suppresses_opportunistic_combat(unit)) {
      continue;
    }

    if (m_engagement_cooldowns.find(unit->get_id()) != m_engagement_cooldowns.end()) {
      continue;
    }

    auto* guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
    bool const in_guard_mode = (guard_mode != nullptr) && guard_mode->active;

    bool const siege_retarget = besieging_structure(unit, query_context);

    if (!in_guard_mode && !siege_retarget && !is_unit_idle(unit)) {
      continue;
    }

    float detection_range =
        shoots_without_closing ? attack_comp->range : unit_comp->vision_range;
    if (in_guard_mode) {
      detection_range = std::min(detection_range, guard_mode->guard_radius);
    }

    auto* nearest_enemy = find_nearest_enemy(unit, query_context, detection_range);

    if (nearest_enemy != nullptr) {
      auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
      if (attack_target == nullptr) {
        attack_target = unit->add_component<Engine::Core::AttackTargetComponent>();
      }
      if (attack_target != nullptr) {
        attack_target->target_id = nearest_enemy->get_id();
        attack_target->should_chase = !in_guard_mode && !shoots_without_closing;

        m_engagement_cooldowns[unit->get_id()] = Constants::k_engagement_cooldown;
      }
    }
  }
}

} // namespace Game::Systems::Combat
