#include "ai_system.h"

#include <QDebug>
#include <QJsonArray>
#include <queue>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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
  read_appeal_answers(*world);
  follow_up_appeals(*world);
  plead_with_allies(*world);
  appeal_for_military_aid(*world);

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
                                            ai.context.barracks_under_threat,
                                            ai.goodwill[request.requester]);
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
      const auto verdict = AI::answer_ally_call(
          ai.context.strategy_config, standing, call.kind, ai.goodwill[call.requester]);
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
  if (m_total_game_time < k_first_plea_at) {
    return;
  }
  // Only pleas to a human share the global spacing; commanders asking each
  // other are paced per commander.
  const bool human_may_hear =
      m_total_game_time - m_last_any_plea_at >= k_any_plea_interval;
  auto& session = Game::Session::session_for(world);
  const auto& owners = session.owners();
  for (auto& ai : m_ai_instances) {
    const int owner = ai.context.player_id;
    if (m_total_game_time - ai.last_plea_at < k_plea_interval ||
        ai.context.nation == nullptr || !ai.context.nation->has_economy ||
        ai.context.total_units == 0) {
      continue;
    }
    // Ask whichever ally holds most of what this camp lacks, human or
    // commander alike.
    int giver = 0;
    std::optional<AI::AllyPleaNeed> need;
    int best_stock = -1;
    for (const int ally : owners.get_allies_of(owner)) {
      const bool human = owners.is_player(ally);
      if ((human && !human_may_hear) || (!human && !owners.is_ai(ally))) {
        continue;
      }
      const auto& ally_stock = session.economy().get_all(ally);
      const auto candidate =
          AI::pick_ally_plea(session.economy().get_all(owner), ally_stock);
      if (!candidate.has_value()) {
        continue;
      }
      const int stock = ally_stock.get(candidate->resource);
      if (stock > best_stock || (stock == best_stock && human)) {
        best_stock = stock;
        giver = ally;
        need = candidate;
      }
    }
    if (!need.has_value()) {
      continue;
    }
    ai.last_plea_at = m_total_game_time;
    if (!owners.is_player(giver)) {
      // Another commander answers through the same rule it uses for the
      // player's requests, and the goods move on the spot.
      session.marketplace().queue_ally_request({.requester = owner,
                                                .giver = giver,
                                                .resource = need->resource,
                                                .amount = need->amount});
      continue;
    }
    auto& board = session.alliance();
    const auto appeal_id = board.next_appeal_id();
    board.record_appeal({.appeal_id = appeal_id,
                         .from_ally = owner,
                         .to_owner = giver,
                         .kind = Game::Systems::AllyAppealKind::Resources,
                         .resource = need->resource,
                         .amount = need->amount});
    ai.appeals.push_back({.appeal_id = appeal_id,
                          .to_owner = giver,
                          .kind = Game::Systems::AllyAppealKind::Resources,
                          .resource = need->resource,
                          .asked_at = m_total_game_time});
    m_last_any_plea_at = m_total_game_time;
    return;
  }
}

namespace {

constexpr float k_defend_appeal_interval = 90.0F;
constexpr float k_attack_appeal_interval = 150.0F;
constexpr int k_defend_appeal_min_threats = 2;
constexpr int k_attack_appeal_min_wave = 4;
constexpr float k_defend_aid_window = 75.0F;
constexpr float k_attack_aid_window = 150.0F;
constexpr float k_defend_aid_radius = 20.0F;
constexpr float k_attack_aid_radius = 25.0F;
constexpr int k_aid_min_soldiers = 1;

auto human_soldiers_near(
    Engine::Core::World& world, int owner, float x, float z, float radius) -> int {
  const float radius_sq = radius * radius;
  int count = 0;
  for (auto [entity_id, unit, transform] :
       world.view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
    (void)entity_id;
    if (unit.owner_id != owner || unit.health <= 0 ||
        !Game::Units::is_troop_spawn(unit.spawn_type)) {
      continue;
    }
    const float dx = transform.position.x - x;
    const float dz = transform.position.z - z;
    if (dx * dx + dz * dz <= radius_sq) {
      ++count;
    }
  }
  return count;
}

void nudge_goodwill(std::unordered_map<int, float>& goodwill, int owner, float delta) {
  float& value = goodwill[owner];
  value = std::clamp(value + delta,
                     Game::Systems::AI::k_min_ally_goodwill,
                     Game::Systems::AI::k_max_ally_goodwill);
}

} // namespace

