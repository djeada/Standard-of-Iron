#include "ai_system.h"
#include "../core/world.h"
#include "ai_system/behaviors/attack_behavior.h"
#include "ai_system/behaviors/defend_behavior.h"
#include "ai_system/behaviors/gather_behavior.h"
#include "ai_system/behaviors/production_behavior.h"
#include "ai_system/behaviors/retreat_behavior.h"
#include "core/event_manager.h"
#include "owner_registry.h"
#include "systems/ai_system/ai_command_applier.h"
#include "systems/ai_system/ai_snapshot_builder.h"
#include "systems/ai_system/ai_types.h"
#include "systems/ai_system/ai_worker.h"
#include <cstdint>
#include <memory>
#include <queue>
#include <utility>

namespace Game::Systems {

AISystem::AISystem() {

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::RetreatBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::DefendBehavior>());

  m_behaviorRegistry.registerBehavior(
      std::make_unique<AI::ProductionBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::AttackBehavior>());

  m_behaviorRegistry.registerBehavior(std::make_unique<AI::GatherBehavior>());

  m_buildingAttackedSubscription = Engine::Core::ScopedEventSubscription<
      Engine::Core::BuildingAttackedEvent>(
      [this](const Engine::Core::BuildingAttackedEvent &event) {
        this->onBuildingAttacked(event);
      });

  initializeAIPlayers();
}

void AISystem::reinitialize() {
  m_aiInstances.clear();

  initializeAIPlayers();
}

void AISystem::initializeAIPlayers() {
  auto &registry = OwnerRegistry::instance();
  const auto &ai_owner_ids = registry.getAIOwnerIds();

  if (ai_owner_ids.empty()) {
    return;
  }

  for (uint32_t const player_id : ai_owner_ids) {
    int const team_id = registry.getOwnerTeam(player_id);
    AIInstance instance;
    instance.context.player_id = player_id;
    instance.context.state = AI::AIState::Idle;
    instance.worker = std::make_unique<AI::AIWorker>(m_reasoner, m_executor,
                                                     m_behaviorRegistry);

    m_aiInstances.push_back(std::move(instance));
  }
}

AISystem::~AISystem() = default;

void AISystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_totalGameTime += delta_time;

  m_commandFilter.update(m_totalGameTime);

  processResults(*world);

  for (auto &ai : m_aiInstances) {

    ai.updateTimer += delta_time;

    if (ai.updateTimer < 0.3F) {
      continue;
    }

    if (ai.worker->busy()) {
      continue;
    }

    AI::AISnapshot snapshot = Game::Systems::AI::AISnapshotBuilder::build(
        *world, ai.context.player_id);
    snapshot.gameTime = m_totalGameTime;

    AI::AIJob job;
    job.snapshot = std::move(snapshot);
    job.context = ai.context;
    job.delta_time = ai.updateTimer;

    if (ai.worker->trySubmit(std::move(job))) {
      ai.updateTimer = 0.0F;
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

      auto filtered_commands =
          m_commandFilter.filter(result.commands, m_totalGameTime);

      Game::Systems::AI::AICommandApplier::apply(world, ai.context.player_id,
                                                 filtered_commands);

      results.pop();
    }
  }
}

void AISystem::onBuildingAttacked(
    const Engine::Core::BuildingAttackedEvent &event) {
  for (auto &ai : m_aiInstances) {
    if (ai.context.player_id == event.owner_id) {
      ai.context.buildingsUnderAttack[event.buildingId] = m_totalGameTime;

      if (event.buildingId == ai.context.primaryBarracks) {
        ai.context.barracksUnderThreat = true;
      }
      break;
    }
  }
}

} // namespace Game::Systems
