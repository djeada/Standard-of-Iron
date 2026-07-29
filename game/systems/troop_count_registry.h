#pragma once

#include <unordered_map>

#include "../core/event_manager.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class TroopCountRegistry {
public:
  TroopCountRegistry() = default;
  ~TroopCountRegistry() = default;
  TroopCountRegistry(const TroopCountRegistry&) = delete;
  TroopCountRegistry(TroopCountRegistry&&) = delete;
  auto operator=(const TroopCountRegistry&) -> TroopCountRegistry& = delete;
  auto operator=(TroopCountRegistry&&) -> TroopCountRegistry& = delete;

  static auto instance() -> TroopCountRegistry&;

  void initialize();
  void clear();

  auto get_troop_count(int owner_id) const -> int;

  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);

  void rebuild_from_world(Engine::Core::World& world);

private:
  std::unordered_map<int, int> m_troop_counts;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
};

} // namespace Game::Systems
