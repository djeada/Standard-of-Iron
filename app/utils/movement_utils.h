#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"

namespace App::Utils {

inline void reset_movement(Engine::Core::Entity *entity) {
  if (entity == nullptr) {
    return;
  }

  auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return;
  }

  auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
  movement->hasTarget = false;
  movement->path.clear();
  movement->pathPending = false;
  movement->pendingRequestId = 0;
  movement->repathCooldown = 0.0F;
  if (transform != nullptr) {
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;
    movement->goalX = transform->position.x;
    movement->goalY = transform->position.z;
  } else {
    movement->target_x = 0.0F;
    movement->target_y = 0.0F;
    movement->goalX = 0.0F;
    movement->goalY = 0.0F;
  }
}

} // namespace App::Utils
