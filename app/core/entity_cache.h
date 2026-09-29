#pragma once

namespace Engine::Core {
class UnitSpawnedEvent;
class UnitDiedEvent;
} // namespace Engine::Core

namespace Game::Systems {
class OwnerRegistry;
}

struct EntityCache {
  int player_troop_count = 0;
  bool player_barracks_alive = false;
  bool enemy_barracks_alive = false;
  int enemy_barracks_count = 0;

  void apply_spawn(const Engine::Core::UnitSpawnedEvent& event,
                   int local_owner_id,
                   const Game::Systems::OwnerRegistry& owners);
  void apply_death(const Engine::Core::UnitDiedEvent& event,
                   int local_owner_id,
                   const Game::Systems::OwnerRegistry& owners);

  void reset() {
    player_troop_count = 0;
    player_barracks_alive = false;
    enemy_barracks_alive = false;
    enemy_barracks_count = 0;
  }
};
