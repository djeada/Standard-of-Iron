#include "production_service.h"

#include <vector>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../systems/nation_registry.h"
#include "../systems/player_resource_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/commander_catalog.h"
#include "../units/troop_config.h"
#include "core/entity.h"
#include "owner_queries.h"
#include "units/spawn_type.h"
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

namespace {

auto resolve_nation_id(int owner_id) -> Game::Systems::NationID {
  auto& registry = NationRegistry::instance();
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
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);
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

auto production_ruling(Engine::Core::Entity& building,
                       Game::Units::TroopType unit_type) -> ProductionResult {
  const auto* unit = building.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return ProductionResult::NoBarracks;
  }
  const bool is_home = unit->spawn_type == Game::Units::SpawnType::Home;
  if (!is_home && Game::Units::is_commander_troop(unit_type)) {
    return ProductionResult::CommanderNotRecruitable;
  }
  const auto* production = building.get_component<Engine::Core::ProductionComponent>();
  const auto profile = TroopProfileService::instance().get_profile(
      resolve_nation_id(unit->owner_id), unit_type);
  const int production_cost = profile.production.cost;
  const int manpower_available =
      production != nullptr ? production->manpower_available : 0;
  if (manpower_available < production_cost) {
    return ProductionResult::InsufficientManpower;
  }
  if (is_home) {
    if (production != nullptr &&
        home_committed_civilian_count(production) >= production->max_units) {
      return ProductionResult::PerBarracksLimitReached;
    }
  } else {
    const int current_troops = Game::Systems::troop_count_for(unit->owner_id);
    const int max_troops = Game::GameConfig::instance().get_max_troops_per_player();
    if (current_troops + production_cost > max_troops) {
      return ProductionResult::GlobalTroopLimitReached;
    }
  }
  const int max_queue_size = is_home ? 3 : 5;
  int total_in_queue = 0;
  if (production != nullptr) {
    total_in_queue = (production->in_progress ? 1 : 0) +
                     static_cast<int>(production->production_queue.size());
  }
  if (total_in_queue >= max_queue_size) {
    return ProductionResult::QueueFull;
  }
  if (!PlayerResourceRegistry::instance().has_at_least(
          unit->owner_id, profile.production.resource_costs)) {
    return ProductionResult::InsufficientResources;
  }
  return ProductionResult::Success;
}

} // namespace

auto ProductionService::can_start_production(Engine::Core::World& world,
                                             Engine::Core::EntityID building_id,
                                             Game::Units::TroopType unit_type)
    -> ProductionResult {
  auto* building = world.get_entity(building_id);
  if (building == nullptr) {
    return ProductionResult::NoBarracks;
  }
  return production_ruling(*building, unit_type);
}

auto ProductionService::start_production(Engine::Core::World& world,
                                         Engine::Core::EntityID building_id,
                                         Game::Units::TroopType unit_type)
    -> ProductionResult {
  auto* building = world.get_entity(building_id);
  if (building == nullptr) {
    return ProductionResult::NoBarracks;
  }
  auto* p = building->get_component<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = building->add_component<Engine::Core::ProductionComponent>();
  }
  if (const auto ruling = production_ruling(*building, unit_type);
      ruling != ProductionResult::Success) {
    return ruling;
  }
  const auto* unit = building->get_component<Engine::Core::UnitComponent>();
  const int owner_id = unit->owner_id;
  const auto nation_id = resolve_nation_id(owner_id);
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);

  if (p->in_progress) {
    p->production_queue.push_back(unit_type);
  } else {
    p->product_type = unit_type;
    apply_production_profile(p, nation_id, unit_type);
    p->time_remaining = p->build_time;
    p->in_progress = true;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent("build.unit_queued"));
  p->manpower_available -= profile.production.cost;
  PlayerResourceRegistry::instance().spend(owner_id, profile.production.resource_costs);
  return ProductionResult::Success;
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
    out_state.nation_id = resolve_nation_id(owner_id);
  } else {
    out_state.nation_id = NationRegistry::instance().default_nation_id();
  }
  if (auto* p = e->get_component<Engine::Core::ProductionComponent>()) {
    out_state.in_progress = p->in_progress;
    out_state.product_type = p->product_type;
    out_state.time_remaining = p->time_remaining;
    out_state.build_time = p->build_time;
    out_state.produced_count = p->produced_count;
    out_state.max_units = p->max_units;
    out_state.villager_cost = p->villager_cost;
    out_state.manpower_available = p->manpower_available;
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
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
    out_state.nation_id = resolve_nation_id(owner_id);
  } else {
    out_state.nation_id = NationRegistry::instance().default_nation_id();
  }

  if (auto* p = e->get_component<Engine::Core::ProductionComponent>()) {
    out_state.in_progress = p->in_progress;
    out_state.product_type = p->product_type;
    out_state.time_remaining = p->time_remaining;
    out_state.build_time = p->build_time;
    out_state.produced_count = p->produced_count;
    out_state.max_units = p->max_units;
    out_state.villager_cost = p->villager_cost;
    out_state.manpower_available = p->manpower_available;
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
  }
  return true;
}

} // namespace Game::Systems
