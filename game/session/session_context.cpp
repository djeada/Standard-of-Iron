#include "session_context.h"

#include <cstdio>

#include "../command/command_queue.h"
#include "../command/replay.h"
#include "../core/ambient_session.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../formation/army_formation_registry.h"
#include "../map/terrain_service.h"
#include "../map/visibility_service.h"
#include "../systems/alliance_board.h"
#include "../systems/building_collision_registry.h"
#include "../systems/economy/marketplace_system.h"
#include "../systems/global_stats_registry.h"
#include "../systems/nation_registry.h"
#include "../systems/navigation/navigation_service.h"
#include "../systems/owner_registry.h"
#include "../systems/player_resource_registry.h"
#include "../systems/troop_count_registry.h"
#include "../wildlife/bird_flock.h"
#include "deterministic_rng.h"
#include "selection_service.h"
#include "simulation_clock.h"

namespace Game::Session {

struct SessionContext::State {
  explicit State(const Config& config)
      : clock(config.tick_seconds)
      , rng(config.rng_seed)
      , seed(config.rng_seed) {}

  SimulationClock clock;
  DeterministicRng rng;
  std::uint64_t seed;

  Engine::Core::EventManager events;
  Engine::Core::World world;
  Game::Map::TerrainService terrain;
  Game::Systems::OwnerRegistry owners;
  Game::Systems::PlayerResourceRegistry economy;
  Game::Systems::NationRegistry nations;
  Game::Systems::GlobalStatsRegistry stats;
  Game::Systems::TroopCountRegistry troop_counts;
  SelectionService selection;
  Game::Systems::BuildingCollisionRegistry building_collision;
  Game::Systems::MarketplaceSystem marketplace;
  Game::Systems::AllianceBoard alliance;
  Game::Systems::NavigationService navigation;
  Game::Formation::ArmyFormationRegistry army_formations;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> units;
  Game::Wildlife::BirdFlockManager birds;
  Game::Map::VisibilityService visibility;
  Game::Map::SimulationVisibility simulation_visibility;
  Game::Command::CommandQueue commands;
  std::unique_ptr<Game::Command::ReplayPlayer> replay_player;
  std::unique_ptr<Game::Command::ReplayRecorder> replay_recorder;

