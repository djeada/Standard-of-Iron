#include "combat_utils.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../owner_registry.h"
#include "../spatial_grid.h"

#include <cmath>

namespace Game::Systems::Combat {

namespace {
thread_local SpatialGrid t_unit_grid(15.0F);

// Radius for hold mode blocking zone - how close a unit must be to the path
constexpr float kHoldModeBlockingRadius = 2.0F;

// Check if a hold-mode unit is blocking the path from attacker to target
// Returns true if the blocker is between attacker and target and close to
// the path
auto is_blocking_path(float attacker_x, float attacker_z, float target_x,
                      float target_z, float blocker_x,
                      float blocker_z) -> bool {
  // Vector from attacker to target
  float const to_target_x = target_x - attacker_x;
  float const to_target_z = target_z - attacker_z;
  float const dist_to_target_sq = to_target_x * to_target_x + to_target_z * to_target_z;

  if (dist_to_target_sq < 0.01F) {
    return false; // Target is at attacker position
  }

  // Vector from attacker to blocker
  float const to_blocker_x = blocker_x - attacker_x;
  float const to_blocker_z = blocker_z - attacker_z;
  float const dist_to_blocker_sq = to_blocker_x * to_blocker_x + to_blocker_z * to_blocker_z;

  // Blocker must be closer than target
  if (dist_to_blocker_sq >= dist_to_target_sq) {
    return false;
  }

  // Project blocker position onto the line from attacker to target
  float const dist_to_target = std::sqrt(dist_to_target_sq);
  float const dir_x = to_target_x / dist_to_target;
  float const dir_z = to_target_z / dist_to_target;

  // Dot product gives projection length along the path
  float const projection = to_blocker_x * dir_x + to_blocker_z * dir_z;

  // Blocker must be in front of attacker (positive projection)
  if (projection < 0.0F) {
    return false;
  }

  // Calculate perpendicular distance from blocker to the path line
  float const proj_x = projection * dir_x;
  float const proj_z = projection * dir_z;
  float const perp_x = to_blocker_x - proj_x;
  float const perp_z = to_blocker_z - proj_z;
  float const perp_dist_sq = perp_x * perp_x + perp_z * perp_z;

  // Check if blocker is within blocking radius of the path
  return perp_dist_sq <= kHoldModeBlockingRadius * kHoldModeBlockingRadius;
}
}

auto is_unit_in_hold_mode(Engine::Core::Entity *entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  return (hold_mode != nullptr) && hold_mode->active;
}

auto is_unit_in_guard_mode(Engine::Core::Entity *entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto *guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
  return (guard_mode != nullptr) && guard_mode->active;
}

auto is_in_range(Engine::Core::Entity *attacker, Engine::Core::Entity *target,
                 float range) -> bool {
  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();

  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return false;
  }

  float const dx =
      target_transform->position.x - attacker_transform->position.x;
  float const dz =
      target_transform->position.z - attacker_transform->position.z;
  float const dy =
      target_transform->position.y - attacker_transform->position.y;
  float const distance_squared = dx * dx + dz * dz;

  float const scale_x = target_transform->scale.x;
  float const scale_z = target_transform->scale.z;
  float const target_radius = std::max(scale_x, scale_z) * 0.5F;
  float const effective_range = range + target_radius;

  if (distance_squared > effective_range * effective_range) {
    return false;
  }

  auto *attacker_atk = attacker->get_component<Engine::Core::AttackComponent>();
  if ((attacker_atk != nullptr) &&
      attacker_atk->current_mode ==
          Engine::Core::AttackComponent::CombatMode::Melee) {
    float const height_diff = std::abs(dy);
    if (height_diff > attacker_atk->max_height_difference) {
      return false;
    }
  }

  return true;
}

