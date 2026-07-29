#pragma once

#include <cstdint>
#include <memory>

namespace Engine::Core {
class World;
}

namespace Game::Map {
class TerrainService;
class VisibilityService;
} // namespace Game::Map

namespace Game::Systems {
class BuildingCollisionRegistry;
class GlobalStatsRegistry;
class MarketplaceSystem;
class NationRegistry;
class OwnerRegistry;
class PlayerResourceRegistry;
class TroopCountRegistry;
} // namespace Game::Systems

namespace Game::Command {
class CommandQueue;
}

namespace Game::Session {

class DeterministicRng;
class SimulationClock;

class SessionContext {
public:
  struct Config {
    std::uint64_t rng_seed = 0x5EED5EED5EED5EEDULL;
    double tick_seconds = 1.0 / 60.0;
  };

  SessionContext();
  explicit SessionContext(const Config& config);
  ~SessionContext();

  SessionContext(const SessionContext&) = delete;
  SessionContext(SessionContext&&) = delete;
  auto operator=(const SessionContext&) -> SessionContext& = delete;
  auto operator=(SessionContext&&) -> SessionContext& = delete;

  [[nodiscard]] auto world() -> Engine::Core::World&;
  [[nodiscard]] auto world() const -> const Engine::Core::World&;

  [[nodiscard]] auto terrain() -> Game::Map::TerrainService&;
  [[nodiscard]] auto terrain() const -> const Game::Map::TerrainService&;

  [[nodiscard]] auto visibility() -> Game::Map::VisibilityService&;

  [[nodiscard]] auto owners() -> Game::Systems::OwnerRegistry&;
  [[nodiscard]] auto owners() const -> const Game::Systems::OwnerRegistry&;

  [[nodiscard]] auto economy() -> Game::Systems::PlayerResourceRegistry&;
  [[nodiscard]] auto economy() const -> const Game::Systems::PlayerResourceRegistry&;

  [[nodiscard]] auto nations() -> Game::Systems::NationRegistry&;
  [[nodiscard]] auto nations() const -> const Game::Systems::NationRegistry&;

  [[nodiscard]] auto stats() -> Game::Systems::GlobalStatsRegistry&;

  [[nodiscard]] auto troop_counts() -> Game::Systems::TroopCountRegistry&;

  [[nodiscard]] auto building_collision() -> Game::Systems::BuildingCollisionRegistry&;

  [[nodiscard]] auto marketplace() -> Game::Systems::MarketplaceSystem&;

  [[nodiscard]] auto clock() -> SimulationClock&;
  [[nodiscard]] auto clock() const -> const SimulationClock&;

  [[nodiscard]] auto rng() -> DeterministicRng&;

  [[nodiscard]] auto commands() -> Game::Command::CommandQueue&;

  void reset();

  static auto active() -> SessionContext&;
  static auto active_or_null() -> SessionContext*;

  static auto for_world(const Engine::Core::World& world) -> SessionContext*;

  static auto set_active(SessionContext* session) -> SessionContext*;

  static auto set_thread_active(SessionContext* session) -> SessionContext*;

private:
  struct State;
  std::unique_ptr<State> m_state;
};

class ScopedSession {
public:
  explicit ScopedSession(SessionContext& session)
      : m_previous(SessionContext::set_active(&session)) {}

  ScopedSession(const ScopedSession&) = delete;
  ScopedSession(ScopedSession&&) = delete;
  auto operator=(const ScopedSession&) -> ScopedSession& = delete;
  auto operator=(ScopedSession&&) -> ScopedSession& = delete;

  ~ScopedSession() { SessionContext::set_active(m_previous); }

private:
  SessionContext* m_previous;
};

class ScopedThreadSession {
public:
  explicit ScopedThreadSession(SessionContext& session)
      : m_previous(SessionContext::set_thread_active(&session)) {}

  ScopedThreadSession(const ScopedThreadSession&) = delete;
  ScopedThreadSession(ScopedThreadSession&&) = delete;
  auto operator=(const ScopedThreadSession&) -> ScopedThreadSession& = delete;
  auto operator=(ScopedThreadSession&&) -> ScopedThreadSession& = delete;

  ~ScopedThreadSession() { SessionContext::set_thread_active(m_previous); }

private:
  SessionContext* m_previous;
};

} // namespace Game::Session
