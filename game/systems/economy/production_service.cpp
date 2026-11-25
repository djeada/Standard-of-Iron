#include "production_service.h"

#include <algorithm>
#include <vector>

#include "core/ambient_session.h"
#include "core/component_structures.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game_config.h"
#include "nation_recruitment.h"
#include "systems/nation_registry.h"
#include "systems/owner_queries.h"
#include "systems/player_feedback.h"
#include "systems/player_resource_registry.h"
#include "systems/troop_profile_service.h"
#include "units/commander_catalog.h"
#include "units/spawn_type.h"
#include "units/troop_catalog.h"
#include "units/troop_config.h"
#include "units/troop_type.h"

namespace Game::Systems {

static auto
find_first_selected_barracks(Engine::Core::World& world,
                             const std::vector<Engine::Core::EntityID>& selected,
                             int owner_id) -> Engine::Core::Entity* {
  for (auto id : selected) {
    if (auto* e = world.get_entity(id)) {
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((u == nullptr) || u->owner_id != owner_id) {
        continue;
      }
      if (u->spawn_type == Game::Units::SpawnType::Barracks) {
        return e;
      }
    }
  }
  return nullptr;
}

static auto
find_first_selected_home(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& selected,
                         int owner_id) -> Engine::Core::Entity* {
  for (auto id : selected) {
    if (auto* e = world.get_entity(id)) {
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((u == nullptr) || u->owner_id != owner_id) {
        continue;
      }
      if (u->spawn_type == Game::Units::SpawnType::Home) {
        return e;
      }
    }
  }
  return nullptr;
}

static auto
find_first_selected_temple(Engine::Core::World& world,
                           const std::vector<Engine::Core::EntityID>& selected,
                           int owner_id) -> Engine::Core::Entity* {
  for (auto id : selected) {
    if (auto* e = world.get_entity(id)) {
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((u == nullptr) || u->owner_id != owner_id) {
        continue;
      }
      if (u->spawn_type == Game::Units::SpawnType::Temple) {
        return e;
      }
    }
  }
  return nullptr;
}

namespace {

auto resolve_nation_id(const Engine::Core::World& world,
                       int owner_id) -> Game::Systems::NationID {
  auto& registry = *Game::Session::services_for(world).nations;
  if (const auto* nation = registry.get_nation_for_player(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

void apply_production_profile(Engine::Core::ProductionComponent* prod,
                              Game::Systems::NationID nation_id,
                              Game::Units::TroopType unit_type) {
  if (prod == nullptr) {
    return;
  }
  const auto& profile =
      TroopProfileService::instance().get_profile_ref(nation_id, unit_type);
  prod->build_time = profile.production.build_time;
  prod->villager_cost = profile.production.cost;
}

auto home_committed_civilian_count(const Engine::Core::ProductionComponent* prod)
    -> int {
  if (prod == nullptr) {
    return 0;
  }

  return prod->produced_count + (prod->in_progress ? 1 : 0) +
         static_cast<int>(prod->production_queue.size());
}

} // namespace

namespace {

auto production_ruling(Engine::Core::World& world,
                       Engine::Core::Entity& building,
                       Game::Units::TroopType unit_type,
                       int planned = 0,
                       std::optional<NationID> band = std::nullopt)
    -> ProductionResult {
  const auto* unit = building.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return ProductionResult::NoBarracks;
  }
  const bool is_home = unit->spawn_type == Game::Units::SpawnType::Home;
  if (!is_home && Game::Units::is_commander_troop(unit_type)) {
    return ProductionResult::CommanderNotRecruitable;
  }
  switch (nation_recruit_ruling(world, building, unit_type, band, planned)) {
  case NationRecruitRefusal::None:
    break;
  case NationRecruitRefusal::NotInRoster:
    return ProductionResult::NotInRoster;
  case NationRecruitRefusal::WrongBuilding:
    return ProductionResult::WrongBuilding;
  case NationRecruitRefusal::NeedsBuilding:
    return ProductionResult::NeedsBuilding;
  case NationRecruitRefusal::TypeLimitReached:
    return ProductionResult::TypeLimitReached;
  }
  const auto* production = building.get_component<Engine::Core::ProductionComponent>();
  // Roster costs, a band's gold and the Fabrica's discount (NATION_ASYMMETRY.md).
  const RecruitQuote quote = quote_recruit(world, unit->owner_id, unit_type, band);
  const int production_cost = quote.manpower;
  const int manpower_available =
      (production != nullptr ? production->manpower_available : 0) -
      planned * production_cost;
  if (manpower_available < production_cost) {
    return ProductionResult::InsufficientManpower;
  }
  if (is_home) {
    if (production != nullptr &&
        home_committed_civilian_count(production) + planned >= production->max_units) {
      return ProductionResult::PerBarracksLimitReached;
    }
  } else {

    const int current_troops =
        Game::Systems::authoritative_troop_count_for(world, unit->owner_id);
    const int max_troops = Game::GameConfig::instance().get_max_troops_per_player();
    if (current_troops + (planned + 1) * std::max(1, quote.individuals) > max_troops) {
      return ProductionResult::GlobalTroopLimitReached;
    }
  }
  const int max_queue_size =
      is_home ? ProductionService::k_home_queue_limit
      // A Mercenary Camp holds one contract at a time (NATION_ASYMMETRY.md 11).
      : unit->spawn_type == Game::Units::SpawnType::MercenaryCamp ? 1
      : unit->spawn_type == Game::Units::SpawnType::Barracks
          ? ProductionService::k_barracks_queue_limit
          : ProductionService::k_other_queue_limit;
  int total_in_queue = 0;
  if (production != nullptr) {
    total_in_queue = (production->in_progress ? 1 : 0) +
                     static_cast<int>(production->production_queue.size());
  }
  if (total_in_queue + planned >= max_queue_size) {
    return ProductionResult::QueueFull;
  }
  ResourceAmounts required;
  for (const auto type : k_all_resource_types) {
    required.set(type, quote.resources.get(type) * (planned + 1));
  }
  if (!Game::Session::services_for(world).economy->has_at_least(unit->owner_id,
                                                                required)) {
    return ProductionResult::InsufficientResources;
  }
  return ProductionResult::Success;
}

} // namespace

auto ProductionService::can_start_production(Engine::Core::World& world,
                                             Engine::Core::EntityID building_id,
                                             Game::Units::TroopType unit_type,
                                             std::optional<NationID> band)
    -> ProductionResult {
  auto* building = world.get_entity(building_id);
  if (building == nullptr) {
    return ProductionResult::NoBarracks;
  }
  return production_ruling(world, *building, unit_type, 0, band);
}

auto ProductionService::start_production(Engine::Core::World& world,
                                         Engine::Core::EntityID building_id,
                                         Game::Units::TroopType unit_type,
                                         std::optional<NationID> band)
    -> ProductionResult {
  auto* building = world.get_entity(building_id);
  if (building == nullptr) {
    return ProductionResult::NoBarracks;
  }
  auto* p = building->get_component<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = building->add_component<Engine::Core::ProductionComponent>();
  }
  if (const auto ruling = production_ruling(world, *building, unit_type, 0, band);
      ruling != ProductionResult::Success) {
    return ruling;
  }
  const auto* unit = building->get_component<Engine::Core::UnitComponent>();
  const int owner_id = unit->owner_id;
  const auto nation_id = resolve_nation_id(world, owner_id);
  const RecruitQuote quote = quote_recruit(world, owner_id, unit_type, band);

  if (p->in_progress) {
    p->production_queue.push_back(unit_type);
  } else {
    p->product_type = unit_type;
    apply_production_profile(p, nation_id, unit_type);
    p->build_time = quote.build_time;
    p->time_remaining = p->build_time;
    p->in_progress = true;
    p->has_product_nation = quote.band != nullptr;
    p->product_nation = quote.troop_nation;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent::for_owner(owner_id, "build.unit_queued"));
  if (quote.manpower > 0) {
    spend_manpower(owner_id, building_id, *p, quote.manpower);
  }
  spend_resources(owner_id, building_id, quote.resources);
  return ProductionResult::Success;
}

auto ProductionService::plan_batch(Engine::Core::World& world,
                                   Engine::Core::EntityID building_id,
                                   Game::Units::TroopType unit_type,
                                   int wanted) -> BatchPlan {
  BatchPlan plan;
  auto* building = world.get_entity(building_id);
  if (building == nullptr) {
    plan.limit = ProductionResult::NoBarracks;
    return plan;
  }
  const int ceiling = std::clamp(wanted, 1, k_barracks_queue_limit);
  while (plan.count < ceiling) {
    const auto ruling = production_ruling(world, *building, unit_type, plan.count);
    if (ruling != ProductionResult::Success) {
      plan.limit = ruling;
      return plan;
    }
    ++plan.count;
  }
  return plan;
}

auto ProductionService::start_production_batch(Engine::Core::World& world,
                                               Engine::Core::EntityID building_id,
                                               Game::Units::TroopType unit_type,
                                               int count) -> int {
  int started = 0;
  const int wanted = std::clamp(count, 1, k_barracks_queue_limit);
  while (started < wanted &&
         start_production(world, building_id, unit_type) == ProductionResult::Success) {
    ++started;
  }
  return started;
}

auto ProductionService::toggle_repeat(Engine::Core::World& world,
                                      Engine::Core::EntityID building_id,
                                      Game::Units::TroopType unit_type) -> bool {
  auto* p = world.try_get<Engine::Core::ProductionComponent>(building_id);
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(building_id);
  if (p == nullptr || unit == nullptr ||
      unit->spawn_type != quote_recruit(world, unit->owner_id, unit_type).building) {
    return false;
  }
  if (p->repeat_active && p->repeat_product == unit_type) {
    p->repeat_active = false;
    return false;
  }
  p->repeat_active = true;
  p->repeat_product = unit_type;
  return true;
}

void ProductionService::top_up_repeats(Engine::Core::World& world) {
  for (auto [entity_ref, prod_ref] :
       world.entity_view<Engine::Core::ProductionComponent>()) {
    Engine::Core::ProductionComponent& prod = prod_ref;
    if (!prod.repeat_active) {
      continue;
    }
    const auto id = entity_ref.get_id();
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || Game::Core::is_neutral_owner(unit->owner_id) ||
        unit->health <= 0) {
      prod.repeat_active = false;
      continue;
    }
    const int in_line =
        (prod.in_progress ? 1 : 0) + static_cast<int>(prod.production_queue.size());
    if (in_line >= k_repeat_lookahead) {
      continue;
    }
    (void)start_production(world, id, prod.repeat_product);
  }
}

auto ProductionService::set_rally_point(Engine::Core::World& world,
                                        Engine::Core::EntityID building_id,
                                        float x,
                                        float z) -> bool {
  auto* e = world.get_entity(building_id);
  if (e == nullptr) {
    return false;
  }
  auto* p = e->get_component<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = e->add_component<Engine::Core::ProductionComponent>();
  }
  if (p == nullptr) {
    return false;
  }
  p->rally_x = x;
  p->rally_z = z;
  p->rally_set = true;
  p->rally_default = false;
  return true;
}

