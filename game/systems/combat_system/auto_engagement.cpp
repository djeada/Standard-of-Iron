#include "auto_engagement.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "combat_types.h"
#include "combat_utils.h"

#include <cmath>

namespace Game::Systems::Combat {

void AutoEngagement::process(Engine::Core::World *world, float delta_time) {
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto it = m_engagement_cooldowns.begin();
       it != m_engagement_cooldowns.end();) {
    it->second -= delta_time;
    if (it->second <= 0.0F) {
      it = m_engagement_cooldowns.erase(it);
    } else {
      ++it;
    }
  }

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp == nullptr) || unit_comp->health <= 0) {
      continue;
    }

    if (unit->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *attack_comp = unit->get_component<Engine::Core::AttackComponent>();
    if ((attack_comp == nullptr) || !attack_comp->can_melee) {
      continue;
    }

    if (attack_comp->can_ranged &&
        attack_comp->preferred_mode !=
            Engine::Core::AttackComponent::CombatMode::Melee) {
      continue;
    }

    if (!should_auto_engage_melee(unit)) {
      continue;
    }

    if (m_engagement_cooldowns.find(unit->get_id()) !=
        m_engagement_cooldowns.end()) {
      continue;
    }

    auto *guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
    bool const in_guard_mode = (guard_mode != nullptr) && guard_mode->active;

    if (!in_guard_mode && !is_unit_idle(unit)) {
      continue;
    }

    float detection_range = unit_comp->vision_range;
    if (in_guard_mode) {
      detection_range = std::min(detection_range, guard_mode->guard_radius);
    }

    auto *nearest_enemy = find_nearest_enemy(unit, world, detection_range);

    if (nearest_enemy != nullptr) {
      auto *attack_target =
          unit->get_component<Engine::Core::AttackTargetComponent>();
      if (attack_target == nullptr) {
        attack_target =
            unit->add_component<Engine::Core::AttackTargetComponent>();
      }
      if (attack_target != nullptr) {
        attack_target->target_id = nearest_enemy->get_id();
        attack_target->should_chase = !in_guard_mode;

        m_engagement_cooldowns[unit->get_id()] = Constants::kEngagementCooldown;
      }
    }
  }
}

} // namespace Game::Systems::Combat
