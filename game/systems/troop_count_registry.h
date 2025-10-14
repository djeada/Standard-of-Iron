#pragma once

#include "../core/event_manager.h"
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class TroopCountRegistry {
public:
  static TroopCountRegistry &instance();

  void initialize();
  void clear();

  int getTroopCount(int ownerId) const;

  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);

  void rebuildFromWorld(Engine::Core::World &world);

private:
  TroopCountRegistry() = default;
  ~TroopCountRegistry() = default;
  TroopCountRegistry(const TroopCountRegistry &) = delete;
  TroopCountRegistry &operator=(const TroopCountRegistry &) = delete;

  std::unordered_map<int, int> m_troopCounts;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
};

} // namespace Game::Systems
