#include "defense_tower_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "combat_system/combat_utils.h"
#include "combat_system/damage_processor.h"
#include "owner_registry.h"

#include <cmath>
#include <random>

namespace Game::Systems {

namespace {

thread_local std::mt19937 gen(std::random_device{}());

auto find_nearest_enemy_in_range(Engine::Core::Entity *tower,
                                 Engine::Core::World *world,
                                 float range) -> Engine::Core::Entity * {
  auto *tower_unit = tower->get_component<Engine::Core::UnitComponent>();
  auto *tower_transform =
      tower->get_component<Engine::Core::TransformComponent>();

  if (tower_unit == nullptr || tower_transform == nullptr) {
    return nullptr;
  }

  float best_dist_sq = range * range;
  Engine::Core::Entity *best_target = nullptr;

  auto &owner_registry = OwnerRegistry::instance();

  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : entities) {
    if (entity == tower) {
      continue;
    }

    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *target_unit = entity->get_component<Engine::Core::UnitComponent>();
    if (target_unit == nullptr || target_unit->health <= 0) {
      continue;
    }

    if (target_unit->owner_id == tower_unit->owner_id) {
      continue;
    }

    if (owner_registry.are_allies(tower_unit->owner_id,
                                  target_unit->owner_id)) {
      continue;
    }

    if (entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *target_transform =
        entity->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - tower_transform->position.x;
    float const dz = target_transform->position.z - tower_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_target = entity;
    }
  }

  return best_target;
}

void spawn_tower_arrows(Engine::Core::Entity *tower,
                        Engine::Core::Entity *target, ArrowSystem *arrow_sys) {
  if (arrow_sys == nullptr) {
    return;
  }

  auto *tower_t = tower->get_component<Engine::Core::TransformComponent>();
  auto *tgt_t = target->get_component<Engine::Core::TransformComponent>();
  auto *tower_u = tower->get_component<Engine::Core::UnitComponent>();

  if (tower_t == nullptr || tgt_t == nullptr) {
    return;
  }

  QVector3D const tower_pos(tower_t->position.x, tower_t->position.y + 2.0F,
                            tower_t->position.z);
  QVector3D const target_pos(tgt_t->position.x, tgt_t->position.y,
                             tgt_t->position.z);
  QVector3D const dir = (target_pos - tower_pos).normalized();
  QVector3D const color =
      (tower_u != nullptr)
          ? Game::Visuals::team_colorForOwner(tower_u->owner_id)
          : QVector3D(0.8F, 0.9F, 1.0F);

  std::uniform_real_distribution<float> spread_dist(-0.3F, 0.3F);
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const lateral_offset = spread_dist(gen);

  QVector3D const start =
      tower_pos + dir * 0.5F + perpendicular * lateral_offset;
  QVector3D const end =
      target_pos + QVector3D(0.0F, 0.8F, 0.0F) + perpendicular * lateral_offset;

  constexpr float kArrowSpeed = 12.0F;
  arrow_sys->spawn_arrow(start, end, color, kArrowSpeed);
}

} // namespace

void DefenseTowerSystem::update(Engine::Core::World *world, float delta_time) {
  auto *arrow_sys = world->get_system<ArrowSystem>();

  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *tower : entities) {
    if (tower->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *tower_unit = tower->get_component<Engine::Core::UnitComponent>();
    if (tower_unit == nullptr || tower_unit->health <= 0) {
      continue;
    }

    if (tower_unit->spawn_type != Game::Units::SpawnType::DefenseTower) {
      continue;
    }

    if (!tower->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *attack_comp = tower->get_component<Engine::Core::AttackComponent>();
    if (attack_comp == nullptr) {
      continue;
    }

    attack_comp->time_since_last += delta_time;
    if (attack_comp->time_since_last < attack_comp->cooldown) {
      continue;
    }

    auto *target =
        find_nearest_enemy_in_range(tower, world, attack_comp->range);
    if (target == nullptr) {
      continue;
    }

    spawn_tower_arrows(tower, target, arrow_sys);
    Combat::deal_damage(world, target, attack_comp->damage, tower->get_id());
    attack_comp->time_since_last = 0.0F;
  }
}

} // namespace Game::Systems
