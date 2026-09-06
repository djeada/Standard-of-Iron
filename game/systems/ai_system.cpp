#include "ai_system.h"

#include <QDebug>
#include <queue>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

#include "../core/world.h"
#include "../session/session_context.h"
#include "ai_system/ai_command_applier.h"
#include "ai_system/ai_doctrine_catalog.h"
#include "ai_system/ai_snapshot_builder.h"
#include "ai_system/ai_strategy.h"
#include "ai_system/behaviors/assault_behavior.h"
#include "ai_system/behaviors/attack_behavior.h"
#include "ai_system/behaviors/builder_behavior.h"
#include "ai_system/behaviors/commander_behavior.h"
#include "ai_system/behaviors/defend_behavior.h"
#include "ai_system/behaviors/economy_behavior.h"
#include "ai_system/behaviors/expand_behavior.h"
#include "ai_system/behaviors/gather_behavior.h"
#include "ai_system/behaviors/harass_behavior.h"
#include "ai_system/behaviors/local_engagement_behavior.h"
#include "ai_system/behaviors/production_behavior.h"
#include "ai_system/behaviors/retreat_behavior.h"
#include "ai_system/behaviors/squad_discipline_behavior.h"
#include "core/event_manager.h"
#include "nation_registry.h"
#include "owner_registry.h"
#include "player_resource_registry.h"
#include "systems/ai_system/ai_types.h"
#include "systems/ai_system/ai_worker.h"

