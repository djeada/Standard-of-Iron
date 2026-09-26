#include "ai_system.h"

#include <QDebug>
#include <QJsonArray>
#include <queue>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

#include "../core/ambient_session.h"
#include "../core/component_core.h"
#include "../core/world.h"
#include "../session/session_context.h"
#include "../units/spawn_type.h"
#include "ai_system/ai_command_applier.h"
#include "ai_system/ai_doctrine_catalog.h"
#include "ai_system/ai_snapshot_builder.h"
#include "ai_system/ai_strategy.h"
#include "ai_system/ai_tribute.h"
#include "ai_system/behaviors/ally_aid_behavior.h"
#include "ai_system/behaviors/assault_behavior.h"
#include "ai_system/behaviors/attack_behavior.h"
#include "ai_system/behaviors/builder_behavior.h"
#include "ai_system/behaviors/commander_behavior.h"
#include "ai_system/behaviors/defend_behavior.h"
#include "ai_system/behaviors/economy_behavior.h"
#include "ai_system/behaviors/expand_behavior.h"
#include "ai_system/behaviors/gather_behavior.h"
#include "ai_system/behaviors/gold_vein_behavior.h"
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
  registry.register_behavior(std::make_unique<AI::AllyAidBehavior>());
  registry.register_behavior(std::make_unique<AI::AssaultBehavior>());
  registry.register_behavior(std::make_unique<AI::ProductionBehavior>());
  registry.register_behavior(std::make_unique<AI::BuilderBehavior>());
  registry.register_behavior(std::make_unique<AI::EconomyBehavior>());
  registry.register_behavior(std::make_unique<AI::SquadDisciplineBehavior>());
  registry.register_behavior(std::make_unique<AI::CommanderBehavior>());
  registry.register_behavior(std::make_unique<AI::ExpandBehavior>());
  registry.register_behavior(std::make_unique<AI::GoldVeinBehavior>());
  registry.register_behavior(std::make_unique<AI::HarassBehavior>());
  registry.register_behavior(std::make_unique<AI::AttackBehavior>());
  registry.register_behavior(std::make_unique<AI::LocalEngagementBehavior>());
  registry.register_behavior(std::make_unique<AI::GatherBehavior>());
}

void AISystem::reinitialize() {
  shutdown_workers();

  m_completed_decision_count = 0;
  m_applied_command_count = 0;
  m_decisions_over_wait_budget = 0;
  m_longest_decision_wait_us = 0;
  m_snapshot_build_count = 0;
  m_initial_decisions_prepared = false;
  m_initial_decisions_ready.store(false, std::memory_order_release);

  initialize_ai_players();
}

auto AISystem::submit_decision_job(AIInstance& ai,
                                   Engine::Core::World& world,
                                   float delta_time) -> bool {
  AI::AISnapshot snapshot = Game::Systems::AI::AISnapshotBuilder::build(
      world, ai.context.player_id, &ai.known_objectives);
  snapshot.game_time = m_total_game_time;
  snapshot.pledges = ai.pledges;
  ++m_snapshot_build_count;

  AI::AIJob job;
  job.snapshot = std::move(snapshot);
  job.context = ai.context;
  job.context.nation = nullptr;
  job.delta_time = delta_time;
  const auto* bound = Game::Session::services_for_or_null(world);
  job.session = bound != nullptr ? bound->session : nullptr;
  merge_building_attacks(ai, job.context);

  if (!ai.worker->try_submit(std::move(job))) {
    return false;
  }

  ai.job_pending = true;
  ai.job_due_update = m_update_count + k_decision_latency_updates;
  return true;
}

void AISystem::prepare_initial_decisions(Engine::Core::World& world) {
  m_initial_decisions_prepared = true;
  if (m_ai_instances.empty()) {
    m_initial_decisions_ready.store(true, std::memory_order_release);
    return;
  }

  const std::size_t count = m_ai_instances.size();
  for (std::size_t index = 0; index < count; ++index) {
    auto& ai = m_ai_instances[index];
    if (ai.job_pending) {
      continue;
    }
    const float stagger = initial_ai_update_timer(index, count, m_update_interval);
    if (submit_decision_job(ai, world, m_update_interval)) {
      ai.update_timer = -stagger;
    }
  }
}

auto AISystem::await_initial_decisions(std::chrono::milliseconds budget) -> bool {
  if (!m_initial_decisions_prepared) {
    return false;
  }

  const auto deadline = std::chrono::steady_clock::now() + budget;
  bool all_idle = true;
  for (auto& ai : m_ai_instances) {
    if (!ai.job_pending) {
      continue;
    }
    const auto now = std::chrono::steady_clock::now();
    const auto remaining =
        now >= deadline
            ? std::chrono::microseconds{0}
            : std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
    if (!ai.worker->wait_idle(remaining)) {
      all_idle = false;
    }
  }

  m_initial_decisions_ready.store(all_idle, std::memory_order_release);
  return all_idle;
}