auto ProductionService::find_selected_barracks(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id) -> Engine::Core::EntityID {
  auto* e = find_first_selected_barracks(world, selected, owner_id);
  return e != nullptr ? e->get_id() : Engine::Core::NULL_ENTITY;
}

auto ProductionService::find_selected_home(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id) -> Engine::Core::EntityID {
  auto* e = find_first_selected_home(world, selected, owner_id);
  return e != nullptr ? e->get_id() : Engine::Core::NULL_ENTITY;
}

auto ProductionService::find_selected_temple(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id) -> Engine::Core::EntityID {
  auto* e = find_first_selected_temple(world, selected, owner_id);
  return e != nullptr ? e->get_id() : Engine::Core::NULL_ENTITY;
}

auto ProductionService::cheapest_recruit_cost(Game::Units::SpawnType building) -> int {
  int cheapest = 0;
  for (const auto& [type, troop_class] :
       Game::Units::TroopCatalog::instance().get_all_classes()) {
    if (troop_class.production.cost <= 0 || Game::Units::is_commander_troop(type) ||
        recruiting_building_for(type) != building) {
      continue;
    }
    cheapest = cheapest == 0 ? troop_class.production.cost
                             : std::min(cheapest, troop_class.production.cost);
  }
  return cheapest;
}

