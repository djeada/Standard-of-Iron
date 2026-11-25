#pragma once

#include <optional>
#include <string_view>

#include "core/entity_id.h"
#include "systems/nation_id.h"
#include "systems/resource_types.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

struct Nation;
struct MercenaryBand;

struct RecruitQuote {
  bool offered = false;
  Game::Units::SpawnType building = Game::Units::SpawnType::Barracks;
  int manpower = 0;
  ResourceAmounts resources{};
  float build_time = 0.0F;
  int individuals = 1;

  NationID troop_nation = NationID::RomanRepublic;
  const MercenaryBand* band = nullptr;
  int max_squads = 0;
  std::optional<Game::Units::SpawnType> requires_building;
};

[[nodiscard]] auto owner_nation(const Engine::Core::World& world,
                                int owner_id) -> const Nation*;

[[nodiscard]] auto nation_may_build(const Engine::Core::World& world,
                                    int owner_id,
                                    std::string_view item_type) -> bool;

[[nodiscard]] auto civilian_reserve_grant(const Engine::Core::World& world,
                                          int owner_id) -> int;

[[nodiscard]] auto
resolve_band(const Nation& nation,
             Game::Units::TroopType unit_type,
             std::optional<NationID> band_nation) -> const MercenaryBand*;

[[nodiscard]] auto
quote_recruit(const Engine::Core::World& world,
              int owner_id,
              Game::Units::TroopType unit_type,
              std::optional<NationID> band_nation = std::nullopt) -> RecruitQuote;

[[nodiscard]] auto owner_has_standing(const Engine::Core::World& world,
                                      int owner_id,
                                      Game::Units::SpawnType type) -> bool;

[[nodiscard]] auto squads_fielded_or_training(const Engine::Core::World& world,
                                              int owner_id,
                                              Game::Units::TroopType type) -> int;

enum class NationRecruitRefusal {
  None,
  NotInRoster,
  WrongBuilding,
  NeedsBuilding,
  TypeLimitReached
};

[[nodiscard]] auto nation_recruit_ruling(const Engine::Core::World& world,
                                         const Engine::Core::Entity& building,
                                         Game::Units::TroopType unit_type,
                                         std::optional<NationID> band_nation,
                                         int planned = 0) -> NationRecruitRefusal;

} // namespace Game::Systems
