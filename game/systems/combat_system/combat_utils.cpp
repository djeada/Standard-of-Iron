#include "combat_utils.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../owner_registry.h"

namespace Game::Systems::Combat {

namespace {
constexpr float k_combat_query_cell_size = 15.0F;

auto get_entity_from_query_context(const CombatQueryContext &query_context,
                                   Engine::Core::EntityID entity_id)
    -> Engine::Core::Entity * {
  auto const it = query_context.entities_by_id.find(entity_id);
  return (it != query_context.entities_by_id.end()) ? it->second : nullptr;
}
} // namespace

CombatQueryContext::CombatQueryContext()
    : unit_grid(k_combat_query_cell_size) {}

void CombatQueryContext::clear() {
  units.clear();
  entities_by_id.clear();
  unit_grid.clear();
  nearby_unit_ids.clear();
}

auto build_combat_query_context(Engine::Core::World *world)
    -> CombatQueryContext {
  CombatQueryContext query_context;
  rebuild_combat_query_context(world, query_context);
  return query_context;
}

void rebuild_combat_query_context(Engine::Core::World *world,
                                  CombatQueryContext &query_context) {
  query_context.clear();
  if (world == nullptr) {
    return;
  }

  auto const world_units =
      world->get_entities_with<Engine::Core::UnitComponent>();
  query_context.units.reserve(world_units.size());
  query_context.entities_by_id.reserve(world_units.size());

  for (auto *entity : world_units) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || (unit->health <= 0)) {
      continue;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    query_context.units.push_back(entity);
    query_context.entities_by_id.emplace(entity->get_id(), entity);

    if (is_building(entity)) {
      continue;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      query_context.unit_grid.insert(entity->get_id(), transform->position.x,
                                     transform->position.z);
    }
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

auto is_building(Engine::Core::Entity *entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  return entity->has_component<Engine::Core::BuildingComponent>();
}

auto is_valid_enemy_unit(const Engine::Core::UnitComponent *attacker_unit,
                         Engine::Core::Entity *target,
                         bool allow_buildings) -> bool {
  if ((attacker_unit == nullptr) || (target == nullptr)) {
    return false;
  }
  if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }

  auto *target_unit = target->get_component<Engine::Core::UnitComponent>();
  if ((target_unit == nullptr) || (target_unit->health <= 0)) {
    return false;
  }
  if (target_unit->owner_id == attacker_unit->owner_id) {
    return false;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  if (owner_registry.are_allies(attacker_unit->owner_id,
                                target_unit->owner_id)) {
    return false;
  }

  if (!allow_buildings && is_building(target)) {
    return false;
  }

  return true;
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

auto find_nearest_enemy(
    Engine::Core::Entity *unit, const CombatQueryContext &query_context,
    float max_range, std::uint64_t *scan_iterations) -> Engine::Core::Entity * {
  auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto *unit_transform =
      unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  Engine::Core::Entity *nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;
  auto &nearby_ids = query_context.nearby_unit_ids;
  query_context.unit_grid.get_entities_in_range(unit_transform->position.x,
                                                unit_transform->position.z,
                                                max_range, nearby_ids);

  for (auto target_id : nearby_ids) {
    if (scan_iterations != nullptr) {
      *scan_iterations += 1;
    }
    auto *target = get_entity_from_query_context(query_context, target_id);
    if (target == nullptr) {
      continue;
    }
    if (target == unit) {
      continue;
    }

    if (!is_valid_enemy_unit(unit_comp, target, false)) {
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