namespace Game::Systems {

namespace {

auto initial_ai_update_timer(std::size_t index,
                             std::size_t count,
                             float update_interval) -> float {
  if ((count <= 1U) || (update_interval <= 0.0F)) {
    return 0.0F;
  }

  return update_interval * static_cast<float>(index) / static_cast<float>(count);
}

void apply_nation_default_strategy(AI::AIContext& context,
                                   const Game::Systems::NationRegistry& nations) {
  const auto* nation = nations.get_nation_for_player(context.player_id);
  if (nation == nullptr) {
    return;
  }

  context.nation = nation;
  if (nation->ai_profile.empty()) {
    return;
  }

  context.strategy_config =
      AI::AIStrategyFactory::create_config(AI::AIStrategyFactory::parse_strategy(
          QString::fromStdString(nation->ai_profile)));
}

} // namespace

AISystem::AISystem(Services services)
    : m_services(services) {

  AI::ensure_ai_doctrine_catalog_loaded();

  m_building_attacked_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>(
          [this](const Engine::Core::BuildingAttackedEvent& event) {
            this->on_building_attacked(event);
          });

  initialize_ai_players();
}

void AISystem::populate_behavior_registry(AI::AIBehaviorRegistry& registry) {
  registry.register_behavior(std::make_unique<AI::RetreatBehavior>());
  registry.register_behavior(std::make_unique<AI::DefendBehavior>());
  registry.register_behavior(std::make_unique<AI::AssaultBehavior>());
  registry.register_behavior(std::make_unique<AI::ProductionBehavior>());
  registry.register_behavior(std::make_unique<AI::BuilderBehavior>());
  registry.register_behavior(std::make_unique<AI::EconomyBehavior>());
  registry.register_behavior(std::make_unique<AI::SquadDisciplineBehavior>());
  registry.register_behavior(std::make_unique<AI::CommanderBehavior>());
  registry.register_behavior(std::make_unique<AI::ExpandBehavior>());
  registry.register_behavior(std::make_unique<AI::HarassBehavior>());
  registry.register_behavior(std::make_unique<AI::AttackBehavior>());
  registry.register_behavior(std::make_unique<AI::LocalEngagementBehavior>());
  registry.register_behavior(std::make_unique<AI::GatherBehavior>());
}

void AISystem::reinitialize() {
  shutdown_workers();

  m_completed_decision_count = 0;
  m_applied_command_count = 0;
  m_deferred_decision_count = 0;
  m_longest_decision_wait_us = 0;

  initialize_ai_players();
}

void AISystem::shutdown_workers() {
  for (auto& ai : m_ai_instances) {
    if (ai.worker) {
      ai.worker->stop();
    }
    ai.context.nation = nullptr;
  }
  m_ai_instances.clear();
  m_worker_pool.reset();
}

auto AISystem::decision_thread_count() const -> std::size_t {
  return (m_worker_pool != nullptr) ? m_worker_pool->thread_count() : 0U;
}

void AISystem::initialize_ai_players() {
  auto& registry = m_services.owners;
  const auto& ai_owner_ids = registry.get_ai_owner_ids();

  if (ai_owner_ids.empty()) {
    return;
  }

  m_worker_pool = std::make_unique<AI::AIWorkerPool>(
      AI::AIWorkerPool::default_thread_count(ai_owner_ids.size()));

  for (std::size_t index = 0; index < ai_owner_ids.size(); ++index) {
    uint32_t const player_id = ai_owner_ids[index];
    AIInstance instance;
    instance.context.player_id = player_id;
    instance.context.state = AI::AIState::Idle;
    instance.behavior_registry = std::make_unique<AI::AIBehaviorRegistry>();
    populate_behavior_registry(*instance.behavior_registry);
    instance.worker =
        std::make_unique<AI::AIWorker>(*m_worker_pool, *instance.behavior_registry);
    instance.update_timer =
        initial_ai_update_timer(index, ai_owner_ids.size(), m_update_interval);
    apply_nation_default_strategy(instance.context, m_services.nations);

    m_ai_instances.push_back(std::move(instance));
  }
}

AISystem::~AISystem() = default;

void AISystem::set_ai_profile(int player_id, const AI::AIPlayerProfile& profile) {
  for (auto& ai : m_ai_instances) {
    if (ai.context.player_id == player_id) {
      ai.context.strategy_config = AI::AIStrategyFactory::create_config(profile);
      break;
    }
  }
}

auto AISystem::ai_player_state(int player_id) const -> AIPlayerState {
  for (const auto& ai : m_ai_instances) {
    if (ai.context.player_id != player_id) {
      continue;
    }
    const auto& config = ai.context.strategy_config;
    return {.valid = true,
            .strategy = config.strategy,
            .posture = config.posture,
            .state = ai.context.state,
            .aggression_modifier = config.aggression_modifier,
            .defense_modifier = config.defense_modifier,
            .proactive_attack_size = config.proactive_attack_size,
            .reactive_attack_size = config.reactive_attack_size};
  }
  return {};
}

void AISystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_total_game_time += delta_time;
  ++m_update_count;

  trace_progress(*world);

  m_command_filter.update(m_total_game_time);

  process_results(*world);

  for (auto& ai : m_ai_instances) {

    ai.update_timer += delta_time;
    const float effective_update_interval =
        m_update_interval *
        std::max(0.25F,
                 ai.context.strategy_config.difficulty.update_interval_multiplier);

    if (ai.update_timer < effective_update_interval) {
      continue;
    }

    if (ai.job_pending) {
      continue;
    }

    AI::AISnapshot snapshot =
        Game::Systems::AI::AISnapshotBuilder::build(*world, ai.context.player_id);
    snapshot.game_time = m_total_game_time;

    AI::AIJob job;
    job.snapshot = std::move(snapshot);
    job.context = ai.context;
    job.context.nation = nullptr;
    job.delta_time = ai.update_timer;
    merge_building_attacks(ai, job.context);

    if (ai.worker->try_submit(std::move(job))) {
      ai.update_timer = 0.0F;
      ai.job_pending = true;
      ai.job_due_update = m_update_count + k_decision_latency_updates;
    }
  }
}