  AmbientServices services;
};

SessionContext::SessionContext()
    : SessionContext(Config{}) {
}

SessionContext::SessionContext(const Config& config)
    : m_state(std::make_unique<State>(config)) {
  auto& services = m_state->services;
  services.session = this;
  services.world = &m_state->world;
  services.events = &m_state->events;
  services.terrain = &m_state->terrain;
  services.visibility = &m_state->visibility;
  services.simulation_visibility = &m_state->simulation_visibility;
  services.owners = &m_state->owners;
  services.economy = &m_state->economy;
  services.nations = &m_state->nations;
  services.stats = &m_state->stats;
  services.troop_counts = &m_state->troop_counts;
  services.building_collision = &m_state->building_collision;
  services.marketplace = &m_state->marketplace;
  services.navigation = &m_state->navigation;
  services.army_formations = &m_state->army_formations;
  services.units = &m_state->units;
  services.birds = &m_state->birds;
  services.clock = &m_state->clock;
  services.rng = &m_state->rng;
  services.commands = &m_state->commands;

  m_state->building_collision.bind_navigation(&m_state->navigation);
  m_state->world.add_world_cleared_observer([state = m_state.get()]() {
    state->alliance.clear();
    state->marketplace.clear_ally_exchange();
    state->simulation_visibility.clear();
  });

  bind_world_services(m_state->world, &m_state->services);
}

SessionContext::~SessionContext() {
  m_state->commands.log().finish_match();
  m_state->world.shutdown_systems();
  unbind_world_services(m_state->world);
  unbind_ambient_services(&m_state->services);
}

auto SessionContext::army_formations() -> Game::Formation::ArmyFormationRegistry& {
  return m_state->army_formations;
}

auto SessionContext::birds() const -> const Game::Wildlife::BirdFlockManager& {
  return m_state->birds;
}

auto SessionContext::units() const
    -> const std::shared_ptr<Game::Units::UnitFactoryRegistry>& {
  return m_state->units;
}

void SessionContext::set_units(
    std::shared_ptr<Game::Units::UnitFactoryRegistry> units) {
  m_state->units = std::move(units);
}

auto SessionContext::navigation() -> Game::Systems::NavigationService& {
  return m_state->navigation;
}

auto SessionContext::navigation() const -> const Game::Systems::NavigationService& {
  return m_state->navigation;
}

auto SessionContext::world() -> Engine::Core::World& {
  return m_state->world;
}

auto SessionContext::events() -> Engine::Core::EventManager& {
  return m_state->events;
}

auto SessionContext::world() const -> const Engine::Core::World& {
  return m_state->world;
}

auto SessionContext::terrain() -> Game::Map::TerrainService& {
  return m_state->terrain;
}

auto SessionContext::terrain() const -> const Game::Map::TerrainService& {
  return m_state->terrain;
}

auto SessionContext::visibility() -> Game::Map::VisibilityService& {
  return m_state->visibility;
}

auto SessionContext::visibility() const -> const Game::Map::VisibilityService& {
  return m_state->visibility;
}

auto SessionContext::simulation_visibility() -> Game::Map::SimulationVisibility& {
  return m_state->simulation_visibility;
}

auto SessionContext::owners() -> Game::Systems::OwnerRegistry& {
  return m_state->owners;
}

auto SessionContext::owners() const -> const Game::Systems::OwnerRegistry& {
  return m_state->owners;
}

auto SessionContext::economy() -> Game::Systems::PlayerResourceRegistry& {
  return m_state->economy;
}

auto SessionContext::economy() const -> const Game::Systems::PlayerResourceRegistry& {
  return m_state->economy;
}

auto SessionContext::nations() -> Game::Systems::NationRegistry& {
  return m_state->nations;
}

auto SessionContext::nations() const -> const Game::Systems::NationRegistry& {
  return m_state->nations;
}

auto SessionContext::stats() -> Game::Systems::GlobalStatsRegistry& {
  return m_state->stats;
}

auto SessionContext::troop_counts() -> Game::Systems::TroopCountRegistry& {
  return m_state->troop_counts;
}

auto SessionContext::selection() -> SelectionService& {
  return m_state->selection;
}

auto SessionContext::selection() const -> const SelectionService& {
  return m_state->selection;
}

auto SessionContext::building_collision() -> Game::Systems::BuildingCollisionRegistry& {
  return m_state->building_collision;
}

auto SessionContext::marketplace() -> Game::Systems::MarketplaceSystem& {
  return m_state->marketplace;
}

auto SessionContext::alliance() -> Game::Systems::AllianceBoard& {
  return m_state->alliance;
}

auto SessionContext::clock() -> SimulationClock& {
  return m_state->clock;
}

auto SessionContext::clock() const -> const SimulationClock& {
  return m_state->clock;
}

auto SessionContext::rng() -> DeterministicRng& {
  return m_state->rng;
}

auto SessionContext::human_owners() const -> std::vector<int> {
  return m_state->owners.get_player_owner_ids();
}

auto SessionContext::local_owner() const -> std::optional<int> {
  const int owner = m_state->owners.get_local_player_id();
  if (log_follower() || !m_state->owners.is_player(owner)) {
    return std::nullopt;
  }
  return owner;
}

auto SessionContext::commands() -> Game::Command::CommandQueue& {
  return m_state->commands;
}

void SessionContext::set_replay_player(
    std::unique_ptr<Game::Command::ReplayPlayer> player) {
  if (player != nullptr || m_state->replay_player != nullptr) {
    m_state->commands.clear(m_state->clock.tick());
  }
  m_state->replay_player = std::move(player);
  m_state->commands.set_replay_only(m_state->replay_player != nullptr);
}

auto SessionContext::replay_player() -> Game::Command::ReplayPlayer* {
  return m_state->replay_player.get();
}

void SessionContext::set_replay_recorder(
    std::unique_ptr<Game::Command::ReplayRecorder> recorder) {
  m_state->replay_recorder = std::move(recorder);
}

auto SessionContext::replay_recorder() -> Game::Command::ReplayRecorder* {
  return m_state->replay_recorder.get();
}

void SessionContext::submit_clock_change(Game::Command::ClockChange change) {
  m_state->commands.log().append(
      m_state->clock.tick() + 1, Game::Command::Source::LocalPlayer, 0, change);
}

auto SessionContext::log_follower() const -> bool {
  return m_state->commands.replay_only();
}

void SessionContext::prepare_tick() {
  const auto tick = m_state->clock.tick() + 1;
  if (auto* player = replay_player()) {
    player->feed(tick, m_state->commands);
  }
  const auto& log = m_state->commands.log();
  if (log.follower() && tick > log.available_tick()) {
    return;
  }
  for (const auto& entry : log.entries_at(tick)) {
    if (const auto* change = std::get_if<Game::Command::ClockChange>(&entry->payload)) {
      if (change->paused) {
        m_state->clock.set_paused(*change->paused);
      }
      if (change->speed) {
        m_state->clock.set_time_scale(*change->speed);
      }
    }
  }
}

void SessionContext::run_tick(const TickFn& per_tick) {
  auto& clock = m_state->clock;
  auto& log = m_state->commands.log();
  log.flush();
  if (const auto* map = m_state->terrain.get_height_map()) {
    m_state->simulation_visibility.advance(
        m_state->world, m_state->owners, *map, clock.tick(), clock.tick_seconds());
  }
  if (per_tick) {
    per_tick(static_cast<float>(clock.tick_seconds()));
  } else {
    step();
  }
  const auto tick = clock.tick();
  auto* recorder = replay_recorder();
  auto* player = replay_player();
  const bool record_digest = recorder != nullptr && recorder->digest_due(tick);
  const bool check_digest = player != nullptr && player->digest_due(tick);
  if (record_digest || check_digest) {
    const auto digests = subsystem_digests(*this);
    if (record_digest) {
      log.append(tick, Game::Command::Source::Script, 0, digests);
    }
    if (check_digest && !player->check(tick, digests)) {
      const auto& divergence = *player->divergence();
      if (divergence.tick == tick) {
        std::fprintf(stderr,
                     "replay: digest diverged at tick %llu in %s (recorded %llu, "
                     "observed %llu)\n",
                     static_cast<unsigned long long>(tick),
                     divergence.subsystem,
                     static_cast<unsigned long long>(divergence.recorded),
                     static_cast<unsigned long long>(divergence.observed));
      }
    }
  }
  if (recorder != nullptr) {
    if (auto checkpoint = recorder->checkpoint_at(tick)) {
      log.append(tick, Game::Command::Source::Script, 0, std::move(*checkpoint));
    }
  }
  log.complete_tick(tick);
}

auto SessionContext::rng_seed() const -> std::uint64_t {
  return m_state->seed;
}

auto SessionContext::advance(double real_dt,
                             int max_steps,
                             OverloadPolicy overload,
                             const TickFn& per_tick,
                             double pacing) -> int {
  auto& clock = m_state->clock;
  const auto& log = m_state->commands.log();
  const bool following = log.follower();
  prepare_tick();
  if (pacing <= 0.0) {
    return 0;
  }
  clock.advance(real_dt, following, pacing);
  int steps = 0;
  while (steps < max_steps && !clock.paused() &&
         (!following || clock.tick() < log.available_tick()) && clock.consume_tick()) {
    run_tick(per_tick);
    ++steps;
    prepare_tick();
  }
  if (!following && overload == OverloadPolicy::DiscardBacklog) {
    clock.drop_pending_ticks();
  }
  return steps;
}

void SessionContext::service_stopped_clock() {
  auto& world = m_state->world;
  const bool recorded = m_state->replay_recorder != nullptr ||
                        replay_player() != nullptr || m_state->commands.replay_only();
  world.service_stopped_frame([&] {
    if (!recorded) {
      m_state->commands.drain(world, m_state->clock.tick());
    }
  });
}

auto SessionContext::advance_from_log(int max_steps, const TickFn& per_tick) -> int {
  const auto& log = m_state->commands.log();
  if (!log.follower()) {
    return 0;
  }
  auto& clock = m_state->clock;
  int steps = 0;
  while (steps < max_steps) {
    prepare_tick();
    clock.queue_through(log.available_tick(), max_steps - steps);
    if (clock.paused() || !clock.consume_tick()) {
      break;
    }
    run_tick(per_tick);
    ++steps;
  }
  return steps;
}

void SessionContext::step() {
  m_state->world.update(static_cast<float>(m_state->clock.tick_seconds()));
}

void SessionContext::reset() {
  m_state->commands.log().finish_match();
  m_state->replay_recorder.reset();
  set_replay_player(nullptr);
  m_state->commands.clear();
  m_state->visibility.shutdown();
  m_state->world.clear();
  m_state->terrain.clear();
  m_state->owners.clear();
  m_state->economy.clear();
  m_state->nations.clear_player_assignments();
  m_state->stats.clear();
  m_state->troop_counts.clear();
  m_state->selection.clear_selection();
  m_state->selection.clear_control_groups();
  m_state->building_collision.clear();
  m_state->navigation.clear();
  m_state->army_formations.clear();
  m_state->clock.reset();
  m_state->rng.reseed(m_state->seed);
  m_state->economy.set_idle_builders_gather(false);
}

auto SessionContext::set_idle_builders_gather(bool enabled) -> void {
  m_state->economy.set_idle_builders_gather(enabled);
}

auto SessionContext::idle_builders_gather() const -> bool {
  return m_state->economy.idle_builders_gather();
}

auto SessionContext::active() -> SessionContext& {
  return *ambient_services().session;
}

auto SessionContext::for_world(const Engine::Core::World& world) -> SessionContext* {
  const auto* services = services_for_or_null(world);
  return services != nullptr ? services->session : nullptr;
}

auto session_for(const Engine::Core::World& world) -> SessionContext& {
  if (auto* session = SessionContext::for_world(world)) {
    return *session;
  }
  if (auto* fallback = services_for(world).session) {
    return *fallback;
  }
  return SessionContext::active();
}

auto SessionContext::active_or_null() -> SessionContext* {
  const auto* bound = ambient_services_or_null();
  return bound != nullptr ? bound->session : nullptr;
}

auto SessionContext::set_active(SessionContext* session) -> SessionContext* {
  const auto* previous =
      set_ambient_services(session != nullptr ? &session->m_state->services : nullptr);
  return previous != nullptr ? previous->session : nullptr;
}

auto SessionContext::set_thread_active(SessionContext* session) -> SessionContext* {
  const auto* previous = set_thread_ambient_services(
      session != nullptr ? &session->m_state->services : nullptr);
  return previous != nullptr ? previous->session : nullptr;
}

} // namespace Game::Session