auto ProductionService::reserve_is_short(
    const Engine::Core::ProductionComponent& production,
    Game::Units::SpawnType building) -> bool {
  if (!Game::Units::is_recruitment_building(building)) {
    return false;
  }
  const int cheapest = cheapest_recruit_cost(building);
  return cheapest > 0 && production.manpower_available < cheapest;
}

auto ProductionService::get_selected_barracks_state(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    ProductionState& out_state) -> bool {
  auto* e = find_first_selected_barracks(world, selected, owner_id);
  if (e == nullptr) {
    out_state = {};
    return false;
  }
  out_state = {};
  out_state.has_barracks = true;
  if (e->get_component<Engine::Core::UnitComponent>() != nullptr) {
    out_state.nation_id = resolve_nation_id(world, owner_id);
  } else {
    out_state.nation_id =
        Game::Session::services_for(world).nations->default_nation_id();
  }
  if (auto* p = e->get_component<Engine::Core::ProductionComponent>()) {
    out_state.in_progress = p->in_progress;
    out_state.product_type = p->product_type;
    out_state.time_remaining = p->time_remaining;
    out_state.build_time = p->build_time;
    out_state.produced_count = p->produced_count;
    out_state.max_units = p->manpower_limit();
    out_state.villager_cost = p->villager_cost;
    out_state.manpower_available = p->manpower_available;
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
    out_state.repeat_active = p->repeat_active;
    out_state.repeat_product = p->repeat_product;
    out_state.cheapest_recruit_cost =
        cheapest_recruit_cost(Game::Units::SpawnType::Barracks);
    out_state.reserve_short = reserve_is_short(*p, Game::Units::SpawnType::Barracks);
  }
  return true;
}

