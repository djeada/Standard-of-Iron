#include "ai_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "command_service.h"
#include "formation_planner.h"
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

  m_aiThread = std::thread(&AISystem::workerLoop, this);
}

AISystem::~AISystem() {
  m_shouldStop.store(true, std::memory_order_release);
  m_jobCondition.notify_all();
  if (m_aiThread.joinable()) {
    m_aiThread.join();
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

  processResults(world);

  m_globalUpdateTimer += deltaTime;
  if (m_globalUpdateTimer < 0.5f)
    return;

  if (m_workerBusy.load(std::memory_order_acquire))
    return;

  AISnapshot snapshot = buildSnapshot(world);

  AIJob job;
  job.snapshot = std::move(snapshot);
  job.context = m_enemyAI;
  job.deltaTime = m_globalUpdateTimer;

  {
    std::lock_guard<std::mutex> lock(m_jobMutex);
    m_pendingJob = std::move(job);
    m_hasPendingJob = true;
  }

  m_workerBusy.store(true, std::memory_order_release);
  m_jobCondition.notify_one();

  m_globalUpdateTimer = 0.0f;
}

AISnapshot AISystem::buildSnapshot(Engine::Core::World *world) const {
  AISnapshot snapshot;
  snapshot.playerId = m_enemyAI.playerId;

  auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
  snapshot.friendlies.reserve(entities.size());

  for (auto *entity : entities) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    EntitySnapshot data;
    data.id = entity->getId();
    data.unitType = unit->unitType;
    data.ownerId = unit->ownerId;
    data.health = unit->health;
    data.maxHealth = unit->maxHealth;
    data.isBuilding = entity->hasComponent<Engine::Core::BuildingComponent>();

    if (auto *transform =
            entity->getComponent<Engine::Core::TransformComponent>()) {
      data.position =
          QVector3D(transform->position.x, 0.0f, transform->position.z);
    }

    if (auto *movement =
            entity->getComponent<Engine::Core::MovementComponent>()) {
      data.movement.hasComponent = true;
      data.movement.hasTarget = movement->hasTarget;
    }

    if (auto *production =
            entity->getComponent<Engine::Core::ProductionComponent>()) {
      data.production.hasComponent = true;
      data.production.inProgress = production->inProgress;
      data.production.buildTime = production->buildTime;
      data.production.timeRemaining = production->timeRemaining;
      data.production.producedCount = production->producedCount;
      data.production.maxUnits = production->maxUnits;
      data.production.productType = production->productType;
      data.production.rallySet = production->rallySet;
      data.production.rallyX = production->rallyX;
      data.production.rallyZ = production->rallyZ;
    }

    if (unit->ownerId == snapshot.playerId) {
      snapshot.friendlies.push_back(std::move(data));
    } else if (data.isBuilding) {
      snapshot.enemyBuildings.push_back(std::move(data));
    } else {
      snapshot.enemyUnits.push_back(std::move(data));
    }
  }

  return snapshot;
}

void AISystem::processResults(Engine::Core::World *world) {
  std::vector<AIResult> results;
  {
    std::lock_guard<std::mutex> lock(m_resultMutex);
    while (!m_results.empty()) {
      results.push_back(std::move(m_results.front()));
      m_results.pop();
    }
  }

  for (auto &result : results) {
    m_enemyAI = result.context;
    applyCommands(world, result.commands);
  }
}

void AISystem::applyCommands(Engine::Core::World *world,
                             const std::vector<AICommand> &commands) {
  for (const auto &command : commands) {
    switch (command.type) {
    case AICommandType::MoveUnits:
      if (!command.units.empty() &&
          command.units.size() == command.moveTargets.size()) {

        CommandService::MoveOptions opts;
        opts.allowDirectFallback = false;
        opts.clearAttackIntent = true;
        CommandService::moveUnits(*world, command.units, command.moveTargets,
                                  opts);
      }
      break;
    case AICommandType::AttackTarget:
      if (!command.units.empty() && command.targetId != 0) {
        CommandService::attackTarget(*world, command.units, command.targetId,
                                     command.shouldChase);
      }
      break;
    case AICommandType::StartProduction: {
      auto *entity = world->getEntity(command.buildingId);
      if (!entity)
        break;
      auto *production =
          entity->getComponent<Engine::Core::ProductionComponent>();
      if (!production || production->inProgress)
        break;
      production->productType = command.productType;
      production->timeRemaining = production->buildTime;
      production->inProgress = true;
      break;
    }
    }
  }
}

