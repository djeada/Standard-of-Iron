#include "idle_behavior_system.h"
#include "../core/component.h"
#include "../core/world.h"

namespace Game::Systems {

namespace {
constexpr float kMinMovementSpeedSq = 0.01F;
constexpr float kGroupIdleSearchRadius = 5.0F;
constexpr float kGroupIdleSearchRadiusSq =
    kGroupIdleSearchRadius * kGroupIdleSearchRadius;

/// Check if a unit is currently moving
[[nodiscard]] inline auto
is_unit_moving(const Engine::Core::MovementComponent *movement) noexcept
    -> bool {
  if (movement == nullptr) {
    return false;
  }
  const float speed_sq =
      movement->vx * movement->vx + movement->vz * movement->vz;
  return speed_sq > kMinMovementSpeedSq;
}

/// Check if a unit is in combat or has an attack target
[[nodiscard]] inline auto
is_unit_in_combat(const Engine::Core::Entity *entity) noexcept -> bool {
  const auto *attack_target =
      entity->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target != nullptr && attack_target->target_id != 0) {
    return true;
  }
  const auto *combat_state =
      entity->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr &&
      combat_state->animation_state !=
          Engine::Core::CombatAnimationState::Idle) {
    return true;
  }
  return false;
}

/// Select a micro idle animation type based on personality and timer
[[nodiscard]] inline auto select_micro_idle_type(std::uint8_t personality_seed,
                                                 float timer) noexcept
    -> Engine::Core::IdleAnimationType {
  // Use combination of personality and timer for variety
  const auto variant = static_cast<std::uint8_t>(
      (personality_seed + static_cast<std::uint8_t>(timer * 10.0F)) %
      Engine::Core::IdleBehaviorComponent::kMaxMicroIdleVariants);
  switch (variant) {
  case 0:
    return Engine::Core::IdleAnimationType::WeightShift;
  case 1:
    return Engine::Core::IdleAnimationType::Breathing;
  case 2:
    return Engine::Core::IdleAnimationType::HeadTurn;
  case 3:
    return Engine::Core::IdleAnimationType::FootAdjust;
  case 4:
    return Engine::Core::IdleAnimationType::GripAdjust;
  default:
    return Engine::Core::IdleAnimationType::WeightShift;
  }
}

/// Select an ambient idle animation type based on personality and cooldown
[[nodiscard]] inline auto
select_ambient_idle_type(std::uint8_t personality_seed,
                         float cooldown) noexcept
    -> Engine::Core::IdleAnimationType {
  const auto variant = static_cast<std::uint8_t>(
      (personality_seed + static_cast<std::uint8_t>(cooldown * 7.0F)) %
      Engine::Core::IdleBehaviorComponent::kMaxAmbientIdleVariants);
  switch (variant) {
  case 0:
    return Engine::Core::IdleAnimationType::CheckWeapon;
  case 1:
    return Engine::Core::IdleAnimationType::KneelRest;
  case 2:
    return Engine::Core::IdleAnimationType::StretchShoulders;
  case 3:
    return Engine::Core::IdleAnimationType::AdjustHelmet;
  case 4:
    return Engine::Core::IdleAnimationType::Yawn;
  case 5:
    return Engine::Core::IdleAnimationType::Sigh;
  default:
    return Engine::Core::IdleAnimationType::CheckWeapon;
  }
}

/// Update micro idle state (always-on subtle movements)
void update_micro_idles(Engine::Core::IdleBehaviorComponent *idle,
                        float delta_time) {
  if (!idle->micro_idles_enabled) {
    idle->current_micro_idle = Engine::Core::IdleAnimationType::None;
    return;
  }

  idle->micro_idle_timer += delta_time;

  // Add random offset to prevent synchronization across units
  const float adjusted_interval =
      idle->micro_idle_interval + idle->random_offset;

  if (idle->micro_idle_timer >= adjusted_interval) {
    idle->micro_idle_timer = 0.0F;
    idle->micro_idle_variant =
        (idle->micro_idle_variant + 1) %
        Engine::Core::IdleBehaviorComponent::kMaxMicroIdleVariants;
    idle->current_micro_idle =
        select_micro_idle_type(idle->personality_seed, idle->idle_time);
  }

  // Update micro idle animation phase (0.0 to 1.0)
  idle->micro_idle_phase = idle->micro_idle_timer / adjusted_interval;
}

