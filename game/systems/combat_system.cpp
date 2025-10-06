#include "combat_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include <algorithm>
#include <limits>

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World *world, float deltaTime) {
  processAttacks(world, deltaTime);
}

void CombatSystem::processAttacks(Engine::Core::World *world, float deltaTime) {
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  ArrowSystem *arrowSys = nullptr;
  for (auto &sys : world->systems()) {
    arrowSys = dynamic_cast<ArrowSystem *>(sys.get());
    if (arrowSys)
      break;
  }

  for (auto attacker : units) {
    auto attackerUnit = attacker->getComponent<Engine::Core::UnitComponent>();
    auto attackerTransform =
        attacker->getComponent<Engine::Core::TransformComponent>();
    auto attackerAtk = attacker->getComponent<Engine::Core::AttackComponent>();

    if (!attackerUnit || !attackerTransform) {
      continue;
    }

    if (attackerUnit->health <= 0)
      continue;

    float range = 2.0f;
    int damage = 10;
    float cooldown = 1.0f;
    float *tAccum = nullptr;
    float tmpAccum = 0.0f;
    if (attackerAtk) {
      range = std::max(0.1f, attackerAtk->range);
      damage = std::max(0, attackerAtk->damage);
      cooldown = std::max(0.05f, attackerAtk->cooldown);
      attackerAtk->timeSinceLast += deltaTime;
      tAccum = &attackerAtk->timeSinceLast;
    } else {
      tmpAccum += deltaTime;
      tAccum = &tmpAccum;
    }
    if (*tAccum < cooldown) {
      continue;
    }

    auto *attackTarget =
        attacker->getComponent<Engine::Core::AttackTargetComponent>();
    Engine::Core::Entity *bestTarget = nullptr;

    if (attackTarget && attackTarget->targetId != 0) {

      auto *target = world->getEntity(attackTarget->targetId);
      if (target) {
        auto *targetUnit = target->getComponent<Engine::Core::UnitComponent>();

        if (targetUnit && targetUnit->health > 0 &&
            targetUnit->ownerId != attackerUnit->ownerId) {

          if (isInRange(attacker, target, range)) {
            bestTarget = target;

            if (auto *attT =
                    attacker
                        ->getComponent<Engine::Core::TransformComponent>()) {
              if (auto *tgtT =
                      target
                          ->getComponent<Engine::Core::TransformComponent>()) {
                float dx = tgtT->position.x - attT->position.x;
                float dz = tgtT->position.z - attT->position.z;
                float yaw = std::atan2(dx, dz) * 180.0f / 3.14159265f;
                attT->desiredYaw = yaw;
                attT->hasDesiredYaw = true;
              }
            }
          } else if (attackTarget->shouldChase) {

            auto *movement =
                attacker->getComponent<Engine::Core::MovementComponent>();
            if (!movement) {
              movement =
                  attacker->addComponent<Engine::Core::MovementComponent>();
            }
            if (movement) {
              auto *targetTransform =
                  target->getComponent<Engine::Core::TransformComponent>();
              if (targetTransform) {
                movement->targetX = targetTransform->position.x;
                movement->targetY = targetTransform->position.z;
                movement->hasTarget = true;
              }
            }

            if (isInRange(attacker, target, range)) {
              bestTarget = target;
            }
          } else {

            attacker->removeComponent<Engine::Core::AttackTargetComponent>();
          }
        } else {

          attacker->removeComponent<Engine::Core::AttackTargetComponent>();
        }
      } else {

        attacker->removeComponent<Engine::Core::AttackTargetComponent>();
      }
    }

    if (!bestTarget && !attackTarget) {

      for (auto target : units) {
        if (target == attacker) {
          continue;
        }

        auto targetUnit = target->getComponent<Engine::Core::UnitComponent>();
        if (!targetUnit || targetUnit->health <= 0) {
          continue;
        }

        if (targetUnit->ownerId == attackerUnit->ownerId) {
          continue;
        }

        if (target->hasComponent<Engine::Core::BuildingComponent>()) {
          continue;
        }

        if (isInRange(attacker, target, range)) {
          bestTarget = target;
          break;
        }
      }
    }

    if (bestTarget) {
      auto *bestTargetUnit =
          bestTarget->getComponent<Engine::Core::UnitComponent>();

      if (!attacker->hasComponent<Engine::Core::AttackTargetComponent>()) {
        auto *newTarget =
            attacker->addComponent<Engine::Core::AttackTargetComponent>();
        newTarget->targetId = bestTarget->getId();
        newTarget->shouldChase = false;
      } else {
        auto *existingTarget =
            attacker->getComponent<Engine::Core::AttackTargetComponent>();
        if (existingTarget->targetId != bestTarget->getId()) {
          existingTarget->targetId = bestTarget->getId();
          existingTarget->shouldChase = false;
        }
      }

      if (auto *attT =
              attacker->getComponent<Engine::Core::TransformComponent>()) {
        if (auto *tgtT =
                bestTarget->getComponent<Engine::Core::TransformComponent>()) {
          float dx = tgtT->position.x - attT->position.x;
          float dz = tgtT->position.z - attT->position.z;
          float yaw = std::atan2(dx, dz) * 180.0f / 3.14159265f;
          attT->desiredYaw = yaw;
          attT->hasDesiredYaw = true;
        }
      }

      if (arrowSys) {
        auto attT = attacker->getComponent<Engine::Core::TransformComponent>();
        auto tgtT =
            bestTarget->getComponent<Engine::Core::TransformComponent>();
        auto attU = attacker->getComponent<Engine::Core::UnitComponent>();
        QVector3D aPos(attT->position.x, attT->position.y, attT->position.z);
        QVector3D tPos(tgtT->position.x, tgtT->position.y, tgtT->position.z);
        QVector3D dir = (tPos - aPos).normalized();

        QVector3D start = aPos + QVector3D(0.0f, 0.6f, 0.0f) + dir * 0.35f;
        QVector3D end = tPos + QVector3D(0.5f, 0.5f, 0.0f);
        QVector3D color = attU ? Game::Visuals::teamColorForOwner(attU->ownerId)
                               : QVector3D(0.8f, 0.9f, 1.0f);
        arrowSys->spawnArrow(start, end, color, 14.0f);
      }
      dealDamage(bestTarget, damage);
      *tAccum = 0.0f;
    } else {

      if (!attackTarget &&
          attacker->hasComponent<Engine::Core::AttackTargetComponent>()) {
        attacker->removeComponent<Engine::Core::AttackTargetComponent>();
      }
    }
  }
}

