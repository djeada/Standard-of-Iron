#include "combat_mode_processor.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../owner_registry.h"

#include <cmath>
#include <limits>

namespace Game::Systems::Combat {

void update_combat_mode(Engine::Core::Entity *attacker,
                        Engine::Core::World *world,
                        Engine::Core::AttackComponent *attack_comp) {
  if (attack_comp == nullptr) {
    return;
  }

  if (attack_comp->preferred_mode !=
      Engine::Core::AttackComponent::CombatMode::Auto) {
    attack_comp->current_mode = attack_comp->preferred_mode;
    return;
  }

  // Only update combat mode when already engaged in combat
  // This prevents triggering attack mode when just moving near enemies
  bool const in_melee_combat = attack_comp->in_melee_lock;
  bool const has_attack_target = 
      attacker->has_component<Engine::Core::AttackTargetComponent>();
  
  if (!in_melee_combat && !has_attack_target) {
    // Not engaged in combat, keep default mode
    if (attack_comp->can_ranged) {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr) {
    return;
  }

  auto *attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  float closest_enemy_dist_sq = std::numeric_limits<float>::max();
  float closest_height_diff = 0.0F;

  for (auto *target : units) {
    if (target == attacker) {
      continue;
    }

    auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (owner_registry.are_allies(attacker_unit->owner_id,
                                  target_unit->owner_id)) {
      continue;
    }

    // Exclude buildings from combat mode calculation
    if (target->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx =
        target_transform->position.x - attacker_transform->position.x;
    float const dz =
        target_transform->position.z - attacker_transform->position.z;
    float const dy =
        target_transform->position.y - attacker_transform->position.y;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < closest_enemy_dist_sq) {
      closest_enemy_dist_sq = dist_sq;
      closest_height_diff = std::abs(dy);
    }
  }

  if (closest_enemy_dist_sq == std::numeric_limits<float>::max()) {
    if (attack_comp->can_ranged) {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->current_mode =
          Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  float const closest_dist = std::sqrt(closest_enemy_dist_sq);

  bool const in_melee_range =
      attack_comp->is_in_melee_range(closest_dist, closest_height_diff);
  bool const in_ranged_range = attack_comp->is_in_ranged_range(closest_dist);

  if (in_melee_range && attack_comp->can_melee) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Melee;
  } else if (in_ranged_range && attack_comp->can_ranged) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack_comp->can_ranged) {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Ranged;
  } else {
    attack_comp->current_mode =
        Engine::Core::AttackComponent::CombatMode::Melee;
  }
}

} // namespace Game::Systems::Combat
