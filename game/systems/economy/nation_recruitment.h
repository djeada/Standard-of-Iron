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

// What one recruit of `unit_type` costs an owner right now, and where it comes
// from. The single price rule for players, the AI and the HUD
// (NATION_ASYMMETRY.md): nation roster costs, mercenary bands priced in gold,
// and the Fabrica's iron discount.
struct RecruitQuote {
  bool offered = false;
  Game::Units::SpawnType building = Game::Units::SpawnType::Barracks;
  int manpower = 0;
  ResourceAmounts resources{};
  float build_time = 0.0F;
  int individuals = 1;
  // The nation whose profile the squad takes: a band's own nation.
  NationID troop_nation = NationID::RomanRepublic;
  const MercenaryBand* band = nullptr;
  int max_squads = 0;
  std::optional<Game::Units::SpawnType> requires_building;
};

[[nodiscard]] auto owner_nation(const Engine::Core::World& world,
                                int owner_id) -> const Nation*;

// Whether the owner's nation may raise `item_type`: anyone may raise the
// shared buildings, only Carthage its Mercenary Camp and Elephant Stables,
// only Rome its Fabrica and Castra.
[[nodiscard]] auto nation_may_build(const Engine::Core::World& world,
                                    int owner_id,
                                    std::string_view item_type) -> bool;

// Men one civilian adds to a barracks reserve for this owner's nation.
[[nodiscard]] auto civilian_reserve_grant(const Engine::Core::World& world,
                                          int owner_id) -> int;

// A band of `nation` offered for `unit_type`; with no band nation named, the
// owner's own band of that troop or, failing that, the first one.
[[nodiscard]] auto
resolve_band(const Nation& nation,
             Game::Units::TroopType unit_type,
             std::optional<NationID> band_nation) -> const MercenaryBand*;

[[nodiscard]] auto
quote_recruit(const Engine::Core::World& world,
              int owner_id,
              Game::Units::TroopType unit_type,
              std::optional<NationID> band_nation = std::nullopt) -> RecruitQuote;

// A finished, living building of `type` that `owner_id` holds.
[[nodiscard]] auto owner_has_standing(const Engine::Core::World& world,
                                      int owner_id,
                                      Game::Units::SpawnType type) -> bool;

// Squads of `type` the owner fields or has in training anywhere.
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

// The nation half of a recruit ruling: is it on this nation's roster, at this
// building, with the building it needs standing, and under its squad cap.
// `planned` counts recruits of the same order already accepted.
[[nodiscard]] auto nation_recruit_ruling(const Engine::Core::World& world,
                                         const Engine::Core::Entity& building,
                                         Game::Units::TroopType unit_type,
                                         std::optional<NationID> band_nation,
                                         int planned = 0) -> NationRecruitRefusal;

} // namespace Game::Systems
