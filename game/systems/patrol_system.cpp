#include "patrol_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include <QVector3D>
#include <cmath>

namespace Game::Systems {

void PatrolSystem::update(Engine::Core::World *world, float deltaTime) {
  if (!world)
    return;

  auto entities = world->getEntitiesWith<Engine::Core::PatrolComponent>();

  for (auto *entity : entities) {
    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();

    if (!patrol || !movement || !transform || !unit)
      continue;
    if (!patrol->patrolling || patrol->waypoints.size() < 2)
      continue;

    if (unit->health <= 0) {
      patrol->patrolling = false;
      continue;
    }

    auto *attackTarget =
        entity->getComponent<Engine::Core::AttackTargetComponent>();
    if (attackTarget && attackTarget->targetId != 0) {

      continue;
    }

    bool enemyNearby = false;
    auto allEntities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto *other : allEntities) {
      auto *otherUnit = other->getComponent<Engine::Core::UnitComponent>();
      auto *otherTransform =
          other->getComponent<Engine::Core::TransformComponent>();

      if (!otherUnit || !otherTransform || otherUnit->health <= 0)
        continue;
      if (otherUnit->ownerId == unit->ownerId)
        continue;

      float dx = otherTransform->position.x - transform->position.x;
      float dz = otherTransform->position.z - transform->position.z;
      float distSq = dx * dx + dz * dz;

      if (distSq < 25.0f) {
        enemyNearby = true;

        if (!attackTarget) {
          entity->addComponent<Engine::Core::AttackTargetComponent>();
          attackTarget =
              entity->getComponent<Engine::Core::AttackTargetComponent>();
        }
        if (attackTarget) {
          attackTarget->targetId = other->getId();
          attackTarget->shouldChase = false;
        }
        break;
      }
    }

    if (enemyNearby) {

      continue;
    }

    auto waypoint = patrol->waypoints[patrol->currentWaypoint];
    float targetX = waypoint.first;
    float targetZ = waypoint.second;

    float dx = targetX - transform->position.x;
    float dz = targetZ - transform->position.z;
    float distSq = dx * dx + dz * dz;

    if (distSq < 1.0f) {

      patrol->currentWaypoint =
          (patrol->currentWaypoint + 1) % patrol->waypoints.size();
      waypoint = patrol->waypoints[patrol->currentWaypoint];
      targetX = waypoint.first;
      targetZ = waypoint.second;
    }

    movement->hasTarget = true;
    movement->targetX = targetX;
    movement->targetY = targetZ;
  }
}

} // namespace Game::Systems
