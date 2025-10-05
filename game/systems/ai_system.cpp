#include "ai_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "command_service.h"
#include "formation_planner.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Game::Systems {

AISystem::AISystem() {

  m_enemyAI.playerId = 2;
  m_enemyAI.state = AIState::Idle;

  registerBehavior(std::make_unique<DefendBehavior>());

  registerBehavior(std::make_unique<ProductionBehavior>());

  registerBehavior(std::make_unique<AttackBehavior>());

  registerBehavior(std::make_unique<GatherBehavior>());
}

AISystem::~AISystem() {

  if (m_aiThread && m_aiThread->joinable()) {
    m_shouldStop = true;
    m_aiCondition.notify_all();
    m_aiThread->join();
  }
}

void AISystem::registerBehavior(std::unique_ptr<AIBehavior> behavior) {
  m_behaviors.push_back(std::move(behavior));

  std::sort(m_behaviors.begin(), m_behaviors.end(),
            [](const std::unique_ptr<AIBehavior> &a,
               const std::unique_ptr<AIBehavior> &b) {
              return a->getPriority() > b->getPriority();
            });
}

void AISystem::update(Engine::Core::World *world, float deltaTime) {
  if (!world)
    return;

  m_globalUpdateTimer += deltaTime;
  if (m_globalUpdateTimer < 0.5f)
    return;
  deltaTime = m_globalUpdateTimer;
  m_globalUpdateTimer = 0.0f;

  updateContext(world, m_enemyAI);

  updateStateMachine(world, m_enemyAI, deltaTime);

  if (m_useThreading) {
    executeBehaviorsThreaded(world, deltaTime);
  } else {
    executeBehaviors(world, deltaTime);
  }
}

void AISystem::executeBehaviors(Engine::Core::World *world, float deltaTime) {

  bool exclusiveBehaviorExecuted = false;

  for (auto &behavior : m_behaviors) {
    if (!behavior)
      continue;

    if (exclusiveBehaviorExecuted && !behavior->canRunConcurrently()) {
      continue;
    }

    if (behavior->shouldExecute(world, m_enemyAI.playerId)) {
      behavior->execute(world, m_enemyAI.playerId, deltaTime);

      if (!behavior->canRunConcurrently()) {
        exclusiveBehaviorExecuted = true;
      }
    }
  }
}

void AISystem::executeBehaviorsThreaded(Engine::Core::World *world,
                                        float deltaTime) {

  executeBehaviors(world, deltaTime);
}

void AISystem::updateContext(Engine::Core::World *world, AIContext &ctx) {

  ctx.militaryUnits.clear();
  ctx.buildings.clear();
  ctx.totalUnits = 0;
  ctx.idleUnits = 0;
  ctx.combatUnits = 0;
  float totalHealthRatio = 0.0f;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != ctx.playerId || u->health <= 0)
      continue;

    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      ctx.buildings.push_back(e->getId());
      if (u->unitType == "barracks" && ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = e->getId();

        if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
          ctx.rallyX = t->position.x - 5.0f;
          ctx.rallyZ = t->position.z;
        }
      }
    } else {
      ctx.militaryUnits.push_back(e->getId());
      ctx.totalUnits++;

      auto *m = e->getComponent<Engine::Core::MovementComponent>();
      if (!m || !m->hasTarget) {
        ctx.idleUnits++;
      } else {
        ctx.combatUnits++;
      }

      if (u->maxHealth > 0) {
        totalHealthRatio += float(u->health) / float(u->maxHealth);
      }
    }
  }

  if (ctx.totalUnits > 0) {
    ctx.averageHealth = totalHealthRatio / float(ctx.totalUnits);
  } else {
    ctx.averageHealth = 1.0f;
  }
}