/// Update ambient idle state (occasional personality-driven actions)
void update_ambient_idles(Engine::Core::IdleBehaviorComponent *idle,
                          float delta_time) {
  if (!idle->ambient_idles_enabled) {
    idle->ambient_idle_active = false;
    idle->current_ambient_idle = Engine::Core::IdleAnimationType::None;
    return;
  }

  // If already performing ambient idle, update animation time
  if (idle->ambient_idle_active) {
    idle->ambient_animation_time += delta_time;
    if (idle->ambient_animation_time >= idle->ambient_animation_duration) {
      // Animation complete
      idle->ambient_idle_active = false;
      idle->current_ambient_idle = Engine::Core::IdleAnimationType::None;
      idle->ambient_animation_time = 0.0F;
      idle->ambient_idle_cooldown =
          Engine::Core::IdleBehaviorComponent::kDefaultAmbientIdleCooldown;
    }
    return;
  }

  // Update cooldown
  if (idle->ambient_idle_cooldown > 0.0F) {
    idle->ambient_idle_cooldown -= delta_time;
    return;
  }

  // Check if idle long enough to trigger ambient idle
  if (idle->idle_time >= idle->ambient_idle_threshold) {
    // Probability-based trigger using personality seed
    const auto trigger_value = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(idle->idle_time * 100.0F) %
        idle->personality_seed);
    if (trigger_value < 10) { // ~10% chance per check
      idle->ambient_idle_active = true;
      idle->current_ambient_idle = select_ambient_idle_type(
          idle->personality_seed, idle->ambient_idle_cooldown);
      idle->ambient_animation_time = 0.0F;
    }
  }
}

/// Find a nearby idle unit for group idle interaction
[[nodiscard]] auto find_group_idle_partner(
    Engine::Core::World *world, Engine::Core::Entity *entity,
    const Engine::Core::TransformComponent *transform,
    const Engine::Core::UnitComponent *unit) -> Engine::Core::EntityID {
  if (world == nullptr || transform == nullptr || unit == nullptr) {
    return 0;
  }

  const auto entities =
      world->get_entities_with<Engine::Core::IdleBehaviorComponent>();

  for (auto *other : entities) {
    if (other == entity) {
      continue;
    }

    const auto *other_unit =
        other->get_component<Engine::Core::UnitComponent>();
    if (other_unit == nullptr || other_unit->owner_id != unit->owner_id) {
      continue; // Only same-team units
    }

    const auto *other_idle =
        other->get_component<Engine::Core::IdleBehaviorComponent>();
    if (other_idle == nullptr || !other_idle->is_idle ||
        other_idle->group_idle_active) {
      continue; // Partner must be idle and not in group idle
    }

    const auto *other_transform =
        other->get_component<Engine::Core::TransformComponent>();
    if (other_transform == nullptr) {
      continue;
    }

    // Check distance
    const float dx = other_transform->position.x - transform->position.x;
    const float dz = other_transform->position.z - transform->position.z;
    const float dist_sq = dx * dx + dz * dz;

    if (dist_sq <= kGroupIdleSearchRadiusSq) {
      return other->get_id();
    }
  }

  return 0;
}