void AISystem::updateContext(const AISnapshot &snapshot, AIContext &ctx) {
  ctx.militaryUnits.clear();
  ctx.buildings.clear();
  ctx.primaryBarracks = 0;
  ctx.totalUnits = 0;
  ctx.idleUnits = 0;
  ctx.combatUnits = 0;
  ctx.averageHealth = 1.0f;
  ctx.rallyX = 0.0f;
  ctx.rallyZ = 0.0f;

  float totalHealthRatio = 0.0f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      ctx.buildings.push_back(entity.id);
      if (entity.unitType == "barracks" && ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = entity.id;
        ctx.rallyX = entity.position.x() - 5.0f;
        ctx.rallyZ = entity.position.z();
      }
      continue;
    }

    ctx.militaryUnits.push_back(entity.id);
    ctx.totalUnits++;

    if (!entity.movement.hasComponent || !entity.movement.hasTarget) {
      ctx.idleUnits++;
    } else {
      ctx.combatUnits++;
    }

    if (entity.maxHealth > 0) {
      totalHealthRatio += static_cast<float>(entity.health) /
                          static_cast<float>(entity.maxHealth);
    }
  }

  if (ctx.totalUnits > 0) {
    ctx.averageHealth = totalHealthRatio / static_cast<float>(ctx.totalUnits);
  } else {
    ctx.averageHealth = 1.0f;
  }
}

void AISystem::updateStateMachine(AIContext &ctx, float deltaTime) {
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
    } else if (ctx.averageHealth < 0.5f && ctx.totalUnits > 0) {
      ctx.state = AIState::Defending;
    }
    break;
  case AIState::Gathering:
    if (ctx.totalUnits >= 5 && ctx.idleUnits < 2) {
      ctx.state = AIState::Attacking;
    } else if (ctx.totalUnits < 2) {
      ctx.state = AIState::Idle;
    }
    break;
  case AIState::Attacking:
    if (ctx.averageHealth < 0.3f) {
      ctx.state = AIState::Retreating;
    } else if (ctx.totalUnits < 3) {
      ctx.state = AIState::Gathering;
    }
    break;
  case AIState::Defending:
    if (ctx.averageHealth > 0.7f) {
      ctx.state = AIState::Idle;
    } else if (ctx.totalUnits >= 5 && ctx.averageHealth > 0.5f) {
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

void AISystem::executeBehaviors(const AISnapshot &snapshot, AIContext &ctx,
                                float deltaTime,
                                std::vector<AICommand> &outCommands) {
  bool exclusiveBehaviorExecuted = false;

  for (auto &behavior : m_behaviors) {
    if (!behavior)
      continue;

    if (exclusiveBehaviorExecuted && !behavior->canRunConcurrently()) {
      continue;
    }

    if (behavior->shouldExecute(snapshot, ctx)) {
      behavior->execute(snapshot, ctx, deltaTime, outCommands);
      if (!behavior->canRunConcurrently()) {
        exclusiveBehaviorExecuted = true;
      }
    }
  }
}

void AISystem::workerLoop() {
  while (true) {
    AIJob job;
    {
      std::unique_lock<std::mutex> lock(m_jobMutex);
      m_jobCondition.wait(lock, [this]() {
        return m_shouldStop.load(std::memory_order_acquire) || m_hasPendingJob;
      });

      if (m_shouldStop.load(std::memory_order_acquire) && !m_hasPendingJob) {
        break;
      }

      job = std::move(m_pendingJob);
      m_hasPendingJob = false;
    }

    AIResult result;
    result.context = job.context;

    updateContext(job.snapshot, result.context);
    updateStateMachine(result.context, job.deltaTime);
    executeBehaviors(job.snapshot, result.context, job.deltaTime,
                     result.commands);

    {
      std::lock_guard<std::mutex> lock(m_resultMutex);
      m_results.push(std::move(result));
    }

    m_workerBusy.store(false, std::memory_order_release);
  }
}

void ProductionBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                 float deltaTime,
                                 std::vector<AICommand> &outCommands) {
  m_productionTimer += deltaTime;
  if (m_productionTimer < 2.0f)
    return;
  m_productionTimer = 0.0f;

  for (const auto &entity : snapshot.friendlies) {
    if (!entity.isBuilding || entity.unitType != "barracks")
      continue;
    if (!entity.production.hasComponent)
      continue;
    const auto &prod = entity.production;
    if (prod.inProgress || prod.producedCount >= prod.maxUnits)
      continue;

    AICommand command;
    command.type = AICommandType::StartProduction;
    command.buildingId = entity.id;
    command.productType = "archer";
    outCommands.push_back(std::move(command));
  }
}

