#include "ai_system.h"
#include "../core/world.h"
#include "ai_system/behaviors/attack_behavior.h"
#include "ai_system/behaviors/builder_behavior.h"
#include "ai_system/behaviors/defend_behavior.h"
#include "ai_system/behaviors/expand_behavior.h"
#include "ai_system/behaviors/gather_behavior.h"
#include "ai_system/behaviors/production_behavior.h"
#include "ai_system/behaviors/retreat_behavior.h"
#include "ai_system/ai_strategy.h"
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

  m_behaviorRegistry.register_behavior(std::make_unique<AI::RetreatBehavior>());

  m_behaviorRegistry.register_behavior(std::make_unique<AI::DefendBehavior>());

  m_behaviorRegistry.register_behavior(
      std::make_unique<AI::ProductionBehavior>());

  m_behaviorRegistry.register_behavior(std::make_unique<AI::BuilderBehavior>());

  m_behaviorRegistry.register_behavior(std::make_unique<AI::ExpandBehavior>());

  m_behaviorRegistry.register_behavior(std::make_unique<AI::AttackBehavior>());

  m_behaviorRegistry.register_behavior(std::make_unique<AI::GatherBehavior>());

  m_buildingAttackedSubscription = Engine::Core::ScopedEventSubscription<
      Engine::Core::BuildingAttackedEvent>(
      [this](const Engine::Core::BuildingAttackedEvent &event) {
        this->on_building_attacked(event);
      });

  initialize_ai_players();
}

void AISystem::reinitialize() {
  m_aiInstances.clear();

  initialize_ai_players();
}

void AISystem::initialize_ai_players() {
  auto &registry = OwnerRegistry::instance();
  const auto &ai_owner_ids = registry.get_ai_owner_ids();

  if (ai_owner_ids.empty()) {
    return;
  }

  for (uint32_t const player_id : ai_owner_ids) {
    int const team_id = registry.get_owner_team(player_id);
    AIInstance instance;
    instance.context.player_id = player_id;
    instance.context.state = AI::AIState::Idle;
    instance.worker = std::make_unique<AI::AIWorker>(m_reasoner, m_executor,
                                                     m_behaviorRegistry);

    m_aiInstances.push_back(std::move(instance));
  }
}

AISystem::~AISystem() = default;

void AISystem::set_ai_strategy(int player_id, AI::AIStrategy strategy,
                               float aggression, float defense,
                               float harassment) {
  for (auto &ai : m_aiInstances) {
    if (ai.context.player_id == player_id) {
      ai.context.strategy_config = AI::AIStrategyFactory::create_config(strategy);
      AI::AIStrategyFactory::apply_personality(ai.context.strategy_config,
                                              aggression, defense, harassment);
      break;
    }
  }
}

void AISystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_total_game_time += delta_time;

  m_commandFilter.update(m_total_game_time);

  process_results(*world);

  for (auto &ai : m_aiInstances) {

    ai.update_timer += delta_time;

    if (ai.update_timer < m_update_interval) {
      continue;
    }

    if (ai.worker->busy()) {
      continue;
    }

    AI::AISnapshot snapshot = Game::Systems::AI::AISnapshotBuilder::build(
        *world, ai.context.player_id);
    snapshot.game_time = m_total_game_time;

    AI::AIJob job;
    job.snapshot = std::move(snapshot);
    job.context = ai.context;
    job.delta_time = ai.update_timer;

    if (ai.worker->try_submit(std::move(job))) {
      ai.update_timer = 0.0F;
    }
  }
}

void AISystem::process_results(Engine::Core::World &world) {

  for (auto &ai : m_aiInstances) {

    std::queue<AI::AIResult> results;
    ai.worker->drain_results(results);

    while (!results.empty()) {
      auto &result = results.front();

      ai.context = result.context;

      auto filtered_commands =
          m_commandFilter.filter(result.commands, m_total_game_time);

      Game::Systems::AI::AICommandApplier::apply(world, ai.context.player_id,
                                                 filtered_commands);

      results.pop();
    }
  }
}

void AISystem::on_building_attacked(
    const Engine::Core::BuildingAttackedEvent &event) {
  for (auto &ai : m_aiInstances) {
    if (ai.context.player_id == event.owner_id) {
      ai.context.buildings_under_attack[event.buildingId] = m_total_game_time;

      if (event.buildingId == ai.context.primary_barracks) {
        ai.context.barracks_under_threat = true;
      }
      break;
    }
  }
}

} // namespace Game::Systems
