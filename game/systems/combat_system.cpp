#include "combat_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "owner_registry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World *world, float deltaTime) {
  processAttacks(world, deltaTime);
}

void CombatSystem::processAttacks(Engine::Core::World *world, float deltaTime) {
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  ArrowSystem *arrowSys = world->getSystem<ArrowSystem>();

  for (auto attacker : units) {
    auto attackerUnit = attacker->getComponent<Engine::Core::UnitComponent>();
    auto attackerTransform =
        attacker->getComponent<Engine::Core::TransformComponent>();
    auto attackerAtk = attacker->getComponent<Engine::Core::AttackComponent>();

    if (!attackerUnit || !attackerTransform) {
      continue;
    }

    if (attackerUnit->health <= 0 ||
        attacker->hasComponent<Engine::Core::PendingRemovalComponent>())
      continue;

    if (attackerAtk && attackerAtk->inMeleeLock) {
      auto *lockTarget = world->getEntity(attackerAtk->meleeLockTargetId);
      if (!lockTarget) {

        attackerAtk->inMeleeLock = false;
        attackerAtk->meleeLockTargetId = 0;
      } else {
        auto *lockTargetUnit =
            lockTarget->getComponent<Engine::Core::UnitComponent>();
        if (!lockTargetUnit || lockTargetUnit->health <= 0) {

          attackerAtk->inMeleeLock = false;
          attackerAtk->meleeLockTargetId = 0;
        } else {

          auto *attT = attackerTransform;
          auto *tgtT =
              lockTarget->getComponent<Engine::Core::TransformComponent>();
          if (attT && tgtT) {
            float dx = tgtT->position.x - attT->position.x;
            float dz = tgtT->position.z - attT->position.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            const float IDEAL_MELEE_DISTANCE = 0.6f;
            const float MAX_MELEE_SEPARATION = 0.9f;

            if (dist > MAX_MELEE_SEPARATION) {
              float pullAmount =
                  (dist - IDEAL_MELEE_DISTANCE) * 0.3f * deltaTime * 5.0f;

              if (dist > 0.001f) {
                QVector3D direction(dx / dist, 0.0f, dz / dist);

                attT->position.x += direction.x() * pullAmount;
                attT->position.z += direction.z() * pullAmount;
              }
            }
          }
        }
      }
    }

    if (attackerAtk && attackerAtk->inMeleeLock &&
        attackerAtk->meleeLockTargetId != 0) {
      auto *lockTarget = world->getEntity(attackerAtk->meleeLockTargetId);
      if (lockTarget) {

        auto *attackTarget =
            attacker->getComponent<Engine::Core::AttackTargetComponent>();
        if (!attackTarget) {
          attackTarget =
              attacker->addComponent<Engine::Core::AttackTargetComponent>();
        }
        if (attackTarget) {
          attackTarget->targetId = attackerAtk->meleeLockTargetId;
          attackTarget->shouldChase = false;
        }
      }
    }

    float range = 2.0f;
    int damage = 10;
    float cooldown = 1.0f;
    float *tAccum = nullptr;
    float tmpAccum = 0.0f;

    if (attackerAtk) {

      updateCombatMode(attacker, world, attackerAtk);

      range = attackerAtk->getCurrentRange();
      damage = attackerAtk->getCurrentDamage();
      cooldown = attackerAtk->getCurrentCooldown();

      auto *holdMode =
          attacker->getComponent<Engine::Core::HoldModeComponent>();
      if (holdMode && holdMode->active && attackerUnit->unitType == "archer") {
        range *= 1.5f;
        damage = static_cast<int>(damage * 1.3f);
      }

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

        auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
        bool isAlly =
            ownerRegistry.areAllies(attackerUnit->ownerId, targetUnit->ownerId);

        if (targetUnit && targetUnit->health > 0 &&
            targetUnit->ownerId != attackerUnit->ownerId && !isAlly) {

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

            auto *holdMode =
                attacker->getComponent<Engine::Core::HoldModeComponent>();
            if (holdMode && holdMode->active) {
              if (!isInRange(attacker, target, range)) {
                attacker
                    ->removeComponent<Engine::Core::AttackTargetComponent>();
              }
              continue;
            }

            auto *targetTransform =
                target->getComponent<Engine::Core::TransformComponent>();
            auto *attackerTransformComponent =
                attacker->getComponent<Engine::Core::TransformComponent>();
            if (targetTransform && attackerTransformComponent) {
              QVector3D attackerPos(attackerTransformComponent->position.x,
                                    0.0f,
                                    attackerTransformComponent->position.z);
              QVector3D targetPos(targetTransform->position.x, 0.0f,
                                  targetTransform->position.z);
              QVector3D desiredPos = targetPos;
              bool holdPosition = false;

              bool targetIsBuilding =
                  target->hasComponent<Engine::Core::BuildingComponent>();
              if (targetIsBuilding) {
                float scaleX = targetTransform->scale.x;
                float scaleZ = targetTransform->scale.z;
                float targetRadius = std::max(scaleX, scaleZ) * 0.5f;
                QVector3D direction = targetPos - attackerPos;
                float distanceSq = direction.lengthSquared();
                if (distanceSq > 0.000001f) {
                  float distance = std::sqrt(distanceSq);
                  direction /= distance;
                  float desiredDistance =
                      targetRadius + std::max(range - 0.2f, 0.2f);
                  if (distance > desiredDistance + 0.15f) {
                    desiredPos = targetPos - direction * desiredDistance;
                  } else {
                    holdPosition = true;
                  }
                }
              }

              auto *movement =
                  attacker->getComponent<Engine::Core::MovementComponent>();
              if (!movement) {
                movement =
                    attacker->addComponent<Engine::Core::MovementComponent>();
              }

              if (movement) {
                if (holdPosition) {
                  movement->hasTarget = false;
                  movement->vx = 0.0f;
                  movement->vz = 0.0f;
                  movement->path.clear();
                  if (attackerTransformComponent) {
                    movement->targetX = attackerTransformComponent->position.x;
                    movement->targetY = attackerTransformComponent->position.z;
                    movement->goalX = attackerTransformComponent->position.x;
                    movement->goalY = attackerTransformComponent->position.z;
                  }
                } else {
                  QVector3D plannedTarget(movement->targetX, 0.0f,
                                          movement->targetY);
                  if (!movement->path.empty()) {
                    const auto &finalNode = movement->path.back();
                    plannedTarget =
                        QVector3D(finalNode.first, 0.0f, finalNode.second);
                  }

                  float diffSq = (plannedTarget - desiredPos).lengthSquared();
                  bool needNewCommand = !movement->pathPending;
                  if (movement->hasTarget && diffSq <= 0.25f * 0.25f) {
                    needNewCommand = false;
                  }

                  if (needNewCommand) {
                    CommandService::MoveOptions options;
                    options.clearAttackIntent = false;

                    options.allowDirectFallback = true;
                    std::vector<Engine::Core::EntityID> unitIds = {
                        attacker->getId()};
                    std::vector<QVector3D> moveTargets = {desiredPos};
                    CommandService::moveUnits(*world, unitIds, moveTargets,
                                              options);
                  }
                }
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

      auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();

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

        if (ownerRegistry.areAllies(attackerUnit->ownerId,
                                    targetUnit->ownerId)) {
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

        if (!attackerAtk ||
            attackerAtk->currentMode !=
                Engine::Core::AttackComponent::CombatMode::Melee) {
          QVector3D aPos(attT->position.x, attT->position.y, attT->position.z);
          QVector3D tPos(tgtT->position.x, tgtT->position.y, tgtT->position.z);
          QVector3D dir = (tPos - aPos).normalized();
          QVector3D color =
              attU ? Game::Visuals::teamColorForOwner(attU->ownerId)
                   : QVector3D(0.8f, 0.9f, 1.0f);

          int arrowCount = 1;
          if (attU) {
            int troopSize =
                Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                    attU->unitType);
            int maxArrows = std::max(1, troopSize / 3);

            static thread_local std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<> dist(1, maxArrows);
            arrowCount = dist(gen);
          }

          for (int i = 0; i < arrowCount; ++i) {
            static thread_local std::mt19937 spreadGen(std::random_device{}());
            std::uniform_real_distribution<float> spreadDist(-0.15f, 0.15f);

            QVector3D perpendicular(-dir.z(), 0.0f, dir.x());
            QVector3D upVector(0.0f, 1.0f, 0.0f);

            float lateralOffset = spreadDist(spreadGen);
            float verticalOffset = spreadDist(spreadGen) * 0.5f;
            float depthOffset = spreadDist(spreadGen) * 0.3f;

            QVector3D startOffset =
                perpendicular * lateralOffset + upVector * verticalOffset;
            QVector3D endOffset = perpendicular * lateralOffset +
                                  upVector * verticalOffset + dir * depthOffset;

            QVector3D start =
                aPos + QVector3D(0.0f, 0.6f, 0.0f) + dir * 0.35f + startOffset;
            QVector3D end = tPos + QVector3D(0.5f, 0.5f, 0.0f) + endOffset;

            arrowSys->spawnArrow(start, end, color, 14.0f);
          }
        }
      }

      if (attackerAtk && attackerAtk->currentMode ==
                             Engine::Core::AttackComponent::CombatMode::Melee) {

        attackerAtk->inMeleeLock = true;
        attackerAtk->meleeLockTargetId = bestTarget->getId();

        auto *targetAtk =
            bestTarget->getComponent<Engine::Core::AttackComponent>();
        if (targetAtk) {
          targetAtk->inMeleeLock = true;
          targetAtk->meleeLockTargetId = attacker->getId();
        }

        auto *attT = attacker->getComponent<Engine::Core::TransformComponent>();
        auto *tgtT =
            bestTarget->getComponent<Engine::Core::TransformComponent>();
        if (attT && tgtT) {
          float dx = tgtT->position.x - attT->position.x;
          float dz = tgtT->position.z - attT->position.z;
          float dist = std::sqrt(dx * dx + dz * dz);

          const float IDEAL_MELEE_DISTANCE = 0.6f;

          if (dist > IDEAL_MELEE_DISTANCE + 0.1f) {

            float moveAmount = (dist - IDEAL_MELEE_DISTANCE) * 0.5f;

            if (dist > 0.001f) {
              QVector3D direction(dx / dist, 0.0f, dz / dist);

              attT->position.x += direction.x() * moveAmount;
              attT->position.z += direction.z() * moveAmount;

              tgtT->position.x -= direction.x() * moveAmount;
              tgtT->position.z -= direction.z() * moveAmount;
            }
          }
        }
      }

      dealDamage(world, bestTarget, damage, attacker->getId());
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
  float dy = targetTransform->position.y - attackerTransform->position.y;
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

  if (distanceSquared > effectiveRange * effectiveRange) {
    return false;
  }

  auto attackerAtk = attacker->getComponent<Engine::Core::AttackComponent>();
  if (attackerAtk && attackerAtk->currentMode ==
                         Engine::Core::AttackComponent::CombatMode::Melee) {
    float heightDiff = std::abs(dy);
    if (heightDiff > attackerAtk->maxHeightDifference) {
      return false;
    }
  }

  return true;
}

void CombatSystem::dealDamage(Engine::Core::World *world,
                              Engine::Core::Entity *target, int damage,
                              Engine::Core::EntityID attackerId) {
  auto unit = target->getComponent<Engine::Core::UnitComponent>();
  if (unit) {
    unit->health = std::max(0, unit->health - damage);

    int attackerOwnerId = 0;
    if (attackerId != 0 && world) {
      auto *attacker = world->getEntity(attackerId);
      if (attacker) {
        auto *attackerUnit =
            attacker->getComponent<Engine::Core::UnitComponent>();
        if (attackerUnit) {
          attackerOwnerId = attackerUnit->ownerId;
        }
      }
    }

    if (target->hasComponent<Engine::Core::BuildingComponent>() &&
        unit->health > 0) {
      Engine::Core::EventManager::instance().publish(
          Engine::Core::BuildingAttackedEvent(target->getId(), unit->ownerId,
                                              unit->unitType, attackerId,
                                              attackerOwnerId, damage));
    }

    if (unit->health <= 0) {

      int killerOwnerId = attackerOwnerId;

      Engine::Core::EventManager::instance().publish(
          Engine::Core::UnitDiedEvent(target->getId(), unit->ownerId,
                                      unit->unitType, attackerId,
                                      killerOwnerId));

      auto *targetAtk = target->getComponent<Engine::Core::AttackComponent>();
      if (targetAtk && targetAtk->inMeleeLock &&
          targetAtk->meleeLockTargetId != 0) {

        if (world) {
          auto *lockPartner = world->getEntity(targetAtk->meleeLockTargetId);
          if (lockPartner) {
            auto *partnerAtk =
                lockPartner->getComponent<Engine::Core::AttackComponent>();
            if (partnerAtk &&
                partnerAtk->meleeLockTargetId == target->getId()) {
              partnerAtk->inMeleeLock = false;
              partnerAtk->meleeLockTargetId = 0;
            }
          }
        }
      }

      if (target->hasComponent<Engine::Core::BuildingComponent>()) {
        BuildingCollisionRegistry::instance().unregisterBuilding(
            target->getId());
      }

      if (auto *r = target->getComponent<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }

      if (auto *movement =
              target->getComponent<Engine::Core::MovementComponent>()) {
        movement->hasTarget = false;
        movement->vx = 0.0f;
        movement->vz = 0.0f;
        movement->path.clear();
        movement->pathPending = false;
      }

      target->addComponent<Engine::Core::PendingRemovalComponent>();
    }
  }
}

void CombatSystem::updateCombatMode(Engine::Core::Entity *attacker,
                                    Engine::Core::World *world,
                                    Engine::Core::AttackComponent *attackComp) {
  if (!attackComp) {
    return;
  }

  if (attackComp->preferredMode !=
      Engine::Core::AttackComponent::CombatMode::Auto) {
    attackComp->currentMode = attackComp->preferredMode;
    return;
  }

  auto attackerTransform =
      attacker->getComponent<Engine::Core::TransformComponent>();
  if (!attackerTransform) {
    return;
  }

  auto attackerUnit = attacker->getComponent<Engine::Core::UnitComponent>();
  if (!attackerUnit) {
    return;
  }

  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

  float closestEnemyDistSq = std::numeric_limits<float>::max();
  float closestHeightDiff = 0.0f;

  for (auto target : units) {
    if (target == attacker) {
      continue;
    }

    auto targetUnit = target->getComponent<Engine::Core::UnitComponent>();
    if (!targetUnit || targetUnit->health <= 0) {
      continue;
    }

    if (ownerRegistry.areAllies(attackerUnit->ownerId, targetUnit->ownerId)) {
      continue;
    }

    auto targetTransform =
        target->getComponent<Engine::Core::TransformComponent>();
    if (!targetTransform) {
      continue;
    }

    float dx = targetTransform->position.x - attackerTransform->position.x;
    float dz = targetTransform->position.z - attackerTransform->position.z;
    float dy = targetTransform->position.y - attackerTransform->position.y;
    float distSq = dx * dx + dz * dz;

    if (distSq < closestEnemyDistSq) {
      closestEnemyDistSq = distSq;
      closestHeightDiff = std::abs(dy);
    }
  }

  if (closestEnemyDistSq == std::numeric_limits<float>::max()) {
    if (attackComp->canRanged) {
      attackComp->currentMode =
          Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attackComp->currentMode =
          Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  float closestDist = std::sqrt(closestEnemyDistSq);

  bool inMeleeRange =
      attackComp->isInMeleeRange(closestDist, closestHeightDiff);
  bool inRangedRange = attackComp->isInRangedRange(closestDist);

  if (inMeleeRange && attackComp->canMelee) {
    attackComp->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  } else if (inRangedRange && attackComp->canRanged) {
    attackComp->currentMode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attackComp->canRanged) {
    attackComp->currentMode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else {
    attackComp->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  }
}

} // namespace Game::Systems