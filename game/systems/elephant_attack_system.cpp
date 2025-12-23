#include "elephant_attack_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include <cmath>
#include <vector>

namespace Game::Systems {

void ElephantAttackSystem::update(Engine::Core::World *world,
                                  float delta_time) {
  process_elephant_behavior(world, delta_time);
}

void ElephantAttackSystem::process_elephant_behavior(Engine::Core::World *world,
                                                     float delta_time) {
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *entity : entities) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    if (unit->spawn_type != Game::Units::SpawnType::Elephant) {
      continue;
    }

    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *elephant = entity->get_component<Engine::Core::ElephantComponent>();
    if (elephant == nullptr) {
      elephant = entity->add_component<Engine::Core::ElephantComponent>();
    }

    // Check for panic condition (health below 30%)
    float const health_ratio =
        static_cast<float>(unit->health) / static_cast<float>(unit->max_health);
    if (health_ratio < 0.3F && !elephant->is_panicked) {
      // 50% chance to panic when health drops below 30%
      if (std::rand() % 100 < 50) {
        elephant->is_panicked = true;
        elephant->panic_duration = 10.0F; // Panic for 10 seconds
      }
    }

    // Process panic mechanic
    if (elephant->is_panicked) {
      process_panic_mechanic(entity, world, delta_time);
    }

    // Update charge cooldown
    if (elephant->charge_cooldown > 0.0F) {
      elephant->charge_cooldown -= delta_time;
    }

    // Process charge attack
    process_charge_attack(entity, world, delta_time);

    // Process trample damage
    process_trample_damage(entity, world, delta_time);

    // Process melee attack
    process_melee_attack(entity, world, delta_time);
  }
}

void ElephantAttackSystem::process_charge_attack(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  auto *unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto *transform = elephant->get_component<Engine::Core::TransformComponent>();
  auto *movement = elephant->get_component<Engine::Core::MovementComponent>();
  auto *attack_target =
      elephant->get_component<Engine::Core::AttackTargetComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr ||
      movement == nullptr) {
    return;
  }

  // Handle charge state
  switch (elephant_comp->charge_state) {
  case Engine::Core::ElephantComponent::ChargeState::Idle: {
    // Check if we should initiate a charge
    if (attack_target != nullptr && attack_target->target_id != 0 &&
        elephant_comp->charge_cooldown <= 0.0F && !elephant_comp->is_panicked) {
      auto *target = world->get_entity(attack_target->target_id);
      if (target != nullptr) {
        auto *target_transform =
            target->get_component<Engine::Core::TransformComponent>();
        if (target_transform != nullptr) {
          float const dx =
              target_transform->position.x - transform->position.x;
          float const dz =
              target_transform->position.z - transform->position.z;
          float const dist = std::sqrt(dx * dx + dz * dz);

          // Initiate charge if target is at medium distance (5-15 units)
          if (dist >= 5.0F && dist <= 15.0F) {
            elephant_comp->charge_state =
                Engine::Core::ElephantComponent::ChargeState::Charging;
            elephant_comp->charge_duration = 3.0F; // Charge for 3 seconds
          }
        }
      }
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Charging: {
    // Apply charge speed multiplier
    elephant_comp->charge_duration -= delta_time;

    if (elephant_comp->charge_duration <= 0.0F) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Recovering;
      elephant_comp->charge_cooldown = 8.0F; // 8 second cooldown
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Recovering: {
    elephant_comp->charge_state =
        Engine::Core::ElephantComponent::ChargeState::Idle;
    break;
  }

  default:
    break;
  }
}

void ElephantAttackSystem::process_trample_damage(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  auto *unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto *transform = elephant->get_component<Engine::Core::TransformComponent>();
  auto *movement = elephant->get_component<Engine::Core::MovementComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr ||
      movement == nullptr) {
    return;
  }

  // Only apply trample damage when moving
  constexpr float k_movement_threshold = 0.1F;
  bool const is_moving = (std::abs(movement->vx) > k_movement_threshold ||
                          std::abs(movement->vz) > k_movement_threshold);

  if (!is_moving) {
    return;
  }

  // Find nearby enemy units and apply trample damage
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *other_entity : entities) {
    if (other_entity == elephant) {
      continue;
    }

    auto *other_unit = other_entity->get_component<Engine::Core::UnitComponent>();
    auto *other_transform =
        other_entity->get_component<Engine::Core::TransformComponent>();

    if (other_unit == nullptr || other_transform == nullptr ||
        other_unit->health <= 0) {
      continue;
    }

    // Check if this is an enemy unit (or friendly if panicked)
    bool const is_enemy = other_unit->owner_id != unit->owner_id;
    bool const should_damage = is_enemy || elephant_comp->is_panicked;

    if (!should_damage) {
      continue;
    }

    // Calculate distance
    float const dx = other_transform->position.x - transform->position.x;
    float const dz = other_transform->position.z - transform->position.z;
    float const dist = std::sqrt(dx * dx + dz * dz);

    // Apply trample damage if within radius
    if (dist <= elephant_comp->trample_radius) {
      other_unit->health -= static_cast<int>(
          static_cast<float>(elephant_comp->trample_damage) * delta_time);
      if (other_unit->health < 0) {
        other_unit->health = 0;
      }
    }
  }
}

void ElephantAttackSystem::process_panic_mechanic(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  auto *movement = elephant->get_component<Engine::Core::MovementComponent>();

  if (elephant_comp == nullptr || movement == nullptr) {
    return;
  }

  elephant_comp->panic_duration -= delta_time;

  if (elephant_comp->panic_duration <= 0.0F) {
    elephant_comp->is_panicked = false;
    elephant_comp->panic_duration = 0.0F;
    return;
  }

  // Make the elephant move in random directions while panicked
  // The movement system will handle the actual movement
  // We just set random targets periodically
  static float random_target_timer = 0.0F;
  random_target_timer += delta_time;

  if (random_target_timer >= 2.0F) {
    random_target_timer = 0.0F;

    auto *transform =
        elephant->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      // Generate random direction
      float const angle = static_cast<float>(std::rand()) /
                          static_cast<float>(RAND_MAX) * 3.14159F * 2.0F;
      float const distance = 10.0F;

      movement->target_x = transform->position.x + std::cos(angle) * distance;
      movement->target_y = transform->position.z + std::sin(angle) * distance;
      movement->has_target = true;
    }
  }
}

void ElephantAttackSystem::process_melee_attack(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  // The standard combat system handles melee attacks
  // This is here for future expansion if needed
}

} // namespace Game::Systems