auto ProductionService::get_selected_home_state(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    ProductionState& out_state) -> bool {
  auto* e = find_first_selected_home(world, selected, owner_id);
  if (e == nullptr) {
    out_state = {};
    return false;
  }

  out_state = {};
  out_state.has_home = true;
  if (e->get_component<Engine::Core::UnitComponent>() != nullptr) {
    out_state.nation_id = resolve_nation_id(world, owner_id);
  } else {
    out_state.nation_id =
        Game::Session::services_for(world).nations->default_nation_id();
  }

  if (auto* p = e->get_component<Engine::Core::ProductionComponent>()) {
    out_state.in_progress = p->in_progress;
    out_state.product_type = p->product_type;
    out_state.time_remaining = p->time_remaining;
    out_state.build_time = p->build_time;
    out_state.produced_count = p->produced_count;
    out_state.max_units = p->manpower_limit();
    out_state.villager_cost = p->villager_cost;
    out_state.manpower_available = p->manpower_available;
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
  }
  return true;
}

auto ProductionService::get_selected_temple_state(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    ProductionState& out_state) -> bool {
  auto* e = find_first_selected_temple(world, selected, owner_id);
  if (e == nullptr) {
    out_state = {};
    return false;
  }

  out_state = {};
  out_state.has_temple = true;

  out_state.nation_id = resolve_nation_id(world, owner_id);

  if (auto* p = e->get_component<Engine::Core::ProductionComponent>()) {
    out_state.in_progress = p->in_progress;
    out_state.product_type = p->product_type;
    out_state.time_remaining = p->time_remaining;
    out_state.build_time = p->build_time;
    out_state.produced_count = p->produced_count;
    out_state.max_units = p->manpower_limit();
    out_state.villager_cost = p->villager_cost;
    out_state.manpower_available = p->manpower_available;
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
    out_state.cheapest_recruit_cost =
        cheapest_recruit_cost(Game::Units::SpawnType::Temple);
    out_state.reserve_short = reserve_is_short(*p, Game::Units::SpawnType::Temple);
  }
  return true;
}

} // namespace Game::Systems
