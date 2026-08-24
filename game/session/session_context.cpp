#include "session_context.h"

#include "../command/command_queue.h"
#include "../command/replay.h"
#include "../core/ambient_session.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../map/visibility_service.h"
#include "../systems/building_collision_registry.h"
#include "../systems/global_stats_registry.h"
#include "../systems/marketplace_system.h"
#include "../systems/nation_registry.h"
#include "../systems/owner_registry.h"
#include "../systems/player_resource_registry.h"
#include "../systems/troop_count_registry.h"
#include "deterministic_rng.h"
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

  Engine::Core::World world;
  Game::Map::TerrainService terrain;
  Game::Systems::OwnerRegistry owners;
  Game::Systems::PlayerResourceRegistry economy;
  Game::Systems::NationRegistry nations;
  Game::Systems::GlobalStatsRegistry stats;
  Game::Systems::TroopCountRegistry troop_counts;
  Game::Systems::BuildingCollisionRegistry building_collision;
  Game::Systems::MarketplaceSystem marketplace;
  Game::Map::VisibilityService visibility;
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
  services.terrain = &m_state->terrain;
  services.visibility = &m_state->visibility;
  services.owners = &m_state->owners;
  services.economy = &m_state->economy;
  services.nations = &m_state->nations;
  services.stats = &m_state->stats;
  services.troop_counts = &m_state->troop_counts;
  services.building_collision = &m_state->building_collision;
  services.marketplace = &m_state->marketplace;
  services.clock = &m_state->clock;
  services.rng = &m_state->rng;
  services.commands = &m_state->commands;

  bind_world_services(m_state->world, &m_state->services);
}

SessionContext::~SessionContext() {
  unbind_world_services(m_state->world);
  unbind_ambient_services(&m_state->services);
}

auto SessionContext::world() -> Engine::Core::World& {
  return m_state->world;
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

auto SessionContext::building_collision() -> Game::Systems::BuildingCollisionRegistry& {
  return m_state->building_collision;
}

auto SessionContext::marketplace() -> Game::Systems::MarketplaceSystem& {
  return m_state->marketplace;
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

auto SessionContext::commands() -> Game::Command::CommandQueue& {
  return m_state->commands;
}

void SessionContext::set_replay_player(
    std::unique_ptr<Game::Command::ReplayPlayer> player) {
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

auto SessionContext::rng_seed() const -> std::uint64_t {
  return m_state->seed;
}

auto SessionContext::advance(double real_dt,
                             int max_steps,
                             const TickFn& per_tick) -> int {
  auto& clock = m_state->clock;
  clock.advance(real_dt);
  int steps = 0;
  while (steps < max_steps && clock.consume_tick()) {
    if (per_tick) {
      per_tick(static_cast<float>(clock.tick_seconds()));
    } else {
      step();
    }
    ++steps;
  }
  clock.drop_pending_ticks();
  return steps;
}

void SessionContext::step() {
  m_state->world.update(static_cast<float>(m_state->clock.tick_seconds()));
}

void SessionContext::reset() {

  m_state->replay_recorder.reset();
  set_replay_player(nullptr);
  m_state->commands.clear();
  m_state->visibility.reset();
  m_state->world.clear();
  m_state->terrain.clear();
  m_state->owners.clear();
  m_state->economy.clear();
  m_state->nations.clear_player_assignments();
  m_state->stats.clear();
  m_state->troop_counts.clear();
  m_state->building_collision.clear();
  m_state->clock.reset();
  m_state->rng.reseed(m_state->seed);
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