auto AISystem::initial_decisions_ready() const -> bool {
  return m_initial_decisions_ready.load(std::memory_order_acquire);
}

void AISystem::wait_for_decisions() {
  for (auto& ai : m_ai_instances) {
    if (ai.worker) {
      ai.worker->wait_idle();
    }
  }
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
            .reactive_attack_size = config.reactive_attack_size,
            .difficulty_level = config.difficulty.level,
            .update_interval_multiplier = config.difficulty.update_interval_multiplier,
            .production_rate_multiplier = config.difficulty.production_rate_multiplier,
            .scouting_distance_multiplier =
                config.difficulty.scouting_distance_multiplier};
  }
  return {};
}

auto AISystem::serialize_state() const -> QJsonObject {
  QJsonObject state;
  state["update_count"] = static_cast<double>(m_update_count);
  state["total_game_time"] = static_cast<double>(m_total_game_time);
  QJsonArray players;
  for (const auto& ai : m_ai_instances) {
    QJsonObject entry;
    entry["player_id"] = ai.context.player_id;
    entry["update_timer"] = static_cast<double>(ai.update_timer);
    entry["state"] = static_cast<int>(ai.context.state);

    const auto& config = ai.context.strategy_config;
    QJsonObject profile;
    profile["strategy"] = AI::AIStrategyFactory::strategy_to_string(config.strategy);
    profile["posture"] = AI::AIStrategyFactory::posture_to_string(config.posture);
    profile["aggression"] = static_cast<double>(config.personality.aggression);
    profile["defense"] = static_cast<double>(config.personality.defense);
    profile["harassment"] = static_cast<double>(config.personality.harassment);
    profile["difficulty"] = config.difficulty.level;
    if (config.doctrine != nullptr) {
      profile["doctrine"] = QString::fromStdString(config.doctrine->id);
    }
    entry["profile"] = profile;

    players.append(entry);
  }
  state["players"] = players;
  return state;
}

void AISystem::restore_state(const QJsonObject& state) {
  if (state.isEmpty()) {
    return;
  }
  m_update_count =
      static_cast<std::uint64_t>(state.value("update_count").toDouble(0.0));
  m_total_game_time = static_cast<float>(state.value("total_game_time").toDouble(0.0));
  for (const auto value : state.value("players").toArray()) {
    const auto entry = value.toObject();
    const int player_id = entry.value("player_id").toInt(-1);
    for (auto& ai : m_ai_instances) {
      if (ai.context.player_id != player_id) {
        continue;
      }
      ai.update_timer = static_cast<float>(entry.value("update_timer").toDouble(0.0));
      ai.context.state = static_cast<AI::AIState>(
          entry.value("state").toInt(static_cast<int>(AI::AIState::Idle)));

      if (entry.contains("profile")) {
        const auto saved = entry.value("profile").toObject();
        AI::AIPlayerProfile profile;
        profile.strategy =
            AI::AIStrategyFactory::parse_strategy(saved.value("strategy").toString());
        profile.posture = AI::AIStrategyFactory::parse_posture(
            saved.value("posture").toString(), AI::AIPosture::Field);
        profile.personality.aggression =
            static_cast<float>(saved.value("aggression").toDouble(0.5));
        profile.personality.defense =
            static_cast<float>(saved.value("defense").toDouble(0.5));
        profile.personality.harassment =
            static_cast<float>(saved.value("harassment").toDouble(0.5));
        profile.difficulty = saved.value("difficulty").toString();
        const QString doctrine_id = saved.value("doctrine").toString();
        if (!doctrine_id.isEmpty()) {
          profile.doctrine = AI::authored_doctrine(doctrine_id.toStdString());
        }
        ai.context.strategy_config = AI::AIStrategyFactory::create_config(profile);
      }
      break;
    }
  }
}

void AISystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_total_game_time += delta_time;
  ++m_update_count;

  m_command_filter.update(m_total_game_time);

  process_results(*world);
  answer_ally_requests(*world);
  answer_ally_calls(*world);
  plead_with_allies(*world);

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

    if (submit_decision_job(ai, *world, ai.update_timer)) {
      ai.update_timer = 0.0F;
    }
  }
}

