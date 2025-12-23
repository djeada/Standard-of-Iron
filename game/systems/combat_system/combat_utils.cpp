#include "combat_utils.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../owner_registry.h"
#include "../spatial_grid.h"

namespace Game::Systems::Combat {

namespace {
thread_local SpatialGrid t_unit_grid(15.0F);
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

auto is_blocked_by_hold_mode_unit(
    Engine::Core::Entity *attacker, Engine::Core::Entity *target,
    const std::vector<Engine::Core::Entity *> &all_units) -> bool {
  auto *attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  auto *attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();

  if ((attacker_transform == nullptr) || (target_transform == nullptr) ||
      (attacker_unit == nullptr)) {
    return false;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  float const ax = attacker_transform->position.x;
  float const az = attacker_transform->position.z;
  float const tx = target_transform->position.x;
  float const tz = target_transform->position.z;

  float const dx = tx - ax;
  float const dz = tz - az;
  float const line_length_sq = dx * dx + dz * dz;

  if (line_length_sq < 0.01F) {
    return false;
  }

  float const line_length = std::sqrt(line_length_sq);
  float const ndx = dx / line_length;
  float const ndz = dz / line_length;

  constexpr float kBlockRadius = 1.5F;

  for (auto *blocker : all_units) {
    if (blocker == attacker || blocker == target) {
      continue;
    }

    if (blocker->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *blocker_unit = blocker->get_component<Engine::Core::UnitComponent>();
    if ((blocker_unit == nullptr) || blocker_unit->health <= 0) {
      continue;
    }

    if (blocker_unit->owner_id == attacker_unit->owner_id) {
      continue;
    }
    if (owner_registry.are_allies(attacker_unit->owner_id,
                                   blocker_unit->owner_id)) {
      continue;
    }

    auto *hold_mode =
        blocker->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode == nullptr) || !hold_mode->active) {
      continue;
    }

    auto *blocker_transform =
        blocker->get_component<Engine::Core::TransformComponent>();
    if (blocker_transform == nullptr) {
      continue;
    }

    float const bx = blocker_transform->position.x;
    float const bz = blocker_transform->position.z;

    float const abx = bx - ax;
    float const abz = bz - az;
    float const projection = abx * ndx + abz * ndz;

    if (projection < 0.0F || projection > line_length) {
      continue;
    }

    float const closest_x = ax + ndx * projection;
    float const closest_z = az + ndz * projection;

    float const dist_x = bx - closest_x;
    float const dist_z = bz - closest_z;
    float const dist_sq = dist_x * dist_x + dist_z * dist_z;

    if (dist_sq < kBlockRadius * kBlockRadius) {
      return true;
    }
  }

  return false;
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
      if (!is_blocked_by_hold_mode_unit(unit, target, all_units)) {
        nearest_dist_sq = dist_sq;
        nearest_enemy = target;
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
      if (!is_blocked_by_hold_mode_unit(unit, target, units)) {
        nearest_dist_sq = dist_sq;
        nearest_enemy = target;
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