bool CombatSystem::isInRange(Engine::Core::Entity *attacker,
                             Engine::Core::Entity *target, float range) {
  auto attackerTransform =
      attacker->getComponent<Engine::Core::TransformComponent>();
  auto targetTransform =
      target->getComponent<Engine::Core::TransformComponent>();

  if (!attackerTransform || !targetTransform) {
    return false;
  }

  float dx = targetTransform->position.x - attackerTransform->position.x;
  float dz = targetTransform->position.z - attackerTransform->position.z;
  float distanceSquared = dx * dx + dz * dz;

  float targetRadius = 0.0f;
  if (target->hasComponent<Engine::Core::BuildingComponent>()) {

    float scaleX = targetTransform->scale.x;
    float scaleZ = targetTransform->scale.z;
    targetRadius = std::max(scaleX, scaleZ) * 0.5f;
  } else {

    float scaleX = targetTransform->scale.x;
    float scaleZ = targetTransform->scale.z;
    targetRadius = std::max(scaleX, scaleZ) * 0.5f;
  }

  float effectiveRange = range + targetRadius;

  return distanceSquared <= effectiveRange * effectiveRange;
}

void CombatSystem::dealDamage(Engine::Core::Entity *target, int damage) {
  auto unit = target->getComponent<Engine::Core::UnitComponent>();
  if (unit) {
    unit->health = std::max(0, unit->health - damage);

    if (unit->health <= 0) {

      if (auto *r = target->getComponent<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }
    }
  }
}

} // namespace Game::Systems