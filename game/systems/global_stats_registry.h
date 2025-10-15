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
  float playTimeSec = 0.0f;
  bool gameEnded = false;
};

class GlobalStatsRegistry {
public:
  static GlobalStatsRegistry &instance();

  void initialize();
  void clear();

  const PlayerStats *getStats(int ownerId) const;
  PlayerStats *getStats(int ownerId);

  void markGameStart(int ownerId);

  void markGameEnd(int ownerId);

  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);

  void rebuildFromWorld(Engine::Core::World &world);

private:
  GlobalStatsRegistry() = default;
  ~GlobalStatsRegistry() = default;
  GlobalStatsRegistry(const GlobalStatsRegistry &) = delete;
  GlobalStatsRegistry &operator=(const GlobalStatsRegistry &) = delete;

  std::unordered_map<int, PlayerStats> m_playerStats;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
};

} // namespace Game::Systems
