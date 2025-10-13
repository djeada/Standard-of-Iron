#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"

namespace App {
namespace Utils {

inline void resetMovement(Engine::Core::Entity *entity) {
  if (!entity)
    return;

  auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
  if (!movement)
    return;

  auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
  movement->hasTarget = false;
  movement->path.clear();
  movement->pathPending = false;
  movement->pendingRequestId = 0;
  movement->repathCooldown = 0.0f;
  if (transform) {
    movement->targetX = transform->position.x;
    movement->targetY = transform->position.z;
    movement->goalX = transform->position.x;
    movement->goalY = transform->position.z;
  } else {
    movement->targetX = 0.0f;
    movement->targetY = 0.0f;
    movement->goalX = 0.0f;
    movement->goalY = 0.0f;
  }
}

} // namespace Utils
} // namespace App
