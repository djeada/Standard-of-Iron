#include "movement_system.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

void MovementSystem::update(Engine::Core::World *world, float deltaTime) {
  auto entities = world->getEntitiesWith<Engine::Core::MovementComponent>();

  for (auto entity : entities) {
    moveUnit(entity, deltaTime);
  }
}

void MovementSystem::moveUnit(Engine::Core::Entity *entity, float deltaTime) {
  auto transform = entity->getComponent<Engine::Core::TransformComponent>();
  auto movement = entity->getComponent<Engine::Core::MovementComponent>();
  auto unit = entity->getComponent<Engine::Core::UnitComponent>();

  if (!transform || !movement || !unit) {
    return;
  }

  const float maxSpeed = std::max(0.1f, unit->speed);
  const float accel = maxSpeed * 4.0f;
  const float damping = 6.0f;

  if (!movement->hasTarget) {
    movement->vx *= std::max(0.0f, 1.0f - damping * deltaTime);
    movement->vz *= std::max(0.0f, 1.0f - damping * deltaTime);
  } else {
    float dx = movement->targetX - transform->position.x;
    float dz = movement->targetY - transform->position.z;
    float dist2 = dx * dx + dz * dz;
    float distance = std::sqrt(dist2);

    const float arriveRadius = 0.25f;
    if (distance < arriveRadius) {

      transform->position.x = movement->targetX;
      transform->position.z = movement->targetY;
      movement->hasTarget = false;
      movement->vx = movement->vz = 0.0f;
    } else {

      float nx = dx / std::max(0.0001f, distance);
      float nz = dz / std::max(0.0001f, distance);
      float desiredSpeed = maxSpeed;
      float slowRadius = arriveRadius * 4.0f;
      if (distance < slowRadius) {
        desiredSpeed = maxSpeed * (distance / slowRadius);
      }
      float desiredVx = nx * desiredSpeed;
      float desiredVz = nz * desiredSpeed;

      float ax = (desiredVx - movement->vx) * accel;
      float az = (desiredVz - movement->vz) * accel;
      movement->vx += ax * deltaTime;
      movement->vz += az * deltaTime;

      movement->vx *= std::max(0.0f, 1.0f - 0.5f * damping * deltaTime);
      movement->vz *= std::max(0.0f, 1.0f - 0.5f * damping * deltaTime);
    }
  }

  transform->position.x += movement->vx * deltaTime;
  transform->position.z += movement->vz * deltaTime;

  if (!entity->hasComponent<Engine::Core::BuildingComponent>()) {
    float speed2 = movement->vx * movement->vx + movement->vz * movement->vz;
    if (speed2 > 1e-5f) {
      float targetYaw =
          std::atan2(movement->vx, movement->vz) * 180.0f / 3.14159265f;

      float current = transform->rotation.y;

      float diff = std::fmod((targetYaw - current + 540.0f), 360.0f) - 180.0f;
      float turnSpeed = 720.0f;
      float step =
          std::clamp(diff, -turnSpeed * deltaTime, turnSpeed * deltaTime);
      transform->rotation.y = current + step;
    }
  }
}

bool MovementSystem::hasReachedTarget(
    const Engine::Core::TransformComponent *transform,
    const Engine::Core::MovementComponent *movement) {
  float dx = movement->targetX - transform->position.x;
  float dz = movement->targetY - transform->position.z;
  float distanceSquared = dx * dx + dz * dz;

  const float threshold = 0.1f;
  return distanceSquared < threshold * threshold;
}

} // namespace Game::Systems