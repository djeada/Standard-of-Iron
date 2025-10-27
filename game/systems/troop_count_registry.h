#pragma once

#include "../core/event_manager.h"
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class TroopCountRegistry {
public:
  static auto instance() -> TroopCountRegistry &;

  void initialize();
  void clear();

  auto getTroopCount(int owner_id) const -> int;

  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);

  void rebuildFromWorld(Engine::Core::World &world);

private:
  TroopCountRegistry() = default;
  ~TroopCountRegistry() = default;
  TroopCountRegistry(const TroopCountRegistry &) = delete;
  auto operator=(const TroopCountRegistry &) -> TroopCountRegistry & = delete;

  std::unordered_map<int, int> m_troop_counts;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
};

} // namespace Game::Systems
