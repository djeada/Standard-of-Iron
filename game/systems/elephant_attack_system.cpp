#include "elephant_attack_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include <cmath>
#include <random>

namespace Game::Systems {

void ElephantAttackSystem::update(Engine::Core::World *world,
                                  float delta_time) {
  process_elephant_attacks(world, delta_time);
}

void ElephantAttackSystem::process_elephant_attacks(Engine::Core::World *world,
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

    process_panic_behavior(entity, world, delta_time);

    if (!elephant->is_panicked) {
      process_charge_mechanics(entity, world, delta_time);
      process_trample_damage(entity, world, delta_time);
    }
  }
}

void ElephantAttackSystem::process_charge_mechanics(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp =
      elephant->get_component<Engine::Core::ElephantComponent>();
  auto *unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto *movement = elephant->get_component<Engine::Core::MovementComponent>();
  auto *attack_target =
      elephant->get_component<Engine::Core::AttackTargetComponent>();

  if (elephant_comp == nullptr || unit == nullptr || movement == nullptr) {
    return;
  }

  switch (elephant_comp->charge_state) {
  case Engine::Core::ElephantComponent::ChargeState::Idle: {
    if (elephant_comp->charge_cooldown > 0.0F) {
      elephant_comp->charge_cooldown -= delta_time;
      break;
    }

    if (attack_target != nullptr && attack_target->target_id != 0) {
      auto *target = world->get_entity(attack_target->target_id);
      if (target != nullptr &&
          !target->has_component<Engine::Core::PendingRemovalComponent>()) {
        auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
        auto *transform =
            elephant->get_component<Engine::Core::TransformComponent>();
        auto *target_transform =
            target->get_component<Engine::Core::TransformComponent>();

        if (target_unit != nullptr && target_unit->health > 0 &&
            transform != nullptr && target_transform != nullptr) {
          float const dx =
              target_transform->position.x - transform->position.x;
          float const dz =
              target_transform->position.z - transform->position.z;
          float const dist = std::sqrt(dx * dx + dz * dz);

          constexpr float k_charge_trigger_distance = 8.0F;
          if (dist > 3.5F && dist < k_charge_trigger_distance) {
            elephant_comp->charge_state =
                Engine::Core::ElephantComponent::ChargeState::Charging;
            elephant_comp->charge_duration = 0.0F;
          }
        }
      }
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Charging: {
    elephant_comp->charge_duration += delta_time;

    constexpr float k_max_charge_duration = 3.0F;
    if (elephant_comp->charge_duration >= k_max_charge_duration) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Recovering;
      elephant_comp->charge_duration = 0.0F;
      elephant_comp->charge_cooldown = 8.0F;
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Trampling: {
    elephant_comp->charge_duration += delta_time;

    constexpr float k_trample_duration = 0.5F;
    if (elephant_comp->charge_duration >= k_trample_duration) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Recovering;
      elephant_comp->charge_duration = 0.0F;
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Recovering: {
    elephant_comp->charge_duration += delta_time;

    constexpr float k_recovery_duration = 1.5F;
    if (elephant_comp->charge_duration >= k_recovery_duration) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Idle;
      elephant_comp->charge_duration = 0.0F;
    }
    break;
  }
  }
}

void ElephantAttackSystem::process_trample_damage(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp =
      elephant->get_component<Engine::Core::ElephantComponent>();
  auto *unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto *transform =
      elephant->get_component<Engine::Core::TransformComponent>();
  auto *movement = elephant->get_component<Engine::Core::MovementComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr ||
      movement == nullptr) {
    return;
  }

  if (elephant_comp->charge_state !=
      Engine::Core::ElephantComponent::ChargeState::Charging) {
    return;
  }

  constexpr float k_movement_threshold = 0.1F;
  bool is_moving = (std::abs(movement->vx) > k_movement_threshold ||
                    std::abs(movement->vz) > k_movement_threshold);

  if (!is_moving) {
    return;
  }

  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : entities) {
    if (entity == elephant) {
      continue;
    }

    auto *target_unit = entity->get_component<Engine::Core::UnitComponent>();
    auto *target_transform =
        entity->get_component<Engine::Core::TransformComponent>();

    if (target_unit == nullptr || target_unit->health <= 0 ||
        target_transform == nullptr) {
      continue;
    }

    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    if (target_unit->owner_id == unit->owner_id && !elephant_comp->is_panicked) {
      continue;
    }

    float const dx = target_transform->position.x - transform->position.x;
    float const dz = target_transform->position.z - transform->position.z;
    float const dist = std::sqrt(dx * dx + dz * dz);

    if (dist <= elephant_comp->trample_radius) {
      target_unit->health -= elephant_comp->trample_damage;

      if (target_unit->health <= 0) {
        target_unit->health = 0;
      }
    }
  }
}

void ElephantAttackSystem::process_panic_behavior(
    Engine::Core::Entity *elephant, Engine::Core::World *world,
    float delta_time) {
  auto *elephant_comp =
      elephant->get_component<Engine::Core::ElephantComponent>();
  auto *unit = elephant->get_component<Engine::Core::UnitComponent>();

  if (elephant_comp == nullptr || unit == nullptr) {
    return;
  }

  float const health_ratio =
      static_cast<float>(unit->health) / static_cast<float>(unit->max_health);

  constexpr float k_panic_threshold = 0.3F;

  if (!elephant_comp->is_panicked && health_ratio < k_panic_threshold) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0F, 1.0F);

    constexpr float k_panic_chance = 0.5F;
    if (dis(gen) < k_panic_chance) {
      elephant_comp->is_panicked = true;
      elephant_comp->panic_duration = 0.0F;
    }
  }

  if (elephant_comp->is_panicked) {
    elephant_comp->panic_duration += delta_time;

    constexpr float k_panic_duration = 5.0F;
    if (elephant_comp->panic_duration >= k_panic_duration) {
      elephant_comp->is_panicked = false;
      elephant_comp->panic_duration = 0.0F;
    }
  }
}

} // namespace Game::Systems
