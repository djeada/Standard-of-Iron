#include "ai_system.h"
#include "../core/world.h"
#include "ai_system/behaviors/attack_behavior.h"
#include "ai_system/behaviors/defend_behavior.h"
#include "ai_system/behaviors/gather_behavior.h"
#include "ai_system/behaviors/production_behavior.h"
#include "ai_system/behaviors/retreat_behavior.h"
#include "owner_registry.h"

namespace Game::Systems {

AISystem::AISystem() {

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::RetreatBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::DefendBehavior>());

  m_behaviorRegistry.registerBehavior(
      std::make_unique<AI::ProductionBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::AttackBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::GatherBehavior>());

  initializeAIPlayers();
}

void AISystem::reinitialize() {
  m_aiInstances.clear();

  initializeAIPlayers();
}

void AISystem::initializeAIPlayers() {
  auto &registry = OwnerRegistry::instance();
  const auto &aiOwnerIds = registry.getAIOwnerIds();

  if (aiOwnerIds.empty()) {
    return;
  }

  for (uint32_t playerId : aiOwnerIds) {
    int teamId = registry.getOwnerTeam(playerId);
    AIInstance instance;
    instance.context.playerId = playerId;
    instance.context.state = AI::AIState::Idle;
    instance.worker = std::make_unique<AI::AIWorker>(m_reasoner, m_executor,
                                                     m_behaviorRegistry);

    m_aiInstances.push_back(std::move(instance));
  }
}

AISystem::~AISystem() {}

void AISystem::update(Engine::Core::World *world, float deltaTime) {
  if (!world)
    return;

  m_totalGameTime += deltaTime;

  m_commandFilter.update(m_totalGameTime);

  processResults(*world);

  for (auto &ai : m_aiInstances) {

    ai.updateTimer += deltaTime;

    if (ai.updateTimer < 0.3f)
      continue;

    if (ai.worker->busy())
      continue;

    AI::AISnapshot snapshot =
        m_snapshotBuilder.build(*world, ai.context.playerId);

    AI::AIJob job;
    job.snapshot = std::move(snapshot);
    job.context = ai.context;
    job.deltaTime = ai.updateTimer;

    if (ai.worker->trySubmit(std::move(job))) {
      ai.updateTimer = 0.0f;
    }
  }
}

void AISystem::processResults(Engine::Core::World &world) {

  for (auto &ai : m_aiInstances) {

    std::queue<AI::AIResult> results;
    ai.worker->drainResults(results);

    while (!results.empty()) {
      auto &result = results.front();

      ai.context = result.context;

      auto filteredCommands =
          m_commandFilter.filter(result.commands, m_totalGameTime);

      m_applier.apply(world, ai.context.playerId, filteredCommands);

      results.pop();
    }
  }
}

} // namespace Game::Systems