void AISystem::updateStateMachine(Engine::Core::World *world, AIContext &ctx,
                                  float deltaTime) {
  ctx.stateTimer += deltaTime;
  ctx.decisionTimer += deltaTime;

  if (ctx.decisionTimer < 5.0f)
    return;
  ctx.decisionTimer = 0.0f;

  AIState previousState = ctx.state;

  switch (ctx.state) {
  case AIState::Idle:

    if (ctx.idleUnits >= 3) {
      ctx.state = AIState::Gathering;
    }

    else if (ctx.averageHealth < 0.5f && ctx.totalUnits > 0) {
      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Gathering:

    if (ctx.totalUnits >= 5 && ctx.idleUnits < 2) {
      ctx.state = AIState::Attacking;
    }

    else if (ctx.totalUnits < 2) {
      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Attacking:

    if (ctx.averageHealth < 0.3f) {
      ctx.state = AIState::Retreating;
    }

    else if (ctx.totalUnits < 3) {
      ctx.state = AIState::Gathering;
    }
    break;

  case AIState::Defending:

    if (ctx.averageHealth > 0.7f) {
      ctx.state = AIState::Idle;
    }

    else if (ctx.totalUnits >= 5 && ctx.averageHealth > 0.5f) {
      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Retreating:

    if (ctx.stateTimer > 8.0f) {
      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    ctx.state = AIState::Idle;
    break;
  }

  if (ctx.state != previousState) {
    ctx.stateTimer = 0.0f;
  }
}

void ProductionBehavior::execute(Engine::Core::World *world, int playerId,
                                 float deltaTime) {
  m_productionTimer += deltaTime;
  if (m_productionTimer < 2.0f)
    return;
  m_productionTimer = 0.0f;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (u->unitType != "barracks")
      continue;

    auto *prod = e->getComponent<Engine::Core::ProductionComponent>();
    if (!prod)
      continue;

    if (!prod->inProgress && prod->producedCount < prod->maxUnits) {
      prod->productType = "archer";
      prod->timeRemaining = prod->buildTime;
      prod->inProgress = true;
    }
  }
}

bool ProductionBehavior::shouldExecute(Engine::Core::World *world,
                                       int playerId) const {

  return true;
}

void GatherBehavior::execute(Engine::Core::World *world, int playerId,
                             float deltaTime) {
  m_gatherTimer += deltaTime;
  if (m_gatherTimer < 4.0f)
    return;
  m_gatherTimer = 0.0f;

  QVector3D rallyPoint(0.0f, 0.0f, 0.0f);
  bool foundBarracks = false;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (u->unitType == "barracks") {
      if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
        rallyPoint = QVector3D(t->position.x - 5.0f, 0.0f, t->position.z);
        foundBarracks = true;
        break;
      }
    }
  }

  if (!foundBarracks)
    return;

  std::vector<Engine::Core::EntityID> idleUnits;
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    auto *m = e->getComponent<Engine::Core::MovementComponent>();
    if (!m || m->hasTarget)
      continue;

    idleUnits.push_back(e->getId());
  }

  if (idleUnits.empty())
    return;

  auto targets = FormationPlanner::spreadFormation(int(idleUnits.size()),
                                                   rallyPoint, 1.2f);
  CommandService::moveUnits(*world, idleUnits, targets);
}

bool GatherBehavior::shouldExecute(Engine::Core::World *world,
                                   int playerId) const {

  int idleCount = 0;
  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    auto *m = e->getComponent<Engine::Core::MovementComponent>();
    if (!m || !m->hasTarget)
      idleCount++;
  }
  return idleCount >= 2;
}

void AttackBehavior::execute(Engine::Core::World *world, int playerId,
                             float deltaTime) {
  m_attackTimer += deltaTime;
  if (m_attackTimer < 3.0f)
    return;
  m_attackTimer = 0.0f;

  std::vector<Engine::Core::Entity *> enemyBuildings;
  std::vector<Engine::Core::Entity *> enemyUnits;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId == playerId || u->health <= 0)
      continue;

    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      enemyBuildings.push_back(e);
    } else {
      enemyUnits.push_back(e);
    }
  }

  Engine::Core::Entity *mainTarget = nullptr;
  if (!enemyBuildings.empty()) {
    mainTarget = enemyBuildings[0];
  } else if (!enemyUnits.empty()) {
    mainTarget = enemyUnits[0];
  }

  if (!mainTarget)
    return;

  std::vector<Engine::Core::EntityID> attackers;
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    attackers.push_back(e->getId());
  }

  if (attackers.empty())
    return;

  CommandService::attackTarget(*world, attackers, mainTarget->getId(), true);
}

bool AttackBehavior::shouldExecute(Engine::Core::World *world,
                                   int playerId) const {

  int unitCount = 0;
  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (!e->hasComponent<Engine::Core::BuildingComponent>())
      unitCount++;
  }
  return unitCount >= 4;
}

void DefendBehavior::execute(Engine::Core::World *world, int playerId,
                             float deltaTime) {
  m_defendTimer += deltaTime;
  if (m_defendTimer < 3.0f)
    return;
  m_defendTimer = 0.0f;

  QVector3D defendPos(0.0f, 0.0f, 0.0f);
  bool foundBarracks = false;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (u->unitType == "barracks") {
      if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
        defendPos = QVector3D(t->position.x, 0.0f, t->position.z);
        foundBarracks = true;
        break;
      }
    }
  }

  if (!foundBarracks)
    return;

  std::vector<Engine::Core::EntityID> defenders;
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    defenders.push_back(e->getId());
  }

  if (defenders.empty())
    return;

  auto targets =
      FormationPlanner::spreadFormation(int(defenders.size()), defendPos, 3.0f);
  CommandService::moveUnits(*world, defenders, targets);
}

bool DefendBehavior::shouldExecute(Engine::Core::World *world,
                                   int playerId) const {

  float totalHealth = 0.0f;
  int count = 0;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != playerId || u->health <= 0)
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    if (u->maxHealth > 0) {
      totalHealth += float(u->health) / float(u->maxHealth);
      count++;
    }
  }

  if (count == 0)
    return false;
  float avgHealth = totalHealth / float(count);
  return avgHealth < 0.6f;
}

} // namespace Game::Systems