void AISystem::answer_ally_requests(Engine::Core::World& world) {
  auto& session = Game::Session::session_for(world);
  auto& marketplace = session.marketplace();
  for (auto& ai : m_ai_instances) {
    const int giver = ai.context.player_id;
    for (const auto& request : marketplace.take_ally_requests_for(giver)) {
      auto answer = AI::answer_ally_request(ai.context.strategy_config,
                                            session.economy().get_all(giver),
                                            request,
                                            ai.context.barracks_under_threat);
      if (answer.granted > 0) {
        answer.granted = marketplace.send_to_ally(
            world, giver, request.requester, request.resource, answer.granted);
      }
      marketplace.record_ally_answer(answer);
    }
  }
}

void AISystem::answer_ally_calls(Engine::Core::World& world) {
  auto& board = Game::Session::session_for(world).alliance();
  for (auto& ai : m_ai_instances) {
    std::erase_if(ai.pledges, [this](const AI::AllyPledge& pledge) {
      return pledge.expires_at < m_total_game_time;
    });
    for (const auto& call : board.take_calls_for(ai.context.player_id)) {
      const int garrison = static_cast<int>(ai.context.garrison_unit_ids.size());
      const int in_wave = static_cast<int>(ai.context.wave.members.size());
      const AI::AllyCallStanding standing{
          .under_threat = ai.context.barracks_under_threat,
          .spare_units = call.kind == AllyCallKind::Attack
                             ? ai.context.combat_units - garrison
                             : ai.context.combat_units - garrison - in_wave};
      const auto verdict =
          AI::answer_ally_call(ai.context.strategy_config, standing, call.kind);
      if (verdict == AllyCallVerdict::Accepted) {
        std::erase_if(ai.pledges, [&call](const AI::AllyPledge& pledge) {
          return pledge.kind == call.kind;
        });
        ai.pledges.push_back({.kind = call.kind,
                              .requester = call.requester,
                              .target = call.target,
                              .pos_x = call.target_x,
                              .pos_z = call.target_z,
                              .expires_at = m_total_game_time + k_ally_pledge_seconds});
        if (call.kind == AllyCallKind::Attack) {
          remember_called_target(world, ai, call);
        }
      }
      board.record_call_answer({.call_id = call.call_id,
                                .requester = call.requester,
                                .ally = call.ally,
                                .kind = call.kind,
                                .target = call.target,
                                .verdict = verdict});
    }
  }
}

void AISystem::remember_called_target(Engine::Core::World& world,
                                      AIInstance& ai,
                                      const AllyCallRequest& call) {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(call.target);
  if (unit == nullptr || unit->health <= 0) {
    return;
  }
  AI::ContactSnapshot contact;
  contact.id = call.target;
  contact.owner_id = unit->owner_id;
  contact.is_building = Game::Units::is_building_spawn(unit->spawn_type);
  contact.pos_x = call.target_x;
  contact.pos_z = call.target_z;
  contact.health = unit->health;
  contact.max_health = unit->max_health;
  contact.spawn_type = unit->spawn_type;
  ai.known_objectives[call.target] = contact;
}

void AISystem::plead_with_allies(Engine::Core::World& world) {
  constexpr float k_first_plea_at = 240.0F;
  constexpr float k_plea_interval = 300.0F;
  constexpr float k_any_plea_interval = 120.0F;
  if (m_total_game_time < k_first_plea_at ||
      m_total_game_time - m_last_any_plea_at < k_any_plea_interval) {
    return;
  }
  auto& session = Game::Session::session_for(world);
  const auto& owners = session.owners();
  for (auto& ai : m_ai_instances) {
    const int owner = ai.context.player_id;
    if (m_total_game_time - ai.last_plea_at < k_plea_interval ||
        ai.context.nation == nullptr || !ai.context.nation->has_economy ||
        ai.context.total_units == 0) {
      continue;
    }
    for (const int ally : owners.get_allies_of(owner)) {
      if (!owners.is_player(ally)) {
        continue;
      }
      const auto need = AI::pick_ally_plea(session.economy().get_all(owner),
                                           session.economy().get_all(ally));
      if (!need.has_value()) {
        continue;
      }
      session.alliance().record_plea({.from_ally = owner,
                                      .to_owner = ally,
                                      .resource = need->resource,
                                      .amount = need->amount});
      ai.last_plea_at = m_total_game_time;
      m_last_any_plea_at = m_total_game_time;
      return;
    }
  }
}

void AISystem::process_results(Engine::Core::World& world) {

  for (auto& ai : m_ai_instances) {
    if (!ai.job_pending || m_update_count < ai.job_due_update) {
      continue;
    }

    const auto wait_started = std::chrono::steady_clock::now();
    ai.worker->wait_idle();
    const auto waited = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - wait_started);
    m_longest_decision_wait_us = std::max(m_longest_decision_wait_us,
                                          static_cast<std::uint64_t>(waited.count()));
    if (waited > m_decision_wait_budget) {
      ++m_decisions_over_wait_budget;
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
