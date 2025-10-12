
#include "ai_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "command_service.h"
#include "formation_planner.h"
#include "owner_registry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Game::Systems {

AISystem::AISystem() {
  auto &registry = OwnerRegistry::instance();
  m_enemyAI.playerId = registry.getAIOwnerIds().empty()
                           ? registry.registerOwner(OwnerType::AI, "AI Player")
                           : registry.getAIOwnerIds()[0];
  m_enemyAI.state = AIState::Idle;

  registerBehavior(std::make_unique<DefendBehavior>());
  registerBehavior(std::make_unique<ProductionBehavior>());
  registerBehavior(std::make_unique<AttackBehavior>());
  registerBehavior(std::make_unique<GatherBehavior>());

  m_aiThread = std::thread(&AISystem::workerLoop, this);
}

AISystem::~AISystem() {
  m_shouldStop.store(true, std::memory_order_release);
  { std::lock_guard<std::mutex> lock(m_jobMutex); }
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
  if (m_globalUpdateTimer < 0.3f)
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

  auto friendlies = world->getUnitsOwnedBy(snapshot.playerId);
  snapshot.friendlies.reserve(friendlies.size());

  for (auto *entity : friendlies) {
    if (!entity->hasComponent<Engine::Core::AIControlledComponent>())
      continue;

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

    snapshot.friendlies.push_back(std::move(data));
  }

  auto others = world->getUnitsNotOwnedBy(snapshot.playerId);
  snapshot.visibleEnemies.reserve(others.size());

  for (auto *entity : others) {
    if (entity->hasComponent<Engine::Core::AIControlledComponent>())
      continue;

    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform)
      continue;

    ContactSnapshot contact;
    contact.id = entity->getId();
    contact.isBuilding =
        entity->hasComponent<Engine::Core::BuildingComponent>();
    contact.position =
        QVector3D(transform->position.x, 0.0f, transform->position.z);
    snapshot.visibleEnemies.push_back(std::move(contact));
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

static void replicateLastTargetIfNeeded(const std::vector<QVector3D> &from,
                                        size_t wanted,
                                        std::vector<QVector3D> &out) {
  out.clear();
  if (from.empty())
    return;
  out.reserve(wanted);
  for (size_t i = 0; i < wanted; ++i) {
    out.push_back(i < from.size() ? from[i] : from.back());
  }
}

static bool isEntityEngaged(const EntitySnapshot &entity,
                            const std::vector<ContactSnapshot> &enemies) {
  if (entity.maxHealth > 0 && entity.health < entity.maxHealth)
    return true;

  constexpr float ENGAGED_RADIUS = 7.5f;
  const float engagedSq = ENGAGED_RADIUS * ENGAGED_RADIUS;

  for (const auto &enemy : enemies) {
    float distSq = (enemy.position - entity.position).lengthSquared();
    if (distSq <= engagedSq)
      return true;
  }
  return false;
}

void AISystem::applyCommands(Engine::Core::World *world,
                             const std::vector<AICommand> &commands) {
  if (!world)
    return;
  const int aiOwnerId = m_enemyAI.playerId;

  for (const auto &command : commands) {
    switch (command.type) {
    case AICommandType::MoveUnits: {
      if (command.units.empty())
        break;

      std::vector<QVector3D> expandedTargets;
      if (command.moveTargets.size() != command.units.size()) {
        replicateLastTargetIfNeeded(command.moveTargets, command.units.size(),
                                    expandedTargets);
      } else {
        expandedTargets = command.moveTargets;
      }

      if (expandedTargets.empty())
        break;

      std::vector<Engine::Core::EntityID> ownedUnits;
      std::vector<QVector3D> ownedTargets;
      ownedUnits.reserve(command.units.size());
      ownedTargets.reserve(command.units.size());

      for (std::size_t idx = 0; idx < command.units.size(); ++idx) {
        auto entityId = command.units[idx];
        auto *entity = world->getEntity(entityId);
        if (!entity)
          continue;

        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId)
          continue;

        ownedUnits.push_back(entityId);
        ownedTargets.push_back(expandedTargets[idx]);
      }

      if (ownedUnits.empty())
        break;

      CommandService::MoveOptions opts;
      opts.allowDirectFallback = true;
      opts.clearAttackIntent = false;
      opts.groupMove = ownedUnits.size() > 1;
      CommandService::moveUnits(*world, ownedUnits, ownedTargets, opts);
      break;
    }
    case AICommandType::AttackTarget: {
      if (command.units.empty() || command.targetId == 0)
        break;
      std::vector<Engine::Core::EntityID> ownedUnits;
      ownedUnits.reserve(command.units.size());

      for (auto entityId : command.units) {
        auto *entity = world->getEntity(entityId);
        if (!entity)
          continue;
        auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->ownerId != aiOwnerId)
          continue;
        ownedUnits.push_back(entityId);
      }

      if (ownedUnits.empty())
        break;

      CommandService::attackTarget(*world, ownedUnits, command.targetId,
                                   command.shouldChase);
      break;
    }
    case AICommandType::StartProduction: {
      auto *entity = world->getEntity(command.buildingId);
      if (!entity)
        break;

      auto *production =
          entity->getComponent<Engine::Core::ProductionComponent>();
      if (!production || production->inProgress)
        break;

      auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && unit->ownerId != aiOwnerId)
        break;

      if (!command.productType.empty())
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
  ctx.barracksUnderThreat = false;
  ctx.nearbyThreatCount = 0;
  ctx.closestThreatDistance = std::numeric_limits<float>::infinity();
  ctx.basePosition = QVector3D();

  float totalHealthRatio = 0.0f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      ctx.buildings.push_back(entity.id);
      if (entity.unitType == "barracks" && ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = entity.id;
        ctx.rallyX = entity.position.x() - 5.0f;
        ctx.rallyZ = entity.position.z();
        ctx.basePosition = entity.position;
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

  ctx.averageHealth =
      (ctx.totalUnits > 0)
          ? (totalHealthRatio / static_cast<float>(ctx.totalUnits))
          : 1.0f;

  if (ctx.primaryBarracks != 0) {
    constexpr float DEFEND_RADIUS = 16.0f;
    const float defendRadiusSq = DEFEND_RADIUS * DEFEND_RADIUS;

    for (const auto &enemy : snapshot.visibleEnemies) {
      float distSq = (enemy.position - ctx.basePosition).lengthSquared();
      if (distSq <= defendRadiusSq) {
        ctx.barracksUnderThreat = true;
        ctx.nearbyThreatCount++;
        float dist = std::sqrt(std::max(distSq, 0.0f));
        ctx.closestThreatDistance = std::min(ctx.closestThreatDistance, dist);
      }
    }

    if (!ctx.barracksUnderThreat) {
      ctx.closestThreatDistance = std::numeric_limits<float>::infinity();
    }
  }
}

void AISystem::updateStateMachine(AIContext &ctx, float deltaTime) {
  ctx.stateTimer += deltaTime;
  ctx.decisionTimer += deltaTime;

  AIState previousState = ctx.state;
  if (ctx.barracksUnderThreat && ctx.state != AIState::Defending) {
    ctx.state = AIState::Defending;
  }

  if (ctx.decisionTimer < 2.0f) {
    if (ctx.state != previousState)
      ctx.stateTimer = 0.0f;
    return;
  }
  ctx.decisionTimer = 0.0f;
  previousState = ctx.state;

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idleUnits >= 2) {
      ctx.state = AIState::Gathering;
    } else if (ctx.averageHealth < 0.5f && ctx.totalUnits > 0) {
      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Gathering:
    if (ctx.totalUnits >= 4 && ctx.idleUnits <= 1) {
      ctx.state = AIState::Attacking;
    } else if (ctx.totalUnits < 2) {
      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Attacking:
    if (ctx.averageHealth < 0.3f) {
      ctx.state = AIState::Retreating;
    } else if (ctx.totalUnits < 2) {
      ctx.state = AIState::Gathering;
    }
    break;

  case AIState::Defending:
    if (ctx.barracksUnderThreat) {

    } else if (ctx.totalUnits >= 4 && ctx.averageHealth > 0.5f) {
      ctx.state = AIState::Attacking;
    } else if (ctx.averageHealth > 0.7f) {
      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:
    if (ctx.stateTimer > 6.0f) {
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

    try {
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
    } catch (...) {
    }

    m_workerBusy.store(false, std::memory_order_release);
  }

  m_workerBusy.store(false, std::memory_order_release);
}

void ProductionBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                                 float deltaTime,
                                 std::vector<AICommand> &outCommands) {
  m_productionTimer += deltaTime;
  if (m_productionTimer < 1.5f)
    return;
  m_productionTimer = 0.0f;

  static bool produceArcher = true;

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
    command.productType = produceArcher ? "archer" : "swordsman";
    outCommands.push_back(std::move(command));
  }

  produceArcher = !produceArcher;
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
  if (m_gatherTimer < 2.0f)
    return;
  m_gatherTimer = 0.0f;

  if (context.primaryBarracks == 0)
    return;

  QVector3D rallyPoint(context.rallyX, 0.0f, context.rallyZ);

  std::vector<const EntitySnapshot *> idleEntities;
  idleEntities.reserve(snapshot.friendlies.size());
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (isEntityEngaged(entity, snapshot.visibleEnemies))
      continue;
    if (!entity.movement.hasComponent || !entity.movement.hasTarget) {
      idleEntities.push_back(&entity);
    }
  }

  if (idleEntities.empty())
    return;

  auto formationTargets = FormationPlanner::spreadFormation(
      static_cast<int>(idleEntities.size()), rallyPoint, 1.4f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<QVector3D> targetsToUse;
  unitsToMove.reserve(idleEntities.size());
  targetsToUse.reserve(idleEntities.size());

  for (size_t i = 0; i < idleEntities.size(); ++i) {
    const auto *entity = idleEntities[i];
    const auto &target = formationTargets[i];
    const float dx = entity->position.x() - target.x();
    const float dz = entity->position.z() - target.z();
    const float distanceSq = dx * dx + dz * dz;
    if (distanceSq < 0.35f * 0.35f)
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
  (void)snapshot;
  if (context.primaryBarracks == 0)
    return false;
  if (context.state == AIState::Retreating)
    return false;
  return context.idleUnits > 0;
}

void AttackBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_attackTimer += deltaTime;
  if (m_attackTimer < 1.25f)
    return;
  m_attackTimer = 0.0f;

  std::vector<const EntitySnapshot *> readyUnits;
  readyUnits.reserve(snapshot.friendlies.size());

  QVector3D groupCenter;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (isEntityEngaged(entity, snapshot.visibleEnemies))
      continue;
    readyUnits.push_back(&entity);
    groupCenter += entity.position;
  }

  if (readyUnits.empty())
    return;

  if (snapshot.visibleEnemies.empty())
    return;

  groupCenter /= static_cast<float>(readyUnits.size());

  Engine::Core::EntityID targetId = 0;
  float bestScore = -std::numeric_limits<float>::infinity();

  auto considerTarget = [&](const ContactSnapshot &enemy) {
    float score = 0.0f;
    float distanceToGroup = (enemy.position - groupCenter).length();
    score -= distanceToGroup;

    if (!enemy.isBuilding)
      score += 4.0f;

    if (context.primaryBarracks != 0) {
      float distanceToBase = (enemy.position - context.basePosition).length();
      score += std::max(0.0f, 12.0f - distanceToBase);
    }

    if (context.state == AIState::Attacking && !enemy.isBuilding)
      score += 2.0f;

    if (score > bestScore) {
      bestScore = score;
      targetId = enemy.id;
    }
  };

  for (const auto &enemy : snapshot.visibleEnemies)
    considerTarget(enemy);

  if (targetId == 0)
    return;

  std::vector<Engine::Core::EntityID> attackers;
  attackers.reserve(readyUnits.size());
  for (const auto *entity : readyUnits)
    attackers.push_back(entity->id);

  if (attackers.empty())
    return;

  AICommand command;
  command.type = AICommandType::AttackTarget;
  command.units = std::move(attackers);
  command.targetId = targetId;
  command.shouldChase =
      (context.state == AIState::Attacking) || context.barracksUnderThreat;
  outCommands.push_back(std::move(command));
}

bool AttackBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  int readyUnits = 0;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (isEntityEngaged(entity, snapshot.visibleEnemies))
      continue;
    ++readyUnits;
  }

  if (readyUnits == 0)
    return false;

  if (context.state == AIState::Retreating)
    return false;

  if (context.state == AIState::Attacking)
    return true;

  const bool hasTargets = !snapshot.visibleEnemies.empty();
  if (!hasTargets)
    return false;

  if (context.state == AIState::Defending) {

    return context.barracksUnderThreat && readyUnits >= 2;
  }

  if (readyUnits >= 2)
    return true;
  return (context.averageHealth > 0.7f && readyUnits >= 1);
}

void DefendBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_defendTimer += deltaTime;
  if (m_defendTimer < 1.5f)
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

  std::vector<const EntitySnapshot *> readyDefenders;
  std::vector<const EntitySnapshot *> engagedDefenders;
  readyDefenders.reserve(snapshot.friendlies.size());
  engagedDefenders.reserve(snapshot.friendlies.size());

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;
    if (isEntityEngaged(entity, snapshot.visibleEnemies)) {
      engagedDefenders.push_back(&entity);
    } else {
      readyDefenders.push_back(&entity);
    }
  }

  if (readyDefenders.empty() && engagedDefenders.empty())
    return;

  auto sortByDistance = [&](std::vector<const EntitySnapshot *> &list) {
    std::sort(list.begin(), list.end(),
              [&](const EntitySnapshot *a, const EntitySnapshot *b) {
                float da = (a->position - defendPos).lengthSquared();
                float db = (b->position - defendPos).lengthSquared();
                return da < db;
              });
  };

  sortByDistance(readyDefenders);
  sortByDistance(engagedDefenders);

  const std::size_t totalAvailable =
      readyDefenders.size() + engagedDefenders.size();
  std::size_t desiredCount = totalAvailable;
  if (context.barracksUnderThreat) {
    desiredCount = std::min<std::size_t>(
        desiredCount,
        static_cast<std::size_t>(std::max(3, context.nearbyThreatCount * 2)));
  } else {
    desiredCount =
        std::min<std::size_t>(desiredCount, static_cast<std::size_t>(6));
  }

  std::size_t readyCount = std::min(desiredCount, readyDefenders.size());
  readyDefenders.resize(readyCount);

  if (readyDefenders.empty())
    return;

  if (context.barracksUnderThreat) {
    Engine::Core::EntityID targetId = 0;
    float bestDistSq = std::numeric_limits<float>::infinity();
    auto considerTarget = [&](const ContactSnapshot &candidate) {
      float d = (candidate.position - defendPos).lengthSquared();
      if (d < bestDistSq) {
        bestDistSq = d;
        targetId = candidate.id;
      }
    };
    for (const auto &enemy : snapshot.visibleEnemies)
      considerTarget(enemy);

    if (targetId != 0) {
      std::vector<Engine::Core::EntityID> units;
      units.reserve(readyDefenders.size());
      for (auto *d : readyDefenders)
        units.push_back(d->id);

      if (!units.empty()) {
        AICommand attack;
        attack.type = AICommandType::AttackTarget;
        attack.units = std::move(units);
        attack.targetId = targetId;
        attack.shouldChase = true;
        outCommands.push_back(std::move(attack));
        return;
      }
    }
  }

  auto targets = FormationPlanner::spreadFormation(
      static_cast<int>(readyDefenders.size()), defendPos, 3.0f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<QVector3D> targetsToUse;
  unitsToMove.reserve(readyDefenders.size());
  targetsToUse.reserve(readyDefenders.size());

  for (size_t i = 0; i < readyDefenders.size(); ++i) {
    const auto *entity = readyDefenders[i];
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
  if (context.primaryBarracks == 0)
    return false;

  if (context.barracksUnderThreat)
    return true;
  if (context.state == AIState::Defending && context.idleUnits > 0)
    return true;
  if (context.averageHealth < 0.6f && context.totalUnits > 0)
    return true;

  return false;
}

} // namespace Game::Systems
