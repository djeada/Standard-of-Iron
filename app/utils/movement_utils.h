#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"

namespace App::Utils {

inline void reset_movement(Engine::Core::Entity *entity) {
  if (entity == nullptr) {
    return;
  }

  auto *movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return;
  }

  auto *transform = entity->get_component<Engine::Core::TransformComponent>();
  movement->has_target = false;
  movement->path.clear();
  movement->path_pending = false;
  movement->pending_request_id = 0;
  movement->repath_cooldown = 0.0F;
  if (transform != nullptr) {
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;
    movement->goal_x = transform->position.x;
    movement->goal_y = transform->position.z;
  } else {
    movement->target_x = 0.0F;
    movement->target_y = 0.0F;
    movement->goal_x = 0.0F;
    movement->goal_y = 0.0F;
  }
}

} // namespace App::Utils