void AISystem::process_results(Engine::Core::World& world) {

  for (auto& ai : m_ai_instances) {
    if (!ai.job_pending || m_update_count < ai.job_due_update) {
      continue;
    }

    const auto wait_started = std::chrono::steady_clock::now();
    const bool finished = ai.worker->wait_idle(m_decision_wait_budget);
    const auto waited = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - wait_started);
    m_longest_decision_wait_us = std::max(m_longest_decision_wait_us,
                                          static_cast<std::uint64_t>(waited.count()));

    if (!finished) {
      ++m_deferred_decision_count;
      ai.job_due_update = m_update_count + 1;
      continue;
    }

    ai.job_pending = false;

    std::queue<AI::AIResult> results;
    ai.worker->drain_results(results);

    while (!results.empty()) {
      auto& result = results.front();

      ai.context = result.context;
      merge_building_attacks(ai, ai.context);
      ai.unmerged_building_attacks.clear();

      auto filtered_commands =
          m_command_filter.filter(result.commands, m_total_game_time);

      ++m_completed_decision_count;
      m_applied_command_count += filtered_commands.size();

      const auto report = Game::Systems::AI::AICommandApplier::apply(
          world, ai.context.player_id, filtered_commands);
      m_refused_command_count += report.refused_production;

      results.pop();
    }
  }
}

void AISystem::trace_progress(const Engine::Core::World& world) {
  static const bool enabled = !qEnvironmentVariableIsEmpty("SOI_AI_TRACE");
  if (!enabled) {
    return;
  }
  constexpr float k_trace_interval_seconds = 10.0F;
  if (m_total_game_time < m_next_trace_time) {
    return;
  }
  m_next_trace_time = m_total_game_time + k_trace_interval_seconds;

  for (const auto& ai : m_ai_instances) {
    const auto& context = ai.context;
    const auto resources =
        Game::Session::session_for(world).economy().get_all(context.player_id);
    qInfo().nospace() << "SOI_AI_TRACE t=" << m_total_game_time
                      << " player=" << context.player_id
                      << " state=" << static_cast<int>(context.state) << " strategy="
                      << static_cast<int>(context.strategy_config.strategy)
                      << " posture="
                      << static_cast<int>(context.strategy_config.posture)
                      << " aggr=" << context.strategy_config.aggression_modifier
                      << " proactive=" << context.strategy_config.proactive_attack_size
                      << " units=" << context.total_units
                      << " melee=" << context.melee_count
                      << " ranged=" << context.ranged_count
                      << " builders=" << context.builder_count
                      << " buildings=" << context.buildings.size()
                      << " primary_barracks=" << context.primary_barracks
                      << " nation=" << (context.nation != nullptr)

                      << " manpower=" << context.recruitment_manpower_available
                      << " civilians_left=" << context.home_civilians_remaining
                      << " gold=" << resources.get(ResourceType::Gold)
                      << " food=" << resources.get(ResourceType::Food)
                      << " wood=" << resources.get(ResourceType::Wood)
                      << " stone=" << resources.get(ResourceType::Stone)
                      << " iron=" << resources.get(ResourceType::Iron)
                      << " decisions=" << m_completed_decision_count
                      << " commands=" << m_applied_command_count
                      << " refused=" << m_refused_command_count;
  }
}

auto AISystem::plan_for(int player_id) const -> const AI::AIContext* {
  for (const auto& ai : m_ai_instances) {
    if (ai.context.player_id == player_id) {
      return &ai.context;
    }
  }
  return nullptr;
}

void AISystem::merge_building_attacks(const AIInstance& ai, AI::AIContext& context) {
  for (const auto& [building_id, time] : ai.unmerged_building_attacks) {
    auto& recorded = context.buildings_under_attack[building_id];
    recorded = std::max(recorded, time);
    context.last_local_threat_time = std::max(context.last_local_threat_time, time);
    if (building_id == context.primary_barracks) {
      context.barracks_under_threat = true;
    }
  }
}

void AISystem::on_building_attacked(const Engine::Core::BuildingAttackedEvent& event) {
  for (auto& ai : m_ai_instances) {
    if (ai.context.player_id == event.owner_id) {
      ai.unmerged_building_attacks[event.building_id] = m_total_game_time;
      break;
    }
  }
}

} // namespace Game::Systems
