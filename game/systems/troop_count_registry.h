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

  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent &event);
  void on_unit_died(const Engine::Core::UnitDiedEvent &event);

  void rebuild_from_world(Engine::Core::World &world);

private:
  TroopCountRegistry() = default;
  ~TroopCountRegistry() = default;
  TroopCountRegistry(const TroopCountRegistry &) = delete;
  auto operator=(const TroopCountRegistry &) -> TroopCountRegistry & = delete;

  std::unordered_map<int, int> m_troop_counts;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
};

} // namespace Game::Systems
