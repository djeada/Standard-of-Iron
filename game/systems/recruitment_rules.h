#pragma once

#include "../units/spawn_type.h"
#include "../units/troop_type.h"

namespace Game::Systems {

inline constexpr int k_civilian_delivery_reserve_grant = 18;

// A home raises this many civilians in its lifetime; each is worth one delivery.
inline constexpr int k_home_lifetime_civilians = 6;

// A barracks raised by builders holds at least this many men of reserve, so a
// civilian walking in is almost never turned away (the authored starting
// barracks keep their own, larger reserve).
inline constexpr int k_built_barracks_reserve_ceiling = 120;

[[nodiscard]] constexpr auto
recruiting_building_for(Game::Units::TroopType unit_type) -> Game::Units::SpawnType {
  switch (unit_type) {
  case Game::Units::TroopType::Civilian:
    return Game::Units::SpawnType::Home;
  case Game::Units::TroopType::Healer:
    return Game::Units::SpawnType::Temple;
  case Game::Units::TroopType::Catapult:
  case Game::Units::TroopType::Ballista:
  case Game::Units::TroopType::Ram:
  case Game::Units::TroopType::SiegeTower:
    return Game::Units::SpawnType::Builder;
  default:
    return Game::Units::SpawnType::Barracks;
  }
}

} // namespace Game::Systems