bool ProductionBehavior::shouldExecute(const AISnapshot &snapshot,
                                       const AIContext &context) const {
  (void)snapshot;
  (void)context;
  return true;
}

void GatherBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_gatherTimer += deltaTime;
  if (m_gatherTimer < 4.0f)
    return;
  m_gatherTimer = 0.0f;

  if (context.primaryBarracks == 0)
    return;

  QVector3D rallyPoint(context.rallyX, 0.0f, context.rallyZ);

  std::vector<const EntitySnapshot *> idleEntities;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (!entity.movement.hasComponent || !entity.movement.hasTarget) {
      idleEntities.push_back(&entity);
    }
  }

  if (idleEntities.empty())
    return;

  auto formationTargets = FormationPlanner::spreadFormation(
      static_cast<int>(idleEntities.size()), rallyPoint, 1.2f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<QVector3D> targetsToUse;
  unitsToMove.reserve(idleEntities.size());
  targetsToUse.reserve(idleEntities.size());

  for (size_t i = 0; i < idleEntities.size(); ++i) {
    const auto *entity = idleEntities[i];
    const auto &target = formationTargets[i];
    float dx = entity->position.x() - target.x();
    float dz = entity->position.z() - target.z();
    float distanceSq = dx * dx + dz * dz;
    if (distanceSq < 0.25f * 0.25f)
      continue;
    unitsToMove.push_back(entity->id);
    targetsToUse.push_back(target);
  }

  if (unitsToMove.empty())
    return;

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(unitsToMove);
  command.moveTargets = std::move(targetsToUse);
  outCommands.push_back(std::move(command));
}

bool GatherBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  (void)context;
  int idleCount = 0;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (!entity.movement.hasComponent || !entity.movement.hasTarget) {
      idleCount++;
    }
  }
  return idleCount >= 2;
}

void AttackBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_attackTimer += deltaTime;
  if (m_attackTimer < 3.0f)
    return;
  m_attackTimer = 0.0f;

  Engine::Core::EntityID targetId = 0;
  if (!snapshot.enemyBuildings.empty()) {
    targetId = snapshot.enemyBuildings.front().id;
  } else if (!snapshot.enemyUnits.empty()) {
    targetId = snapshot.enemyUnits.front().id;
  }

  if (targetId == 0)
    return;

  std::vector<Engine::Core::EntityID> attackers;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    attackers.push_back(entity.id);
  }

  if (attackers.empty())
    return;

  AICommand command;
  command.type = AICommandType::AttackTarget;
  command.units = std::move(attackers);
  command.targetId = targetId;
  command.shouldChase = true;
  outCommands.push_back(std::move(command));
}

bool AttackBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  (void)context;
  int unitCount = 0;
  for (const auto &entity : snapshot.friendlies) {
    if (!entity.isBuilding)
      unitCount++;
  }
  return unitCount >= 4;
}

void DefendBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_defendTimer += deltaTime;
  if (m_defendTimer < 3.0f)
    return;
  m_defendTimer = 0.0f;

  if (context.primaryBarracks == 0)
    return;

  QVector3D defendPos;
  bool foundBarracks = false;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.id == context.primaryBarracks) {
      defendPos = entity.position;
      foundBarracks = true;
      break;
    }
  }

  if (!foundBarracks)
    return;

  std::vector<const EntitySnapshot *> defenders;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    defenders.push_back(&entity);
  }

  if (defenders.empty())
    return;

  auto targets = FormationPlanner::spreadFormation(
      static_cast<int>(defenders.size()), defendPos, 3.0f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<QVector3D> targetsToUse;
  unitsToMove.reserve(defenders.size());
  targetsToUse.reserve(defenders.size());

  for (size_t i = 0; i < defenders.size(); ++i) {
    const auto *entity = defenders[i];
    const auto &target = targets[i];
    float dx = entity->position.x() - target.x();
    float dz = entity->position.z() - target.z();
    float distanceSq = dx * dx + dz * dz;
    if (distanceSq < 1.0f * 1.0f)
      continue;
    unitsToMove.push_back(entity->id);
    targetsToUse.push_back(target);
  }

  if (unitsToMove.empty())
    return;

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(unitsToMove);
  command.moveTargets = std::move(targetsToUse);
  outCommands.push_back(std::move(command));
}

bool DefendBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  (void)snapshot;
  return context.averageHealth < 0.6f;
}

} // namespace Game::Systems