auto is_unit_idle(Engine::Core::Entity *unit) -> bool {
  auto *hold_mode = unit->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    return false;
  }

  auto *guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
  if ((guard_mode != nullptr) && guard_mode->active &&
      guard_mode->returning_to_guard_position) {
    return false;
  }

  auto *attack_target =
      unit->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target != nullptr) && attack_target->target_id != 0) {
    return false;
  }

  auto *movement = unit->get_component<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->has_target) {
    return false;
  }

  auto *attack_comp = unit->get_component<Engine::Core::AttackComponent>();
  if ((attack_comp != nullptr) && attack_comp->in_melee_lock) {
    return false;
  }

  auto *patrol = unit->get_component<Engine::Core::PatrolComponent>();
  return (patrol == nullptr) || !patrol->patrolling;
}

auto find_nearest_enemy_from_list(
    Engine::Core::Entity *unit,
    const std::vector<Engine::Core::Entity *> &all_units, Engine::Core::World *,
    float max_range) -> Engine::Core::Entity * {
  auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto *unit_transform =
      unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  Engine::Core::Entity *nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;

  // First pass: find nearest enemy (original logic)
  for (auto *target : all_units) {
    if (target == unit) {
      continue;
    }

    if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (target_unit->owner_id == unit_comp->owner_id) {
      continue;
    }
    if (owner_registry.are_allies(unit_comp->owner_id, target_unit->owner_id)) {
      continue;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - unit_transform->position.x;
    float const dz = target_transform->position.z - unit_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < nearest_dist_sq) {
      nearest_dist_sq = dist_sq;
      nearest_enemy = target;
    }
  }

  // Second pass: check if a hold-mode enemy is blocking the path to the target
  // If so, prioritize the hold-mode enemy instead
  if (nearest_enemy != nullptr) {
    auto *target_transform =
        nearest_enemy->get_component<Engine::Core::TransformComponent>();
    if (target_transform != nullptr) {
      Engine::Core::Entity *blocking_enemy = nullptr;
      float blocking_dist_sq = nearest_dist_sq;

      for (auto *potential_blocker : all_units) {
        if (potential_blocker == unit || potential_blocker == nearest_enemy) {
          continue;
        }

        if (potential_blocker
                ->has_component<Engine::Core::PendingRemovalComponent>()) {
          continue;
        }

        // Check if this is an enemy unit in hold mode
        if (!is_unit_in_hold_mode(potential_blocker)) {
          continue;
        }

        auto *blocker_unit =
            potential_blocker->get_component<Engine::Core::UnitComponent>();
        if ((blocker_unit == nullptr) || blocker_unit->health <= 0) {
          continue;
        }

        if (blocker_unit->owner_id == unit_comp->owner_id) {
          continue;
        }
        if (owner_registry.are_allies(unit_comp->owner_id,
                                      blocker_unit->owner_id)) {
          continue;
        }

        if (potential_blocker
                ->has_component<Engine::Core::BuildingComponent>()) {
          continue;
        }

        auto *blocker_transform =
            potential_blocker
                ->get_component<Engine::Core::TransformComponent>();
        if (blocker_transform == nullptr) {
          continue;
        }

        // Check if this hold-mode enemy is blocking the path to target
        if (is_blocking_path(unit_transform->position.x,
                             unit_transform->position.z,
                             target_transform->position.x,
                             target_transform->position.z,
                             blocker_transform->position.x,
                             blocker_transform->position.z)) {
          float const dx =
              blocker_transform->position.x - unit_transform->position.x;
          float const dz =
              blocker_transform->position.z - unit_transform->position.z;
          float const dist_sq = dx * dx + dz * dz;

          // Prefer the closest blocking enemy
          if (dist_sq < blocking_dist_sq) {
            blocking_dist_sq = dist_sq;
            blocking_enemy = potential_blocker;
          }
        }
      }

      // If we found a blocking enemy, target them instead
      if (blocking_enemy != nullptr) {
        return blocking_enemy;
      }
    }
  }

  return nearest_enemy;
}