auto AISystem::ally_goodwill(int ai_owner, int owner) const -> float {
  for (const auto& ai : m_ai_instances) {
    if (ai.context.player_id == ai_owner) {
      const auto it = ai.goodwill.find(owner);
      return it != ai.goodwill.end() ? it->second : 0.0F;
    }
  }
  return 0.0F;
}

void AISystem::appeal_for_military_aid(Engine::Core::World& world) {
  auto& session = Game::Session::session_for(world);
  const auto& owners = session.owners();
  auto& board = session.alliance();
  for (auto& ai : m_ai_instances) {
    const int owner = ai.context.player_id;
    std::vector<int> humans;
    std::vector<int> commanders;
    for (const int ally : owners.get_allies_of(owner)) {
      if (owners.is_player(ally)) {
        humans.push_back(ally);
      } else if (owners.is_ai(ally)) {
        commanders.push_back(ally);
      }
    }
    if ((humans.empty() && commanders.empty()) || ai.context.total_units == 0) {
      continue;
    }
    const auto has_open = [&ai](Game::Systems::AllyAppealKind kind) {
      return std::any_of(ai.appeals.begin(), ai.appeals.end(), [kind](const auto& a) {
        return a.kind == kind;
      });
    };

    struct Ask {
      Game::Systems::AllyAppealKind kind;
      Engine::Core::EntityID target;
      float x;
      float z;
    };
    std::optional<Ask> ask;
    if (ai.context.barracks_under_threat &&
        ai.context.nearby_threat_count >= k_defend_appeal_min_threats &&
        m_total_game_time - ai.last_defend_appeal_at >= k_defend_appeal_interval &&
        !has_open(Game::Systems::AllyAppealKind::Defend)) {
      ai.last_defend_appeal_at = m_total_game_time;
      ask = Ask{Game::Systems::AllyAppealKind::Defend,
                Engine::Core::NULL_ENTITY,
                ai.context.base_pos_x,
                ai.context.base_pos_z};
    } else if (const auto& wave = ai.context.wave;
               wave.committed && wave.target_id != 0 &&
               wave.committed_at > ai.appealed_wave_committed_at &&
               wave.initial_size >= k_attack_appeal_min_wave &&
               m_total_game_time - ai.last_attack_appeal_at >=
                   k_attack_appeal_interval &&
               !has_open(Game::Systems::AllyAppealKind::Attack)) {
      ai.appealed_wave_committed_at = wave.committed_at;
      ai.last_attack_appeal_at = m_total_game_time;
      ask = Ask{Game::Systems::AllyAppealKind::Attack,
                wave.target_id,
                wave.target_x,
                wave.target_z};
    }
    if (!ask.has_value()) {
      continue;
    }
    // Allied commanders get the same request as a call; each answers it with
    // the rule it uses for the player's calls and acts on its pledge.
    if (!commanders.empty()) {
      const auto* target_unit =
          ask->target != Engine::Core::NULL_ENTITY
              ? world.try_get<Engine::Core::UnitComponent>(ask->target)
              : nullptr;
      const int target_owner = target_unit != nullptr ? target_unit->owner_id : owner;
      const auto call_id = board.next_call_id();
      for (const int commander : commanders) {
        board.queue_call({.call_id = call_id,
                          .requester = owner,
                          .ally = commander,
                          .kind = ask->kind == Game::Systems::AllyAppealKind::Attack
                                      ? Game::Systems::AllyCallKind::Attack
                                      : Game::Systems::AllyCallKind::Defend,
                          .target = ask->target,
                          .target_owner = target_owner,
                          .target_x = ask->x,
                          .target_z = ask->z});
      }
    }
    for (const int human : humans) {
      const auto appeal_id = board.next_appeal_id();
      board.record_appeal({.appeal_id = appeal_id,
                           .from_ally = owner,
                           .to_owner = human,
                           .kind = ask->kind,
                           .target = ask->target,
                           .target_x = ask->x,
                           .target_z = ask->z});
      ai.appeals.push_back({.appeal_id = appeal_id,
                            .to_owner = human,
                            .kind = ask->kind,
                            .target_x = ask->x,
                            .target_z = ask->z,
                            .asked_at = m_total_game_time});
    }
  }
}

