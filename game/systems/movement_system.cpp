#include "movement_system.h"
#include "command_service.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

static constexpr int MAX_WAYPOINT_SKIP_COUNT = 4;

void MovementSystem::update(Engine::Core::World *world, float deltaTime) {
  CommandService::processPathResults(*world);
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
    float arriveRadius = std::clamp(maxSpeed * deltaTime * 2.0f, 0.05f, 0.25f);
    float arriveRadiusSq = arriveRadius * arriveRadius;

    float dx = movement->targetX - transform->position.x;
    float dz = movement->targetY - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safetyCounter = MAX_WAYPOINT_SKIP_COUNT;
    while (movement->hasTarget && dist2 < arriveRadiusSq &&
           safetyCounter-- > 0) {
      if (!movement->path.empty()) {
        movement->path.erase(movement->path.begin());
        if (!movement->path.empty()) {
          movement->targetX = movement->path.front().first;
          movement->targetY = movement->path.front().second;
          dx = movement->targetX - transform->position.x;
          dz = movement->targetY - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      transform->position.x = movement->targetX;
      transform->position.z = movement->targetY;
      movement->hasTarget = false;
      movement->vx = movement->vz = 0.0f;
      break;
    }

    if (!movement->hasTarget) {
      movement->vx *= std::max(0.0f, 1.0f - damping * deltaTime);
      movement->vz *= std::max(0.0f, 1.0f - damping * deltaTime);
    } else {
      float distance = std::sqrt(std::max(dist2, 0.0f));
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
    } else if (transform->hasDesiredYaw) {

      float current = transform->rotation.y;
      float targetYaw = transform->desiredYaw;
      float diff = std::fmod((targetYaw - current + 540.0f), 360.0f) - 180.0f;
      float turnSpeed = 180.0f;
      float step =
          std::clamp(diff, -turnSpeed * deltaTime, turnSpeed * deltaTime);
      transform->rotation.y = current + step;

      if (std::fabs(diff) < 0.5f) {
        transform->hasDesiredYaw = false;
      }
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