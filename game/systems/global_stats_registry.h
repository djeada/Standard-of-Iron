#pragma once

#include "../core/event_manager.h"
#include <chrono>
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct PlayerStats {
  int troops_recruited = 0;
  int enemies_killed = 0;
  int barracks_owned = 0;
  std::chrono::steady_clock::time_point game_start_time;
  std::chrono::steady_clock::time_point game_end_time;
  float play_time_sec = 0.0F;
  bool game_ended = false;
};

class GlobalStatsRegistry {
public:
  static auto instance() -> GlobalStatsRegistry &;

  void initialize();
  void clear();

  auto getStats(int owner_id) const -> const PlayerStats *;
  auto getStats(int owner_id) -> PlayerStats *;

  void mark_game_start(int owner_id);

  void mark_game_end(int owner_id);

  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent &event);
  void on_unit_died(const Engine::Core::UnitDiedEvent &event);
  void on_barrack_captured(const Engine::Core::BarrackCapturedEvent &event);

  void rebuild_from_world(Engine::Core::World &world);

private:
  GlobalStatsRegistry() = default;
  ~GlobalStatsRegistry() = default;
  GlobalStatsRegistry(const GlobalStatsRegistry &) = delete;
  auto operator=(const GlobalStatsRegistry &) -> GlobalStatsRegistry & = delete;

  std::unordered_map<int, PlayerStats> m_player_stats;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_subscription;
};

} // namespace Game::Systems