/// Update group idle state (rare contextual interactions)
void update_group_idles(Engine::Core::World *world, Engine::Core::Entity *entity,
                        Engine::Core::IdleBehaviorComponent *idle,
                        const Engine::Core::TransformComponent *transform,
                        const Engine::Core::UnitComponent *unit,
                        float delta_time) {
  if (!idle->group_idles_enabled) {
    idle->group_idle_active = false;
    idle->current_group_idle = Engine::Core::IdleAnimationType::None;
    idle->group_partner_id = 0;
    return;
  }

  // If already in group idle, check if partner is still valid
  if (idle->group_idle_active) {
    if (idle->group_partner_id != 0) {
      auto *partner = world->get_entity(idle->group_partner_id);
      if (partner == nullptr) {
        // Partner no longer exists
        idle->group_idle_active = false;
        idle->current_group_idle = Engine::Core::IdleAnimationType::None;
        idle->group_partner_id = 0;
        idle->is_group_idle_initiator = false;
        idle->group_idle_cooldown =
            Engine::Core::IdleBehaviorComponent::kDefaultGroupIdleCooldown;
      }
    }
    return;
  }

  // Update cooldown
  if (idle->group_idle_cooldown > 0.0F) {
    idle->group_idle_cooldown -= delta_time;
    return;
  }

  // Check if idle long enough for group idle
  if (idle->idle_time >=
      idle->ambient_idle_threshold * 2.0F) { // Longer threshold for group
    // Low probability trigger
    const auto trigger_value = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>(idle->idle_time * 50.0F) %
        idle->personality_seed);
    if (trigger_value < 5) { // ~5% chance per check
      // Find a partner
      const auto partner_id =
          find_group_idle_partner(world, entity, transform, unit);
      if (partner_id != 0) {
        idle->group_idle_active = true;
        idle->group_partner_id = partner_id;
        idle->is_group_idle_initiator = true;

        // Select group idle type
        const auto group_variant = (idle->personality_seed % 3);
        switch (group_variant) {
        case 0:
          idle->current_group_idle =
              Engine::Core::IdleAnimationType::TalkingPair;
          break;
        case 1:
          idle->current_group_idle =
              Engine::Core::IdleAnimationType::PointAndNod;
          break;
        case 2:
          idle->current_group_idle =
              Engine::Core::IdleAnimationType::SharedLaugh;
          break;
        default:
          idle->current_group_idle =
              Engine::Core::IdleAnimationType::TalkingPair;
          break;
        }

        // Notify partner
        auto *partner = world->get_entity(partner_id);
        if (partner != nullptr) {
          auto *partner_idle =
              partner->get_component<Engine::Core::IdleBehaviorComponent>();
          if (partner_idle != nullptr) {
            partner_idle->group_idle_active = true;
            partner_idle->group_partner_id = entity->get_id();
            partner_idle->current_group_idle = idle->current_group_idle;
            partner_idle->is_group_idle_initiator = false;
          }
        }
      }
    }
  }
}

} // namespace

void IdleBehaviorSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto *entity :
       world->get_entities_with<Engine::Core::IdleBehaviorComponent>()) {
    auto *idle = entity->get_component<Engine::Core::IdleBehaviorComponent>();
    if (idle == nullptr) {
      continue;
    }

    const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      idle->interrupt();
      continue;
    }

    const auto *transform =
        entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    const auto *movement =
        entity->get_component<Engine::Core::MovementComponent>();

    // Determine if unit is currently idle
    const bool is_moving = is_unit_moving(movement);
    const bool in_combat = is_unit_in_combat(entity);
    const bool has_movement_target =
        movement != nullptr && movement->has_target;

    // Unit is idle if not moving, not in combat, and has no movement target
    const bool now_idle = !is_moving && !in_combat && !has_movement_target;

    if (now_idle) {
      if (!idle->is_idle) {
        // Just became idle - initialize random offset
        idle->initialize_random_offset(entity->get_id());
      }
      idle->is_idle = true;
      idle->idle_time += delta_time;
      idle->time_since_last_action += delta_time;

      // Update idle behavior layers
      update_micro_idles(idle, delta_time);
      update_ambient_idles(idle, delta_time);
      update_group_idles(world, entity, idle, transform, unit, delta_time);
    } else {
      // Unit is no longer idle - interrupt all idle behaviors
      if (idle->is_idle) {
        idle->interrupt();
      }
      idle->is_idle = false;
    }
  }
}

} // namespace Game::Systems