auto find_nearest_enemy(Engine::Core::Entity *unit, Engine::Core::World *world,
                        float max_range) -> Engine::Core::Entity * {
  auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto *unit_transform =
      unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  t_unit_grid.clear();
  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *entity : units) {
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      t_unit_grid.insert(entity->get_id(), transform->position.x,
                         transform->position.z);
    }
  }

  auto nearby_ids = t_unit_grid.get_entities_in_range(
      unit_transform->position.x, unit_transform->position.z, max_range);

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  Engine::Core::Entity *nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;

  // First pass: find nearest enemy
  for (Engine::Core::EntityID target_id : nearby_ids) {
    if (target_id == unit->get_id()) {
      continue;
    }

    auto *target = world->get_entity(target_id);
    if (target == nullptr) {
      continue;
    }

    if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (target_unit->owner_id == unit_comp->owner_id) {
      continue;
    }
    if (owner_registry.are_allies(unit_comp->owner_id, target_unit->owner_id)) {
      continue;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - unit_transform->position.x;
    float const dz = target_transform->position.z - unit_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < nearest_dist_sq) {
      nearest_dist_sq = dist_sq;
      nearest_enemy = target;
    }
  }

  // Second pass: check if a hold-mode enemy is blocking the path to the target
  // If so, prioritize the hold-mode enemy instead
  if (nearest_enemy != nullptr) {
    auto *target_transform =
        nearest_enemy->get_component<Engine::Core::TransformComponent>();
    if (target_transform != nullptr) {
      Engine::Core::Entity *blocking_enemy = nullptr;
      float blocking_dist_sq = nearest_dist_sq;

      for (Engine::Core::EntityID blocker_id : nearby_ids) {
        if (blocker_id == unit->get_id() ||
            blocker_id == nearest_enemy->get_id()) {
          continue;
        }

        auto *potential_blocker = world->get_entity(blocker_id);
        if (potential_blocker == nullptr) {
          continue;
        }

        if (potential_blocker
                ->has_component<Engine::Core::PendingRemovalComponent>()) {
          continue;
        }

        // Check if this is an enemy unit in hold mode
        if (!is_unit_in_hold_mode(potential_blocker)) {
          continue;
        }

        auto *blocker_unit =
            potential_blocker->get_component<Engine::Core::UnitComponent>();
        if ((blocker_unit == nullptr) || blocker_unit->health <= 0) {
          continue;
        }

        if (blocker_unit->owner_id == unit_comp->owner_id) {
          continue;
        }
        if (owner_registry.are_allies(unit_comp->owner_id,
                                      blocker_unit->owner_id)) {
          continue;
        }

        if (potential_blocker
                ->has_component<Engine::Core::BuildingComponent>()) {
          continue;
        }

        auto *blocker_transform =
            potential_blocker
                ->get_component<Engine::Core::TransformComponent>();
        if (blocker_transform == nullptr) {
          continue;
        }

        // Check if this hold-mode enemy is blocking the path to target
        if (is_blocking_path(unit_transform->position.x,
                             unit_transform->position.z,
                             target_transform->position.x,
                             target_transform->position.z,
                             blocker_transform->position.x,
                             blocker_transform->position.z)) {
          float const dx =
              blocker_transform->position.x - unit_transform->position.x;
          float const dz =
              blocker_transform->position.z - unit_transform->position.z;
          float const dist_sq = dx * dx + dz * dz;

          // Prefer the closest blocking enemy
          if (dist_sq < blocking_dist_sq) {
            blocking_dist_sq = dist_sq;
            blocking_enemy = potential_blocker;
          }
        }
      }

      // If we found a blocking enemy, target them instead
      if (blocking_enemy != nullptr) {
        return blocking_enemy;
      }
    }
  }

  return nearest_enemy;
}

auto should_auto_engage_melee(Engine::Core::Entity *unit) -> bool {
  if (unit == nullptr) {
    return false;
  }

  auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr) {
    return false;
  }

  switch (unit_comp->spawn_type) {
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::Healer:
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return false;

  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseSpearman:
    return true;

  case Game::Units::SpawnType::Barracks:
    return false;
  }

  return false;
}

} // namespace Game::Systems::Combat
