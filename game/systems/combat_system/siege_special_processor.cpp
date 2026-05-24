#include "siege_special_processor.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "../../visuals/team_colors.h"
#include "../arrow_system.h"
#include "../projectile_system.h"
#include "combat_random.h"
#include "combat_utils.h"
#include "damage_processor.h"

namespace Game::Systems::Combat {

namespace {

[[nodiscard]] auto get_entity_from_query_context(
    const CombatQueryContext& query_context,
    Engine::Core::EntityID entity_id) -> Engine::Core::Entity* {
  auto const it = query_context.entities_by_id.find(entity_id);
  return (it != query_context.entities_by_id.end()) ? it->second : nullptr;
}

[[nodiscard]] auto
is_motion_active(const Engine::Core::Entity& entity) noexcept -> bool {
  auto const* motion =
      entity.get_component<Engine::Core::MotionPresentationComponent>();
  return motion != nullptr && motion->has_locomotion();
}

void reset_loading(Engine::Core::CatapultLoadingComponent& loading) {
  loading.state = Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
  loading.loading_time = 0.0F;
  loading.firing_time = 0.0F;
  loading.target_position_locked = false;
  loading.target_id = 0;
}

void face_locked_target(Engine::Core::Entity* siege,
                        const Engine::Core::TransformComponent* target_transform) {
  auto* siege_transform = siege->get_component<Engine::Core::TransformComponent>();
  if (siege_transform == nullptr || target_transform == nullptr) {
    return;
  }

  float const dx = target_transform->position.x - siege_transform->position.x;
  float const dz = target_transform->position.z - siege_transform->position.z;
  float const yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  siege_transform->desired_yaw = yaw;
  siege_transform->has_desired_yaw = true;
}

void start_loading(Engine::Core::Entity* siege, Engine::Core::Entity* target) {
  auto* loading = siege->get_component<Engine::Core::CatapultLoadingComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (loading == nullptr || target_transform == nullptr) {
    return;
  }

  loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Loading;
  loading->loading_time = 0.0F;
  loading->target_id = target->get_id();
  loading->target_locked_x = target_transform->position.x;
  loading->target_locked_y = target_transform->position.y;
  loading->target_locked_z = target_transform->position.z;
  loading->target_position_locked = true;

  face_locked_target(siege, target_transform);
}

[[nodiscard]] auto locked_target_is_in_range(Engine::Core::Entity* siege,
                                             Engine::Core::Entity* target) -> bool {
  auto* siege_transform = siege->get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  auto* attack = siege->get_component<Engine::Core::AttackComponent>();
  if (siege_transform == nullptr || target_transform == nullptr || attack == nullptr) {
    return false;
  }

  float const dx = target_transform->position.x - siege_transform->position.x;
  float const dz = target_transform->position.z - siege_transform->position.z;
  float const dist = std::sqrt(dx * dx + dz * dz);
  return dist <= attack->range;
}

void update_loading(Engine::Core::Entity* siege,
                    float delta_time,
                    float loading_duration) {
  auto* loading = siege->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  loading->loading_duration = loading_duration;
  loading->loading_time += delta_time;

  if (loading->loading_time >= loading->loading_duration) {
    loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire;
  }
}

void update_firing(Engine::Core::Entity* siege,
                   float delta_time,
                   float firing_duration) {
  auto* loading = siege->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  loading->firing_duration = firing_duration;
  loading->firing_time += delta_time;

  if (loading->firing_time < loading->firing_duration) {
    return;
  }

  loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
  loading->loading_time = 0.0F;
  loading->firing_time = 0.0F;
  loading->target_position_locked = false;

  auto* attack = siege->get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr) {
    attack->time_since_last = 0.0F;
  }
}

void fire_catapult(Engine::Core::World* world, Engine::Core::Entity* catapult) {
  auto* loading = catapult->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  auto* projectile_sys = world->get_system<ProjectileSystem>();
  auto* transform = catapult->get_component<Engine::Core::TransformComponent>();
  auto* attack = catapult->get_component<Engine::Core::AttackComponent>();
  auto* unit = catapult->get_component<Engine::Core::UnitComponent>();
  if (projectile_sys == nullptr || transform == nullptr || attack == nullptr) {
    reset_loading(*loading);
    return;
  }

  QVector3D const start(
      transform->position.x, transform->position.y + 1.5F, transform->position.z);
  QVector3D const end(
      loading->target_locked_x, loading->target_locked_y, loading->target_locked_z);
  QVector3D const color = (unit != nullptr)
                              ? Game::Visuals::team_colorForOwner(unit->owner_id)
                              : QVector3D(0.45F, 0.42F, 0.38F);

  projectile_sys->spawn_stone(start,
                              end,
                              color,
                              8.0F,
                              1.5F,
                              true,
                              attack->damage,
                              catapult->get_id(),
                              loading->target_id);

  loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Firing;
  loading->firing_time = 0.0F;
}

void fire_ballista(Engine::Core::World* world, Engine::Core::Entity* ballista) {
  auto* loading = ballista->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  auto* projectile_sys = world->get_system<ProjectileSystem>();
  auto* transform = ballista->get_component<Engine::Core::TransformComponent>();
  auto* attack = ballista->get_component<Engine::Core::AttackComponent>();
  auto* unit = ballista->get_component<Engine::Core::UnitComponent>();
  if (projectile_sys == nullptr || transform == nullptr || attack == nullptr) {
    reset_loading(*loading);
    return;
  }

  QVector3D const start(
      transform->position.x, transform->position.y + 1.0F, transform->position.z);
  QVector3D const end(
      loading->target_locked_x, loading->target_locked_y, loading->target_locked_z);
  QVector3D const color = (unit != nullptr)
                              ? Game::Visuals::team_colorForOwner(unit->owner_id)
                              : QVector3D(0.8F, 0.7F, 0.2F);

  projectile_sys->spawn_arrow(start, end, color, 10.0F, true);

  auto* target = world->get_entity(loading->target_id);
  if (target != nullptr) {
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    QVector3D const locked_pos(
        loading->target_locked_x, loading->target_locked_y, loading->target_locked_z);
    if (target_transform != nullptr) {
      QVector3D const current_pos(target_transform->position.x,
                                  target_transform->position.y,
                                  target_transform->position.z);
      constexpr float k_escape_radius = 1.5F;
      if ((current_pos - locked_pos).length() <= k_escape_radius) {
        deal_damage(world, target, attack->damage, ballista->get_id());
      }
    }
  }

  loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Firing;
  loading->firing_time = 0.0F;
}

void process_loading_siege_unit(Engine::Core::World* world,
                                Engine::Core::Entity* siege,
                                const CombatQueryContext& query_context,
                                Game::Units::SpawnType spawn_type,
                                float delta_time) {
  auto* unit = siege->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 || unit->spawn_type != spawn_type) {
    return;
  }

