#include "production_service.h"

#include <optional>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../systems/nation_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/commander_catalog.h"
#include "../units/troop_config.h"
#include "core/entity.h"
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

auto resolve_nation_id(const Engine::Core::UnitComponent* unit,
                       int owner_id) -> Game::Systems::NationID {
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

auto production_owner(const Engine::Core::Entity* entity) -> std::optional<int> {
  if (entity == nullptr) {
    return std::nullopt;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return std::nullopt;
  }
  return unit->owner_id;
}

auto owner_has_commander_committed(Engine::Core::World& world, int owner_id) -> bool {
  auto has_commander_in_queue =
      [](const Engine::Core::ProductionComponent* prod) -> bool {
    if (prod == nullptr) {
      return false;
    }
    if (prod->in_progress && Game::Units::is_commander_troop(prod->product_type)) {
      return true;
    }
    for (const auto queued : prod->production_queue) {
      if (Game::Units::is_commander_troop(queued)) {
        return true;
      }
    }
    return false;
  };

  for (const auto& entry : world.get_entities()) {
    auto* entity = entry.second.get();
    if (entity == nullptr) {
      continue;
    }

    if (auto* unit = entity->get_component<Engine::Core::UnitComponent>()) {
      if (unit->owner_id == owner_id && unit->health > 0) {
        const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
        if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
          return true;
        }
      }
    }

    auto* prod = entity->get_component<Engine::Core::ProductionComponent>();
    if (prod == nullptr || !has_commander_in_queue(prod)) {
      continue;
    }
    const auto owner = production_owner(entity);
    if (owner.has_value() && *owner == owner_id) {
      return true;
    }
  }
  return false;
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

auto ProductionService::start_production_for_first_selected_barracks(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    Game::Units::TroopType unit_type) -> ProductionResult {
  auto* e = find_first_selected_barracks(world, selected, owner_id);
  if (e == nullptr) {
    return ProductionResult::NoBarracks;
  }
  auto* unit = e->get_component<Engine::Core::UnitComponent>();
  const auto nation_id = resolve_nation_id(unit, owner_id);
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);

  auto* p = e->get_component<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = e->add_component<Engine::Core::ProductionComponent>();
  }
  if (p == nullptr) {
    return ProductionResult::NoBarracks;
  }

  int const individuals_per_unit = profile.individuals_per_unit;
  int const production_cost = profile.production.cost;
  const bool is_commander = Game::Units::is_commander_troop(unit_type);
  if (is_commander && owner_has_commander_committed(world, owner_id)) {
    return ProductionResult::CommanderLimitReached;
  }

  if (p->manpower_available < production_cost) {
    return ProductionResult::InsufficientManpower;
  }

  if (p->produced_count + production_cost > p->max_units) {
    return ProductionResult::PerBarracksLimitReached;
  }

  int const current_troops = Engine::Core::World::count_troops_for_player(owner_id);
  int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
  if (current_troops + production_cost > max_troops) {
    return ProductionResult::GlobalTroopLimitReached;
  }

  const int max_queue_size = 5;
  int total_in_queue = p->in_progress ? 1 : 0;
  total_in_queue += static_cast<int>(p->production_queue.size());

  if (total_in_queue >= max_queue_size) {
    return ProductionResult::QueueFull;
  }

  if (p->in_progress) {
    p->production_queue.push_back(unit_type);
  } else {
    p->product_type = unit_type;
    apply_production_profile(p, nation_id, unit_type);
    p->time_remaining = p->build_time;
    p->in_progress = true;
  }
  if (is_commander) {
    p->commander_committed = true;
  } else {
    p->commander_committed = owner_has_commander_committed(world, owner_id);
  }
  p->manpower_available -= production_cost;

  return ProductionResult::Success;
}

auto ProductionService::set_rally_for_first_selected_barracks(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    float x,
    float z) -> bool {
  auto* e = find_first_selected_barracks(world, selected, owner_id);
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
  if (auto* unit = e->get_component<Engine::Core::UnitComponent>()) {
    out_state.nation_id = resolve_nation_id(unit, owner_id);
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
    out_state.commander_committed = owner_has_commander_committed(world, owner_id);
    out_state.queue_size = static_cast<int>(p->production_queue.size());
    out_state.production_queue = p->production_queue;
  }
  return true;
}

auto ProductionService::start_production_for_first_selected_home(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& selected,
    int owner_id,
    Game::Units::TroopType unit_type) -> ProductionResult {
  auto* e = find_first_selected_home(world, selected, owner_id);
  if (e == nullptr) {
    return ProductionResult::NoBarracks;
  }

  auto* unit = e->get_component<Engine::Core::UnitComponent>();
  const auto nation_id = resolve_nation_id(unit, owner_id);
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, unit_type);

  auto* p = e->get_component<Engine::Core::ProductionComponent>();
  if (p == nullptr) {
    p = e->add_component<Engine::Core::ProductionComponent>();
  }
  if (p == nullptr) {
    return ProductionResult::NoBarracks;
  }

  int const production_cost = profile.production.cost;
  if (p->manpower_available < production_cost) {
    return ProductionResult::InsufficientManpower;
  }

  if (home_committed_civilian_count(p) >= p->max_units) {
    return ProductionResult::PerBarracksLimitReached;
  }

  const int max_queue_size = 3;
  int total_in_queue = p->in_progress ? 1 : 0;
  total_in_queue += static_cast<int>(p->production_queue.size());
  if (total_in_queue >= max_queue_size) {
    return ProductionResult::QueueFull;
  }

  if (p->in_progress) {
    p->production_queue.push_back(unit_type);
  } else {
    p->product_type = unit_type;
    apply_production_profile(p, nation_id, unit_type);
    p->time_remaining = p->build_time;
    p->in_progress = true;
  }

  p->manpower_available -= production_cost;
  return ProductionResult::Success;
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
  if (auto* unit = e->get_component<Engine::Core::UnitComponent>()) {
    out_state.nation_id = resolve_nation_id(unit, owner_id);
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
