#include "guard_system.h"

#include <QVector3D>

#include <cmath>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "command_service.h"

namespace Game::Systems {

void GuardSystem::run(Engine::Core::SystemContext& context) {
  for (auto [entity_id, guard_mode_ref, movement_ref, transform_ref, unit_ref] :
       context.view<Engine::Core::GuardModeComponent,
                    Engine::Core::MovementComponent,
                    const Engine::Core::TransformComponent,
                    const Engine::Core::UnitComponent>()) {
    auto* guard_mode = &guard_mode_ref;
    const auto* movement = &movement_ref;
    const auto* transform = &transform_ref;
    const auto* unit = &unit_ref;

    if (!guard_mode->active || !guard_mode->has_guard_target) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    const auto* attack_target =
        context.try_get<Engine::Core::AttackTargetComponent>(entity_id);
    if ((attack_target != nullptr) && attack_target->target_id != 0) {
      continue;
    }

    if (guard_mode->guarded_entity_id != 0) {
      {
        const auto* guarded_transform =
            context.try_get<Engine::Core::TransformComponent>(
                guard_mode->guarded_entity_id);
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
                movement->get_has_target() &&
                std::abs(movement->get_goal_x() - new_guard_x) < 0.5F &&
                std::abs(movement->get_goal_y() - new_guard_z) < 0.5F;

            if (!already_moving_to_target) {
              guard_mode->returning_to_guard_position = true;

              CommandService::MoveOptions opts;
              opts.kind = MoveOrderKind::GuardReturn;
              std::vector<Engine::Core::EntityID> const ids = {entity_id};
              std::vector<QVector3D> const targets = {
                  QVector3D(new_guard_x, 0.0F, new_guard_z)};
              CommandService::move_units(context.world(), ids, targets, opts);
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
            Engine::Core::Defaults::k_guard_return_threshold *
            Engine::Core::Defaults::k_guard_return_threshold;

        if (dist_sq > k_return_threshold_sq) {
          guard_mode->returning_to_guard_position = true;

          CommandService::MoveOptions opts;
          opts.kind = MoveOrderKind::GuardReturn;
          std::vector<Engine::Core::EntityID> const ids = {entity_id};
          std::vector<QVector3D> const targets = {QVector3D(
              guard_mode->guard_position_x, 0.0F, guard_mode->guard_position_z)};
          CommandService::move_units(context.world(), ids, targets, opts);
        }
      }
    }
  }
}

auto GuardSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            TransformComponent,
            AttackTargetComponent,
            BuildingComponent,
            PendingRemovalComponent>{},
      Writes<GuardModeComponent, MovementComponent, AttackComponent>{});
}

} // namespace Game::Systems