  auto* loading = siege->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    loading = siege->add_component<Engine::Core::CatapultLoadingComponent>();
  }

  if (is_motion_active(*siege) &&
      loading->state != Engine::Core::CatapultLoadingComponent::LoadingState::Idle) {
    reset_loading(*loading);
  }

  switch (loading->state) {
  case Engine::Core::CatapultLoadingComponent::LoadingState::Idle: {
    auto* attack_target = siege->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target == nullptr || attack_target->target_id == 0) {
      break;
    }
    auto* target =
        get_entity_from_query_context(query_context, attack_target->target_id);
    if (is_valid_enemy_unit(unit, target, true) &&
        locked_target_is_in_range(siege, target)) {
      start_loading(siege, target);
    }
    break;
  }
  case Engine::Core::CatapultLoadingComponent::LoadingState::Loading:
    update_loading(siege,
                   delta_time,
                   spawn_type == Game::Units::SpawnType::Ballista
                       ? 1.0F
                       : loading->loading_duration);
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire:
    if (spawn_type == Game::Units::SpawnType::Ballista) {
      fire_ballista(world, siege);
    } else {
      fire_catapult(world, siege);
    }
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::Firing:
    update_firing(siege,
                  delta_time,
                  spawn_type == Game::Units::SpawnType::Ballista
                      ? 0.3F
                      : loading->firing_duration);
    break;
  }
}

[[nodiscard]] auto is_enemy_tower_building(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr || !entity->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->spawn_type == Game::Units::SpawnType::DefenseTower;
}

