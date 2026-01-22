#include "guard_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "command_service.h"
#include <QVector3D>
#include <cmath>

namespace Game::Systems {

void GuardSystem::update(Engine::Core::World *world, float) {
  if (world == nullptr) {
    return;
  }

  auto entities = world->get_entities_with<Engine::Core::GuardModeComponent>();

  for (auto *entity : entities) {
    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    auto *movement = entity->get_component<Engine::Core::MovementComponent>();
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();

    if ((guard_mode == nullptr) || (movement == nullptr) ||
        (transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    if (!guard_mode->active || !guard_mode->has_guard_target) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    auto *attack_target =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    if ((attack_target != nullptr) && attack_target->target_id != 0) {
      continue;
    }

    if (guard_mode->guarded_entity_id != 0) {
      auto *guarded_entity = world->get_entity(guard_mode->guarded_entity_id);
      if (guarded_entity != nullptr) {
        auto *guarded_transform =
            guarded_entity->get_component<Engine::Core::TransformComponent>();
        if (guarded_transform != nullptr) {

          float const new_guard_x = guarded_transform->position.x;
          float const new_guard_z = guarded_transform->position.z;

          float const dx = new_guard_x - transform->position.x;
          float const dz = new_guard_z - transform->position.z;
          float const dist_sq = dx * dx + dz * dz;

          constexpr float k_follow_threshold_sq = 2.0F * 2.0F;

          if (dist_sq > k_follow_threshold_sq) {

            guard_mode->guard_position_x = new_guard_x;
            guard_mode->guard_position_z = new_guard_z;

            bool const already_moving_to_target =
                movement->has_target &&
                std::abs(movement->goal_x - new_guard_x) < 0.5F &&
                std::abs(movement->goal_y - new_guard_z) < 0.5F;

            if (!already_moving_to_target) {
              guard_mode->returning_to_guard_position = true;

              CommandService::MoveOptions opts;
              opts.clear_attack_intent = false;
              opts.allow_direct_fallback = true;
              std::vector<Engine::Core::EntityID> const ids = {
                  entity->get_id()};
              std::vector<QVector3D> const targets = {
                  QVector3D(new_guard_x, 0.0F, new_guard_z)};
              CommandService::move_units(*world, ids, targets, opts);
            }
          }
        }
      }
    } else {

      if (!guard_mode->returning_to_guard_position) {
        float const dx = guard_mode->guard_position_x - transform->position.x;
        float const dz = guard_mode->guard_position_z - transform->position.z;
        float const dist_sq = dx * dx + dz * dz;

        float const k_return_threshold_sq =
            Engine::Core::Defaults::kGuardReturnThreshold *
            Engine::Core::Defaults::kGuardReturnThreshold;

        if (dist_sq > k_return_threshold_sq) {
          guard_mode->returning_to_guard_position = true;

          CommandService::MoveOptions opts;
          opts.clear_attack_intent = false;
          opts.allow_direct_fallback = true;
          std::vector<Engine::Core::EntityID> const ids = {entity->get_id()};
          std::vector<QVector3D> const targets = {
              QVector3D(guard_mode->guard_position_x, 0.0F,
                        guard_mode->guard_position_z)};
          CommandService::move_units(*world, ids, targets, opts);
        }
      }
    }
  }
}

} // namespace Game::Systems
