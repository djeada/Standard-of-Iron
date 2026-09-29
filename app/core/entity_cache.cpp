#include "app/core/entity_cache.h"

#include <algorithm>

#include "game/core/event_manager.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"

void EntityCache::apply_spawn(const Engine::Core::UnitSpawnedEvent& event,
                              int local_owner_id,
                              const Game::Systems::OwnerRegistry& owners) {
  if (event.owner_id == local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      player_barracks_alive = true;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      player_troop_count += production_cost;
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      enemy_barracks_count++;
      enemy_barracks_alive = true;
    }
  }
}

void EntityCache::apply_death(const Engine::Core::UnitDiedEvent& event,
                              int local_owner_id,
                              const Game::Systems::OwnerRegistry& owners) {
  if (event.owner_id == local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      player_barracks_alive = false;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      player_troop_count -= production_cost;
      player_troop_count = std::max(0, player_troop_count);
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      enemy_barracks_count--;
      enemy_barracks_count = std::max(0, enemy_barracks_count);
      enemy_barracks_alive = (enemy_barracks_count > 0);
    }
  }
}
