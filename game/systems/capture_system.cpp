#include "capture_system.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "../visuals/team_colors.h"
#include "building_collision_registry.h"
#include <cmath>
#include <iostream>

namespace Game::Systems {

CaptureSystem::CaptureSystem(Engine::Core::World &world) : m_world(world) {}

void CaptureSystem::update(Engine::Core::World *world, float deltaTime) {
  processBarrackCapture(deltaTime);
}

int CaptureSystem::countNearbyTroops(float barrackX, float barrackZ,
                                     int ownerId, float radius) {
  int totalTroops = 0;
  auto entities = m_world.getEntitiesWith<Engine::Core::UnitComponent>();

  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    auto *transform = e->getComponent<Engine::Core::TransformComponent>();

    if (!unit || !transform || unit->health <= 0)
      continue;

    if (unit->ownerId != ownerId)
      continue;

    if (unit->unitType == "barracks")
      continue;

    float dx = transform->position.x - barrackX;
    float dz = transform->position.z - barrackZ;
    float distSq = dx * dx + dz * dz;

    if (distSq <= radius * radius) {
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->unitType);
      totalTroops += individualsPerUnit;
    }
  }

  return totalTroops;
}

void CaptureSystem::transferBarrackOwnership(Engine::Core::Entity *barrack,
                                             int newOwnerId) {
  auto *unit = barrack->getComponent<Engine::Core::UnitComponent>();
  auto *renderable = barrack->getComponent<Engine::Core::RenderableComponent>();
  auto *transform = barrack->getComponent<Engine::Core::TransformComponent>();
  auto *prod = barrack->getComponent<Engine::Core::ProductionComponent>();

  if (!unit || !renderable || !transform)
    return;

  int previousOwnerId = unit->ownerId;
  unit->ownerId = newOwnerId;

  QVector3D tc = Game::Visuals::teamColorForOwner(newOwnerId);
  renderable->color[0] = tc.x();
  renderable->color[1] = tc.y();
  renderable->color[2] = tc.z();

  Game::Systems::BuildingCollisionRegistry::instance().updateBuildingOwner(
      barrack->getId(), newOwnerId);

  if (!Game::Core::isNeutralOwner(newOwnerId) && !prod) {
    prod = barrack->addComponent<Engine::Core::ProductionComponent>();
    if (prod) {
      prod->productType = "archer";
      prod->buildTime = 10.0f;
      prod->maxUnits = 150;
      prod->inProgress = false;
      prod->timeRemaining = 0.0f;
      prod->producedCount = 0;
      prod->rallyX = transform->position.x + 4.0f;
      prod->rallyZ = transform->position.z + 2.0f;
      prod->rallySet = true;
      prod->villagerCost =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              prod->productType);
    }
  } else if (Game::Core::isNeutralOwner(newOwnerId) && prod) {
    barrack->removeComponent<Engine::Core::ProductionComponent>();
  }

  Engine::Core::EventManager::instance().publish(
      Engine::Core::BarrackCapturedEvent(barrack->getId(), previousOwnerId,
                                         newOwnerId));
}

void CaptureSystem::processBarrackCapture(float deltaTime) {
  constexpr float CAPTURE_RADIUS = 8.0f;
  constexpr int TROOP_ADVANTAGE_MULTIPLIER = 3;

  auto barracks = m_world.getEntitiesWith<Engine::Core::BuildingComponent>();

  for (auto *barrack : barracks) {
    auto *unit = barrack->getComponent<Engine::Core::UnitComponent>();
    auto *transform = barrack->getComponent<Engine::Core::TransformComponent>();

    if (!unit || !transform)
      continue;

    if (unit->unitType != "barracks")
      continue;

    auto *capture = barrack->getComponent<Engine::Core::CaptureComponent>();
    if (!capture) {
      capture = barrack->addComponent<Engine::Core::CaptureComponent>();
    }

    float barrackX = transform->position.x;
    float barrackZ = transform->position.z;
    int barrackOwnerId = unit->ownerId;

    int maxEnemyTroops = 0;
    int capturingPlayerId = -1;

    auto entities = m_world.getEntitiesWith<Engine::Core::UnitComponent>();
    std::vector<int> playerIds;
    for (auto *e : entities) {
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if (u && u->ownerId != barrackOwnerId &&
          !Game::Core::isNeutralOwner(u->ownerId)) {
        if (std::find(playerIds.begin(), playerIds.end(), u->ownerId) ==
            playerIds.end()) {
          playerIds.push_back(u->ownerId);
        }
      }
    }

    for (int playerId : playerIds) {
      int troopCount = countNearbyTroops(barrackX, barrackZ, playerId,
                                         CAPTURE_RADIUS);
      if (troopCount > maxEnemyTroops) {
        maxEnemyTroops = troopCount;
        capturingPlayerId = playerId;
      }
    }

    int defenderTroops = 0;
    if (!Game::Core::isNeutralOwner(barrackOwnerId)) {
      defenderTroops =
          countNearbyTroops(barrackX, barrackZ, barrackOwnerId, CAPTURE_RADIUS);
    }

    bool canCapture =
        maxEnemyTroops >= (defenderTroops * TROOP_ADVANTAGE_MULTIPLIER);

    if (canCapture && capturingPlayerId != -1) {
      if (capture->capturingPlayerId != capturingPlayerId) {
        capture->capturingPlayerId = capturingPlayerId;
        capture->captureProgress = 0.0f;
      }

      capture->isBeingCaptured = true;
      capture->captureProgress += deltaTime;

      if (capture->captureProgress >= capture->requiredTime) {
        transferBarrackOwnership(barrack, capturingPlayerId);
        capture->captureProgress = 0.0f;
        capture->isBeingCaptured = false;
        capture->capturingPlayerId = -1;
      }
    } else {
      if (capture->isBeingCaptured) {
        capture->captureProgress -= deltaTime * 2.0f;
        if (capture->captureProgress <= 0.0f) {
          capture->captureProgress = 0.0f;
          capture->isBeingCaptured = false;
          capture->capturingPlayerId = -1;
        }
      }
    }
  }
}

} // namespace Game::Systems
