#include "guard_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "formation_system.h"
#include "nation_registry.h"
#include <QVector3D>
#include <cmath>
#include <map>

namespace Game::Systems {

void GuardSystem::update(Engine::Core::World *world, float deltaTime) {
  if (!world)
    return;

  auto entities = world->getEntitiesWith<Engine::Core::GuardComponent>();

  std::map<std::pair<int, int>, std::vector<Engine::Core::Entity *>>
      guardGroups;

  for (auto *entity : entities) {
    auto *guard = entity->getComponent<Engine::Core::GuardComponent>();
    if (!guard || !guard->isGuarding)
      continue;

    int gridX = static_cast<int>(guard->guardX / 5.0f);
    int gridZ = static_cast<int>(guard->guardZ / 5.0f);
    guardGroups[{gridX, gridZ}].push_back(entity);
  }

  for (const auto &[gridKey, groupEntities] : guardGroups) {
    if (groupEntities.empty())
      continue;

    auto *firstEntity = groupEntities[0];
    auto *firstGuard =
        firstEntity->getComponent<Engine::Core::GuardComponent>();
    auto *firstUnit = firstEntity->getComponent<Engine::Core::UnitComponent>();

    if (!firstGuard || !firstUnit)
      continue;

    QVector3D centerPos(firstGuard->guardX, 0.0f, firstGuard->guardZ);

    const Nation *nation =
        NationRegistry::instance().getNationForPlayer(firstUnit->ownerId);
    FormationType formationType = FormationType::Roman;
    if (nation) {
      formationType = nation->formationType;
    }

    auto formationPositions = FormationSystem::instance().getFormationPositions(
        formationType, static_cast<int>(groupEntities.size()), centerPos, 3.0f);

    for (size_t i = 0; i < groupEntities.size(); ++i) {
      auto *entity = groupEntities[i];
      auto *guard = entity->getComponent<Engine::Core::GuardComponent>();
      auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
      auto *transform =
          entity->getComponent<Engine::Core::TransformComponent>();
      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();

      if (!guard || !movement || !transform || !unit)
        continue;

      if (unit->health <= 0) {
        guard->isGuarding = false;
        continue;
      }

      auto *attackTarget =
          entity->getComponent<Engine::Core::AttackTargetComponent>();

      if (attackTarget && attackTarget->targetId != 0) {
        auto *targetEntity = world->getEntity(attackTarget->targetId);
        if (targetEntity) {
          auto *targetUnit =
              targetEntity->getComponent<Engine::Core::UnitComponent>();
          if (targetUnit && targetUnit->health > 0) {
            guard->returnToPosition = true;
            continue;
          }
        }

        entity->removeComponent<Engine::Core::AttackTargetComponent>();
        attackTarget = nullptr;
        guard->returnToPosition = true;
      }

      bool enemyInRange = false;
      auto allEntities = world->getEntitiesWith<Engine::Core::UnitComponent>();
      for (auto *other : allEntities) {
        auto *otherUnit = other->getComponent<Engine::Core::UnitComponent>();
        auto *otherTransform =
            other->getComponent<Engine::Core::TransformComponent>();

        if (!otherUnit || !otherTransform || otherUnit->health <= 0)
          continue;
        if (otherUnit->ownerId == unit->ownerId)
          continue;

        float dx = otherTransform->position.x - guard->guardX;
        float dz = otherTransform->position.z - guard->guardZ;
        float distSq = dx * dx + dz * dz;
        float radiusSq = guard->guardRadius * guard->guardRadius;

        if (distSq < radiusSq) {
          enemyInRange = true;

          if (!attackTarget) {
            entity->addComponent<Engine::Core::AttackTargetComponent>();
            attackTarget =
                entity->getComponent<Engine::Core::AttackTargetComponent>();
          }
          if (attackTarget) {
            attackTarget->targetId = other->getId();
            attackTarget->shouldChase = true;
          }
          guard->returnToPosition = true;
          break;
        }
      }

      if (enemyInRange) {
        continue;
      }

      QVector3D targetPos =
          (i < formationPositions.size()) ? formationPositions[i] : centerPos;

      if (guard->returnToPosition) {
        float dx = targetPos.x() - transform->position.x;
        float dz = targetPos.z() - transform->position.z;
        float distSq = dx * dx + dz * dz;

        if (distSq < 4.0f) {
          guard->returnToPosition = false;
          movement->hasTarget = false;
          continue;
        }

        movement->hasTarget = true;
        movement->targetX = targetPos.x();
        movement->targetY = targetPos.z();
        movement->goalX = targetPos.x();
        movement->goalY = targetPos.z();
      } else {
        float dx = targetPos.x() - transform->position.x;
        float dz = targetPos.z() - transform->position.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > 4.0f) {
          movement->hasTarget = true;
          movement->targetX = targetPos.x();
          movement->targetY = targetPos.z();
          movement->goalX = targetPos.x();
          movement->goalY = targetPos.z();
        }
      }
    }
  }
}

} // namespace Game::Systems