void AISystem::read_appeal_answers(Engine::Core::World& world) {
  using Game::Systems::AllyAppealFollowUp;
  using Game::Systems::AllyAppealKind;
  auto& board = Game::Session::session_for(world).alliance();
  for (auto& ai : m_ai_instances) {
    const int owner = ai.context.player_id;
    for (const auto& answer : board.take_appeal_answers_for(owner)) {
      auto open =
          std::find_if(ai.appeals.begin(), ai.appeals.end(), [&](const auto& a) {
            return a.appeal_id == answer.appeal_id;
          });
      if (open == ai.appeals.end()) {
        continue;
      }
      Game::Systems::AllyAppealReply reply{.appeal_id = answer.appeal_id,
                                           .from_ally = owner,
                                           .to_owner = answer.answerer,
                                           .kind = open->kind,
                                           .resource = open->resource};
      if (open->kind == AllyAppealKind::Resources) {
        if (answer.accepted && answer.given > 0) {
          reply.follow_up = AllyAppealFollowUp::Grateful;
          reply.amount = answer.given;
          nudge_goodwill(ai.goodwill,
                         answer.answerer,
                         0.1F + static_cast<float>(answer.given) / 1000.0F);
        } else {
          reply.follow_up = AllyAppealFollowUp::ManageWithout;
          nudge_goodwill(ai.goodwill, answer.answerer, -0.1F);
        }
        ai.appeals.erase(open);
      } else if (answer.accepted) {
        reply.follow_up = AllyAppealFollowUp::AwaitingAid;
        open->accepted = true;
        open->accepted_at = m_total_game_time;
      } else {
        reply.follow_up = open->kind == AllyAppealKind::Defend
                              ? AllyAppealFollowUp::HoldAlone
                              : AllyAppealFollowUp::MarchAlone;
        nudge_goodwill(ai.goodwill,
                       answer.answerer,
                       open->kind == AllyAppealKind::Defend ? -0.1F : -0.05F);
        ai.appeals.erase(open);
      }
      board.record_appeal_reply(reply);
    }
  }
}

void AISystem::follow_up_appeals(Engine::Core::World& world) {
  using Game::Systems::AllyAppealFollowUp;
  using Game::Systems::AllyAppealKind;
  auto& board = Game::Session::session_for(world).alliance();
  for (auto& ai : m_ai_instances) {
    const int owner = ai.context.player_id;
    std::erase_if(ai.appeals, [&](const auto& appeal) {
      Game::Systems::AllyAppealReply reply{.appeal_id = appeal.appeal_id,
                                           .from_ally = owner,
                                           .to_owner = appeal.to_owner,
                                           .kind = appeal.kind};
      if (!appeal.accepted) {
        if (m_total_game_time - appeal.asked_at <
            Game::Systems::k_ally_appeal_answer_seconds) {
          return false;
        }
        board.close_appeal(appeal.appeal_id);
        reply.follow_up = AllyAppealFollowUp::Withdrawn;
        nudge_goodwill(ai.goodwill, appeal.to_owner, -0.05F);
        board.record_appeal_reply(reply);
        return true;
      }
      const bool defend = appeal.kind == AllyAppealKind::Defend;
      if (human_soldiers_near(world,
                              appeal.to_owner,
                              appeal.target_x,
                              appeal.target_z,
                              defend ? k_defend_aid_radius : k_attack_aid_radius) >=
          k_aid_min_soldiers) {
        reply.follow_up = AllyAppealFollowUp::AidArrived;
        nudge_goodwill(ai.goodwill, appeal.to_owner, 0.2F);
        board.record_appeal_reply(reply);
        return true;
      }
      if (m_total_game_time - appeal.accepted_at <
          (defend ? k_defend_aid_window : k_attack_aid_window)) {
        return false;
      }
      reply.follow_up = AllyAppealFollowUp::AidNeverCame;
      nudge_goodwill(ai.goodwill, appeal.to_owner, -0.25F);
      board.record_appeal_reply(reply);
      return true;
    });
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

      // Workers never see the nation registry, so their context comes back
      // without it. Keep ours: with it gone, every check that reads the nation
      // (resource pleas among them) went quiet after the first decision.
      const auto* nation = ai.context.nation;
      ai.context = result.context;
      ai.context.nation = nation;
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
