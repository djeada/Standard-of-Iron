#pragma once

#include "../core/event_manager.h"
#include <chrono>
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct PlayerStats {
  int troopsRecruited = 0;    // Total units produced
  int enemiesKilled = 0;       // Total enemy units destroyed
  int barracksOwned = 0;       // Current number of barracks controlled
  std::chrono::steady_clock::time_point gameStartTime;
  std::chrono::steady_clock::time_point gameEndTime;
  float playTimeSec = 0.0f;    // Calculated play time in seconds
  bool gameEnded = false;      // Whether the game has ended for this player
};

class GlobalStatsRegistry {
public:
  static GlobalStatsRegistry &instance();

  void initialize();
  void clear();

  // Get statistics for a specific player
  const PlayerStats *getStats(int ownerId) const;
  PlayerStats *getStats(int ownerId);

  // Mark game start for a player
  void markGameStart(int ownerId);

  // Mark game end for a player (defeat or victory)
  void markGameEnd(int ownerId);

  // Event handlers
  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);

  // Rebuild statistics from current world state (for initialization)
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
