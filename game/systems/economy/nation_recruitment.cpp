#include "nation_recruitment.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/ambient_session.h"
#include "core/component_core.h"
#include "core/component_structures.h"
#include "core/world.h"
#include "systems/nation_registry.h"
#include "systems/troop_profile_service.h"

namespace Game::Systems {

auto owner_nation(const Engine::Core::World& world, int owner_id) -> const Nation* {
  const auto& services = Game::Session::services_for(world);
  if (services.nations == nullptr) {
    return nullptr;
  }
  if (const auto* nation = services.nations->get_nation_for_player(owner_id)) {
    return nation;
  }
  return services.nations->get_nation(services.nations->default_nation_id());
}

auto nation_may_build(const Engine::Core::World& world,
                      int owner_id,
                      std::string_view item_type) -> bool {
  const auto type = Game::Units::spawn_typeFromString(std::string(item_type));
  if (!type.has_value() || !Game::Units::is_nation_building_spawn(*type)) {
    return true;
  }
  const auto* nation = owner_nation(world, owner_id);
  return nation != nullptr && std::find(nation->unique_buildings.begin(),
                                        nation->unique_buildings.end(),
                                        item_type) != nation->unique_buildings.end();
}

auto civilian_reserve_grant(const Engine::Core::World& world, int owner_id) -> int {
  const auto* nation = owner_nation(world, owner_id);
  return nation != nullptr && nation->economy.civilian_reserve_grant > 0
             ? nation->economy.civilian_reserve_grant
             : k_civilian_delivery_reserve_grant;
}

auto resolve_band(const Nation& nation,
                  Game::Units::TroopType unit_type,
                  std::optional<NationID> band_nation) -> const MercenaryBand* {
  if (band_nation.has_value()) {
    return nation.find_band(unit_type, *band_nation);
  }
  if (const auto* own = nation.find_band(unit_type, nation.id)) {
    return own;
  }
  for (const auto& band : nation.mercenary_bands) {
    if (band.unit_type == unit_type) {
      return &band;
    }
  }
  return nullptr;
}

auto owner_has_standing(const Engine::Core::World& world,
                        int owner_id,
                        Game::Units::SpawnType type) -> bool {
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    (void)id;
    if (unit.owner_id == owner_id && unit.spawn_type == type && unit.health > 0) {
      return true;
    }
  }
  return false;
}

auto squads_fielded_or_training(const Engine::Core::World& world,
                                int owner_id,
                                Game::Units::TroopType type) -> int {
  const auto spawn = Game::Units::spawn_typeFromTroopType(type);
  int squads = 0;
  for (auto [id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.owner_id != owner_id || unit.health <= 0) {
      continue;
    }
    if (unit.spawn_type == spawn) {
      ++squads;
      continue;
    }
    if (!Game::Units::is_building_spawn(unit.spawn_type)) {
      continue;
    }
    const auto* production = world.try_get<Engine::Core::ProductionComponent>(id);
    if (production == nullptr) {
      continue;
    }
    if (production->in_progress && production->product_type == type) {
      ++squads;
    }
    squads += static_cast<int>(std::count(production->production_queue.begin(),
                                          production->production_queue.end(),
                                          type));
  }
  return squads;
}

auto quote_recruit(const Engine::Core::World& world,
                   int owner_id,
                   Game::Units::TroopType unit_type,
                   std::optional<NationID> band_nation) -> RecruitQuote {
  RecruitQuote quote;
  const Nation* nation = owner_nation(world, owner_id);
  if (nation == nullptr) {

    const auto& profile = TroopProfileService::instance().get_profile_ref(
        NationID::RomanRepublic, unit_type);
    quote.offered = true;
    quote.building = recruiting_building_for(unit_type);
    quote.manpower = profile.production.cost;
    quote.resources = profile.production.resource_costs;
    quote.build_time = profile.production.build_time;
    quote.individuals = std::max(1, profile.individuals_per_unit);
    return quote;
  }
  quote.troop_nation = nation->id;

  const MercenaryBand* band = nullptr;
  const auto* troop = nation->get_troop(unit_type);
  const bool hired = band_nation.has_value() || troop == nullptr || !troop->recruitable;
  if (hired) {
    band = resolve_band(*nation, unit_type, band_nation);
  }
  if (band != nullptr) {
    quote.offered = true;
    quote.band = band;
    quote.building = Game::Units::SpawnType::MercenaryCamp;
    quote.troop_nation = band->nation;
    quote.manpower = 0;
    quote.resources.set(ResourceType::Gold, band->gold);
    quote.build_time = band->contract_seconds;
    quote.individuals = std::max(1,
                                 TroopProfileService::instance()
                                     .get_profile_ref(band->nation, unit_type)
                                     .individuals_per_unit);
    return quote;
  }

  const auto& profile =
      TroopProfileService::instance().get_profile_ref(nation->id, unit_type);
  quote.offered = troop != nullptr ? troop->recruitable : true;
  quote.building = nation->recruiting_building(unit_type);
  quote.manpower = profile.production.cost;
  quote.resources = profile.production.resource_costs;
  quote.build_time = profile.production.build_time;
  quote.individuals = std::max(1, profile.individuals_per_unit);
  if (troop != nullptr) {
    quote.max_squads = troop->max_squads;
    quote.requires_building = troop->requires_building;
    if (troop->fabrica_discount && nation->economy.fabrica_iron_multiplier < 1.0F &&
        owner_has_standing(world, owner_id, Game::Units::SpawnType::Fabrica)) {
      const int iron = quote.resources.get(ResourceType::Iron);
      quote.resources.set(
          ResourceType::Iron,
          static_cast<int>(std::lround(static_cast<float>(iron) *
                                       nation->economy.fabrica_iron_multiplier)));
    }
  }
  return quote;
}

auto nation_recruit_ruling(const Engine::Core::World& world,
                           const Engine::Core::Entity& building,
                           Game::Units::TroopType unit_type,
                           std::optional<NationID> band_nation,
                           int planned) -> NationRecruitRefusal {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(building.get_id());
  if (unit == nullptr) {
    return NationRecruitRefusal::WrongBuilding;
  }
  const RecruitQuote quote =
      quote_recruit(world, unit->owner_id, unit_type, band_nation);
  if (!quote.offered) {
    return NationRecruitRefusal::NotInRoster;
  }
  if (quote.building != unit->spawn_type) {
    return NationRecruitRefusal::WrongBuilding;
  }
  if (quote.requires_building.has_value() &&
      !owner_has_standing(world, unit->owner_id, *quote.requires_building)) {
    return NationRecruitRefusal::NeedsBuilding;
  }
  if (quote.max_squads > 0 &&
      squads_fielded_or_training(world, unit->owner_id, unit_type) + planned >=
          quote.max_squads) {
    return NationRecruitRefusal::TypeLimitReached;
  }
  return NationRecruitRefusal::None;
}

} // namespace Game::Systems
