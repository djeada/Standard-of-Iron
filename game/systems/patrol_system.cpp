#include "patrol_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include <QVector3D>
#include <cmath>

namespace Game::Systems {

void PatrolSystem::update(Engine::Core::World *world, float) {
  if (world == nullptr) {
    return;
  }

  auto entities = world->getEntitiesWith<Engine::Core::PatrolComponent>();

  for (auto *entity : entities) {
    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();

    if ((patrol == nullptr) || (movement == nullptr) ||
        (transform == nullptr) || (unit == nullptr)) {
      continue;
    }
    if (!patrol->patrolling || patrol->waypoints.size() < 2) {
      continue;
    }

    if (unit->health <= 0) {
      patrol->patrolling = false;
      continue;
    }

    auto *attack_target =
        entity->getComponent<Engine::Core::AttackTargetComponent>();
    if ((attack_target != nullptr) && attack_target->target_id != 0) {

      continue;
    }

    bool enemy_nearby = false;
    auto all_entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto *other : all_entities) {
      auto *other_unit = other->getComponent<Engine::Core::UnitComponent>();
      auto *other_transform =
          other->getComponent<Engine::Core::TransformComponent>();

      if ((other_unit == nullptr) || (other_transform == nullptr) ||
          other_unit->health <= 0) {
        continue;
      }
      if (other_unit->owner_id == unit->owner_id) {
        continue;
      }

      float const dx = other_transform->position.x - transform->position.x;
      float const dz = other_transform->position.z - transform->position.z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq < 25.0F) {
        enemy_nearby = true;

        if (attack_target == nullptr) {
          entity->addComponent<Engine::Core::AttackTargetComponent>();
          attack_target =
              entity->getComponent<Engine::Core::AttackTargetComponent>();
        }
        if (attack_target != nullptr) {
          attack_target->target_id = other->getId();
          attack_target->shouldChase = false;
        }
        break;
      }
    }

    if (enemy_nearby) {

      continue;
    }

    auto waypoint = patrol->waypoints[patrol->currentWaypoint];
    float target_x = waypoint.first;
    float target_z = waypoint.second;

    float const dx = target_x - transform->position.x;
    float const dz = target_z - transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < 1.0F) {

      patrol->currentWaypoint =
          (patrol->currentWaypoint + 1) % patrol->waypoints.size();
      waypoint = patrol->waypoints[patrol->currentWaypoint];
      target_x = waypoint.first;
      target_z = waypoint.second;
    }

    movement->hasTarget = true;
    movement->target_x = target_x;
    movement->target_y = target_z;
    movement->goalX = target_x;
    movement->goalY = target_z;
  }
}

} // namespace Game::Systems
