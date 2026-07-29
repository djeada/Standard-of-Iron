#include "session_context.h"

#include <mutex>
#include <unordered_map>

#include "../command/command_queue.h"
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

namespace {

SessionContext* g_active_session = nullptr;

thread_local SessionContext* t_active_session = nullptr;

std::mutex g_world_owner_mutex;
std::unordered_map<const Engine::Core::World*, SessionContext*> g_world_owners;

auto default_session() -> SessionContext& {
  static SessionContext s_default;
  return s_default;
}

} // namespace

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
};

SessionContext::SessionContext()
    : SessionContext(Config{}) {
}

SessionContext::SessionContext(const Config& config)
    : m_state(std::make_unique<State>(config)) {
  const std::lock_guard<std::mutex> lock(g_world_owner_mutex);
  g_world_owners.emplace(&m_state->world, this);
}

SessionContext::~SessionContext() {
  {
    const std::lock_guard<std::mutex> lock(g_world_owner_mutex);
    g_world_owners.erase(&m_state->world);
  }

  if (g_active_session == this) {
    g_active_session = nullptr;
  }
  if (t_active_session == this) {
    t_active_session = nullptr;
  }
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

void SessionContext::reset() {
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
  m_state->marketplace.clear();
  m_state->clock.reset();
  m_state->rng.reseed(m_state->seed);
}

auto SessionContext::active() -> SessionContext& {
  if (t_active_session != nullptr) {
    return *t_active_session;
  }
  if (g_active_session != nullptr) {
    return *g_active_session;
  }
  return default_session();
}

auto SessionContext::for_world(const Engine::Core::World& world) -> SessionContext* {
  const std::lock_guard<std::mutex> lock(g_world_owner_mutex);
  const auto it = g_world_owners.find(&world);
  return it != g_world_owners.end() ? it->second : nullptr;
}

auto SessionContext::active_or_null() -> SessionContext* {
  return t_active_session != nullptr ? t_active_session : g_active_session;
}

auto SessionContext::set_active(SessionContext* session) -> SessionContext* {
  SessionContext* previous = g_active_session;
  g_active_session = session;
  return previous;
}

auto SessionContext::set_thread_active(SessionContext* session) -> SessionContext* {
  SessionContext* previous = t_active_session;
  t_active_session = session;
  return previous;
}

} // namespace Game::Session
