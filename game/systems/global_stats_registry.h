#pragma once

#include "../core/event_manager.h"
#include <chrono>
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct PlayerStats {
  int troopsRecruited = 0;
  int enemiesKilled = 0;
  int barracksOwned = 0;
  std::chrono::steady_clock::time_point gameStartTime;
  std::chrono::steady_clock::time_point gameEndTime;
  float playTimeSec = 0.0F;
  bool gameEnded = false;
};

class GlobalStatsRegistry {
public:
  static auto instance() -> GlobalStatsRegistry &;

  void initialize();
  void clear();

  auto getStats(int owner_id) const -> const PlayerStats *;
  auto getStats(int owner_id) -> PlayerStats *;

  void markGameStart(int owner_id);

  void markGameEnd(int owner_id);

  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);
  void onBarrackCaptured(const Engine::Core::BarrackCapturedEvent &event);

  void rebuildFromWorld(Engine::Core::World &world);

private:
  GlobalStatsRegistry() = default;
  ~GlobalStatsRegistry() = default;
  GlobalStatsRegistry(const GlobalStatsRegistry &) = delete;
  auto operator=(const GlobalStatsRegistry &) -> GlobalStatsRegistry & = delete;

  std::unordered_map<int, PlayerStats> m_playerStats;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrackCapturedSubscription;
};

} // namespace Game::Systems
