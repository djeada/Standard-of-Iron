#include "catapult_attack_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "../visuals/team_colors.h"
#include "projectile_system.h"
#include <cmath>
#include <qvectornd.h>

namespace Game::Systems {

void CatapultAttackSystem::update(Engine::Core::World *world,
                                  float delta_time) {
  process_catapult_attacks(world, delta_time);
}

void CatapultAttackSystem::process_catapult_attacks(Engine::Core::World *world,
                                                    float delta_time) {
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *entity : entities) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    if (unit->spawn_type != Game::Units::SpawnType::Catapult) {
      continue;
    }

    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *loading =
        entity->get_component<Engine::Core::CatapultLoadingComponent>();
    if (loading == nullptr) {
      loading = entity->add_component<Engine::Core::CatapultLoadingComponent>();
    }

    auto *movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement != nullptr) {
      constexpr float k_movement_threshold = 0.01F;
      bool is_moving = (std::abs(movement->vx) > k_movement_threshold ||
                        std::abs(movement->vz) > k_movement_threshold);

      if (is_moving &&
          loading->state !=
              Engine::Core::CatapultLoadingComponent::LoadingState::Idle) {

        loading->state =
            Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
        loading->loading_time = 0.0F;
        loading->firing_time = 0.0F;
        loading->target_position_locked = false;
        loading->target_id = 0;
      }
    }

    switch (loading->state) {
    case Engine::Core::CatapultLoadingComponent::LoadingState::Idle: {

      auto *attack_target =
          entity->get_component<Engine::Core::AttackTargetComponent>();
      if (attack_target != nullptr && attack_target->target_id != 0) {
        auto *target = world->get_entity(attack_target->target_id);
        if (target != nullptr &&
            !target->has_component<Engine::Core::PendingRemovalComponent>()) {
          auto *target_unit =
              target->get_component<Engine::Core::UnitComponent>();
          if (target_unit != nullptr && target_unit->health > 0) {

            auto *transform =
                entity->get_component<Engine::Core::TransformComponent>();
            auto *target_transform =
                target->get_component<Engine::Core::TransformComponent>();
            auto *attack =
                entity->get_component<Engine::Core::AttackComponent>();

            if (transform != nullptr && target_transform != nullptr &&
                attack != nullptr) {
              float const dx =
                  target_transform->position.x - transform->position.x;
              float const dz =
                  target_transform->position.z - transform->position.z;
              float const dist = std::sqrt(dx * dx + dz * dz);

              if (dist <= attack->range) {
                start_loading(entity, target);
              }
            }
          }
        }
      }
      break;
    }

    case Engine::Core::CatapultLoadingComponent::LoadingState::Loading: {
      update_loading(entity, delta_time);
      break;
    }

    case Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire: {
      fire_projectile(world, entity);
      break;
    }

    case Engine::Core::CatapultLoadingComponent::LoadingState::Firing: {
      update_firing(entity, delta_time);
      break;
    }
    }
  }
}

void CatapultAttackSystem::start_loading(Engine::Core::Entity *catapult,
                                         Engine::Core::Entity *target) {
  auto *loading =
      catapult->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  auto *target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return;
  }

  loading->state =
      Engine::Core::CatapultLoadingComponent::LoadingState::Loading;
  loading->loading_time = 0.0F;
  loading->target_id = target->get_id();
  loading->target_locked_x = target_transform->position.x;
  loading->target_locked_y = target_transform->position.y;
  loading->target_locked_z = target_transform->position.z;
  loading->target_position_locked = true;

  auto *catapult_transform =
      catapult->get_component<Engine::Core::TransformComponent>();
  if (catapult_transform != nullptr) {
    float const dx =
        target_transform->position.x - catapult_transform->position.x;
    float const dz =
        target_transform->position.z - catapult_transform->position.z;
    float const yaw = std::atan2(dx, dz) * 180.0F / 3.14159265F;
    catapult_transform->desired_yaw = yaw;
    catapult_transform->has_desired_yaw = true;
  }
}

void CatapultAttackSystem::update_loading(Engine::Core::Entity *catapult,
                                          float delta_time) {
  auto *loading =
      catapult->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  loading->loading_time += delta_time;

  if (loading->loading_time >= loading->loading_duration) {
    loading->state =
        Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire;
  }
}

void CatapultAttackSystem::fire_projectile(Engine::Core::World *world,
                                           Engine::Core::Entity *catapult) {
  auto *loading =
      catapult->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  auto *projectile_sys = world->get_system<ProjectileSystem>();
  if (projectile_sys == nullptr) {
    loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
    loading->loading_time = 0.0F;
    loading->target_position_locked = false;
    return;
  }

  auto *transform = catapult->get_component<Engine::Core::TransformComponent>();
  auto *attack = catapult->get_component<Engine::Core::AttackComponent>();
  auto *unit = catapult->get_component<Engine::Core::UnitComponent>();

  if (transform == nullptr || attack == nullptr) {
    loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
    loading->loading_time = 0.0F;
    loading->target_position_locked = false;
    return;
  }

  QVector3D const start(transform->position.x, transform->position.y + 1.5F,
                        transform->position.z);

  QVector3D const end(loading->target_locked_x, loading->target_locked_y,
                      loading->target_locked_z);

  QVector3D color(0.45F, 0.42F, 0.38F);
  if (unit != nullptr) {
    color = Game::Visuals::team_colorForOwner(unit->owner_id);
  }

  constexpr float k_stone_speed = 8.0F;
  constexpr float k_stone_scale = 1.5F;

  projectile_sys->spawn_stone(start, end, color, k_stone_speed, k_stone_scale,
                              true, attack->damage, catapult->get_id(),
                              loading->target_id);

  loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Firing;
  loading->firing_time = 0.0F;
}

void CatapultAttackSystem::update_firing(Engine::Core::Entity *catapult,
                                         float delta_time) {
  auto *loading =
      catapult->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return;
  }

  loading->firing_time += delta_time;

  if (loading->firing_time >= loading->firing_duration) {

    loading->state = Engine::Core::CatapultLoadingComponent::LoadingState::Idle;
    loading->loading_time = 0.0F;
    loading->firing_time = 0.0F;
    loading->target_position_locked = false;

    auto *attack = catapult->get_component<Engine::Core::AttackComponent>();
    if (attack != nullptr) {
      attack->time_since_last = 0.0F;
    }
  }
}

} // namespace Game::Systems
