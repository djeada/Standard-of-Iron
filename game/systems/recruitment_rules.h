#pragma once

#include "../units/spawn_type.h"
#include "../units/troop_type.h"

namespace Game::Systems {

inline constexpr int k_civilian_delivery_reserve_grant = 18;

[[nodiscard]] constexpr auto
recruiting_building_for(Game::Units::TroopType unit_type) -> Game::Units::SpawnType {
  switch (unit_type) {
  case Game::Units::TroopType::Civilian:
    return Game::Units::SpawnType::Home;
  case Game::Units::TroopType::Healer:
    return Game::Units::SpawnType::Temple;
  case Game::Units::TroopType::Catapult:
  case Game::Units::TroopType::Ballista:
    return Game::Units::SpawnType::Builder;
  default:
    return Game::Units::SpawnType::Barracks;
  }
}

} // namespace Game::Systems