[[nodiscard]] auto
find_nearest_tower_target(Engine::Core::Entity* tower,
                          const CombatQueryContext& query_context,
                          float range,
                          float max_height_diff) -> Engine::Core::Entity* {
  auto* tower_unit = tower->get_component<Engine::Core::UnitComponent>();
  auto* tower_transform = tower->get_component<Engine::Core::TransformComponent>();
  if (tower_unit == nullptr || tower_transform == nullptr) {
    return nullptr;
  }

  float best_dist_sq = range * range;
  Engine::Core::Entity* best_target = nullptr;

  for (auto* entity : query_context.units) {
    if (entity == tower) {
      continue;
    }
    if (!is_valid_enemy_unit(tower_unit, entity, true)) {
      continue;
    }
    if (entity->has_component<Engine::Core::BuildingComponent>() &&
        !is_enemy_tower_building(entity)) {
      continue;
    }

    auto* target_transform = entity->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - tower_transform->position.x;
    float const dy = target_transform->position.y - tower_transform->position.y;
    float const dz = target_transform->position.z - tower_transform->position.z;
    if (std::fabs(dy) > max_height_diff) {
      continue;
    }

    float const dist_sq = dx * dx + dz * dz;
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_target = entity;
    }
  }

  return best_target;
}

void spawn_tower_arrows(Engine::Core::Entity* tower,
                        Engine::Core::Entity* target,
                        ArrowSystem* arrow_sys) {
  if (arrow_sys == nullptr) {
    return;
  }

  auto* tower_t = tower->get_component<Engine::Core::TransformComponent>();
  auto* target_t = target->get_component<Engine::Core::TransformComponent>();
  auto* tower_u = tower->get_component<Engine::Core::UnitComponent>();
  if (tower_t == nullptr || target_t == nullptr) {
    return;
  }

  QVector3D const tower_pos(
      tower_t->position.x, tower_t->position.y + 2.0F, tower_t->position.z);
  QVector3D const target_pos(
      target_t->position.x, target_t->position.y, target_t->position.z);
  QVector3D const dir = (target_pos - tower_pos).normalized();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  QVector3D const color = (tower_u != nullptr)
                              ? Game::Visuals::team_colorForOwner(tower_u->owner_id)
                              : QVector3D(0.8F, 0.9F, 1.0F);

  constexpr int k_arrows_per_volley = 2;
  constexpr float k_arrow_speed = 12.0F;
  std::uint32_t const seed = static_cast<std::uint32_t>(tower->get_id() * 2246822519U) ^
                             static_cast<std::uint32_t>(target->get_id() * 3266489917U);

  for (int i = 0; i < k_arrows_per_volley; ++i) {
    float const lateral_offset =
        deterministic_range(seed, static_cast<std::uint32_t>(i + 1), -0.3F, 0.3F);

    QVector3D const start = tower_pos + dir * 0.5F + perpendicular * lateral_offset;
    QVector3D const end =
        target_pos + QVector3D(0.0F, 0.8F, 0.0F) + perpendicular * lateral_offset;
    arrow_sys->spawn_arrow(start, end, color, k_arrow_speed, ArrowVisualStyle::Focused);
  }
}

void process_defense_tower(Engine::Core::World* world,
                           Engine::Core::Entity* tower,
                           const CombatQueryContext& query_context,
                           float delta_time) {
  auto* tower_unit = tower->get_component<Engine::Core::UnitComponent>();
  if (tower_unit == nullptr || tower_unit->health <= 0 ||
      tower_unit->spawn_type != Game::Units::SpawnType::DefenseTower ||
      !tower->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  auto* attack = tower->get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr) {
    return;
  }

  attack->time_since_last += delta_time;
  if (attack->time_since_last < attack->cooldown) {
    return;
  }

  auto* target = find_nearest_tower_target(
      tower, query_context, attack->range, attack->max_height_difference);
  if (target == nullptr) {
    return;
  }

  spawn_tower_arrows(tower, target, world->get_system<ArrowSystem>());
  deal_damage(world, target, attack->damage, tower->get_id());
  attack->time_since_last = 0.0F;
}

} // namespace

void process_siege_specials(Engine::Core::World* world,
                            const CombatQueryContext& query_context,
                            float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity : query_context.units) {
    process_loading_siege_unit(
        world, entity, query_context, Game::Units::SpawnType::Catapult, delta_time);
    process_loading_siege_unit(
        world, entity, query_context, Game::Units::SpawnType::Ballista, delta_time);
    process_defense_tower(world, entity, query_context, delta_time);
  }
}

} // namespace Game::Systems::Combat
