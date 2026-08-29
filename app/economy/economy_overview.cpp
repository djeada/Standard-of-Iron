#include "app/economy/economy_overview.h"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_queries.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_stockpile.h"
#include "game/systems/resource_types.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

namespace App::Core {

namespace {

using Game::Systems::ResourceAmounts;
using Game::Systems::ResourceType;

constexpr std::array<std::string_view, 9> k_buildable_items = {
    "home",
    "farm",
    "defense_tower",
    "marketplace",
    "temple",
    "wall_segment",
    "wall_gate",
    "catapult",
    "ballista",
};

constexpr int k_home_reserve_bonus = 50;

struct OwnerScan {
  int builder_count = 0;
  int idle_builder_count = 0;
  int constructing_builder_count = 0;
  std::array<int, Game::Systems::k_resource_type_count> gathering_workers{};
  ResourceAmounts carrying{};
  int barracks_count = 0;
  int home_count = 0;
  int marketplace_count = 0;
  int temple_count = 0;
  int farm_count = 0;
  int ripe_farm_count = 0;
  int building_count = 0;
  int barracks_manpower = 0;
  bool barracks_producing = false;
};

auto troop_manpower_of(const EconomyOverviewRequest& request) -> int {
  return request.world != nullptr
             ? Game::Systems::troop_count_for(*request.world, request.owner_id)
             : 0;
}

auto scan_owner(Engine::Core::World* world, int owner_id) -> OwnerScan {
  OwnerScan scan;
  if (world == nullptr) {
    return scan;
  }

  for (auto* entity : world->collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->owner_id != owner_id || unit->health <= 0) {
      continue;
    }

    if (const auto* carry =
            entity->get_component<Engine::Core::ResourceCarryComponent>()) {
      for (const ResourceType type : Game::Systems::k_all_resource_types) {
        scan.carrying.add(type, carry->amounts.get(type));
      }
    }

    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      ++scan.building_count;
      switch (unit->spawn_type) {
      case Game::Units::SpawnType::Barracks: {
        ++scan.barracks_count;
        if (const auto* production =
                entity->get_component<Engine::Core::ProductionComponent>()) {
          scan.barracks_manpower += production->manpower_available;
          scan.barracks_producing = scan.barracks_producing || production->in_progress;
        }
        break;
      }
      case Game::Units::SpawnType::Home:
        ++scan.home_count;
        break;
      case Game::Units::SpawnType::Marketplace:
        ++scan.marketplace_count;
        break;
      case Game::Units::SpawnType::Temple:
        ++scan.temple_count;
        break;
      case Game::Units::SpawnType::Farm:
        ++scan.farm_count;
        if (const auto* farm = entity->get_component<Engine::Core::FarmComponent>();
            farm != nullptr && farm->ripe()) {
          ++scan.ripe_farm_count;
        }
        break;
      default:
        break;
      }
      continue;
    }

    if (unit->spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }
    ++scan.builder_count;
    const auto* builder =
        entity->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder == nullptr) {
      ++scan.idle_builder_count;
      continue;
    }

    const bool busy = builder->in_progress || builder->has_construction_site ||
                      builder->has_task_target ||
                      builder->structure_task_entity_id != 0;
    if (const auto resource =
            Game::Systems::resource_for_harvest_product(builder->product_type);
        resource.has_value() && busy) {
      ++scan.gathering_workers[Game::Systems::resource_type_index(*resource)];
    } else if (busy && !builder->product_type.empty()) {
      ++scan.constructing_builder_count;
    } else if (!builder->has_gather_order && !builder->auto_gather) {
      ++scan.idle_builder_count;
    } else if (const auto gathering = Game::Systems::resource_for_harvest_product(
                   builder->gather_product_type);
               gathering.has_value()) {
      ++scan.gathering_workers[Game::Systems::resource_type_index(*gathering)];
    }
  }
  return scan;
}

auto costs_to_map(const ResourceAmounts& costs) -> QVariantMap {
  QVariantMap map;
  for (const ResourceType type : Game::Systems::k_all_resource_types) {
    if (const int amount = costs.get(type); amount > 0) {
      map[QLatin1String(Game::Systems::resource_type_key(type))] = amount;
    }
  }
  return map;
}

auto missing_to_map(const ResourceAmounts& costs,
                    const ResourceAmounts& available) -> QVariantMap {
  QVariantMap missing;
  for (const ResourceType type : Game::Systems::k_all_resource_types) {
    const int shortfall = costs.get(type) - available.get(type);
    if (shortfall > 0) {
      missing[QLatin1String(Game::Systems::resource_type_key(type))] = shortfall;
    }
  }
  return missing;
}

struct CostedItem {
  QString key;
  QString display_name;
  ResourceAmounts costs;
};

auto building_items() -> std::vector<CostedItem> {
  std::vector<CostedItem> items;
  items.reserve(k_buildable_items.size());
  for (const auto item : k_buildable_items) {
    const std::string item_str(item);
    items.push_back(
        {.key = QString::fromStdString(item_str),
         .display_name = QString::fromStdString(item_str),
         .costs = Game::Systems::construction_cost_info(item).resource_costs});
  }
  return items;
}

auto is_barracks_recruit(Game::Units::TroopType type) -> bool {
  if (Game::Units::is_commander_troop(type)) {
    return false;
  }
  switch (type) {
  case Game::Units::TroopType::Catapult:
  case Game::Units::TroopType::Ballista:
  case Game::Units::TroopType::Civilian:
    return false;
  default:
    return true;
  }
}

struct UnitItem {
  CostedItem item;
  int manpower_cost = 0;
  int army_cap_weight = 0;
  float build_time = 0.0F;
  int individuals_per_unit = 1;
};

auto civilian_item(Game::Systems::NationID nation_id) -> UnitItem {
  const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      nation_id, Game::Units::TroopType::Civilian);
  UnitItem entry;
  entry.item.key = QString::fromStdString(
      Game::Units::troop_typeToString(Game::Units::TroopType::Civilian));
  entry.item.display_name =
      Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
  entry.item.costs = profile.production.resource_costs;
  entry.manpower_cost = profile.production.cost;
  entry.army_cap_weight = profile.production.population_cost();
  entry.build_time = profile.production.build_time;
  entry.individuals_per_unit = profile.individuals_per_unit;
  return entry;
}

auto unit_items(const Game::Systems::NationRegistry* nations,
                Game::Systems::NationID nation_id) -> std::vector<UnitItem> {
  std::vector<UnitItem> items;
  if (nations == nullptr) {
    return items;
  }
  const auto* nation = nations->get_nation(nation_id);
  if (nation == nullptr) {
    return items;
  }
  auto& profiles = Game::Systems::TroopProfileService::instance();
  for (const auto& troop : nation->available_troops) {
    if (!is_barracks_recruit(troop.unit_type)) {
      continue;
    }
    const auto profile = profiles.get_profile(nation_id, troop.unit_type);
    UnitItem entry;
    entry.item.key =
        QString::fromStdString(Game::Units::troop_typeToString(troop.unit_type));
    entry.item.display_name =
        Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
    entry.item.costs = profile.production.resource_costs;
    entry.manpower_cost = profile.production.cost;
    entry.army_cap_weight = profile.production.population_cost();
    entry.build_time = profile.production.build_time;
    entry.individuals_per_unit = profile.individuals_per_unit;
    items.push_back(std::move(entry));
  }
  return items;
}

void note_shortfall(const CostedItem& item,
                    const ResourceAmounts& available,
                    std::array<int, Game::Systems::k_resource_type_count>& shortfall,
                    std::array<QString, Game::Systems::k_resource_type_count>& source) {
  for (const ResourceType type : Game::Systems::k_all_resource_types) {
    const int missing = item.costs.get(type) - available.get(type);
    if (missing <= 0) {
      continue;
    }
    const auto index = Game::Systems::resource_type_index(type);
    if (shortfall[index] == 0 || missing < shortfall[index]) {
      shortfall[index] = missing;
      source[index] = item.key;
    }
  }
}

} // namespace

auto build_resource_overview(const EconomyOverviewRequest& request) -> QVariantList {
  if (request.resources == nullptr) {
    return {};
  }
  auto& registry = *request.resources;
  const ResourceAmounts available = registry.get_all(request.owner_id);
  const ResourceAmounts harvested = registry.get_harvested_all(request.owner_id);
  const OwnerScan scan = scan_owner(request.world, request.owner_id);

  std::array<QStringList, Game::Systems::k_resource_type_count> used_by{};
  std::array<int, Game::Systems::k_resource_type_count> shortfall{};
  std::array<QString, Game::Systems::k_resource_type_count> shortfall_item{};

  for (const auto& item : building_items()) {
    for (const ResourceType type : Game::Systems::k_all_resource_types) {
      if (item.costs.get(type) > 0) {
        used_by[Game::Systems::resource_type_index(type)].push_back(item.key);
      }
    }
    note_shortfall(item, available, shortfall, shortfall_item);
  }
  auto units = unit_items(request.nations, request.nation_id);
  units.push_back(civilian_item(request.nation_id));
  for (const auto& unit : units) {
    for (const ResourceType type : Game::Systems::k_all_resource_types) {
      if (unit.item.costs.get(type) > 0) {
        used_by[Game::Systems::resource_type_index(type)].push_back(unit.item.key);
      }
    }
    note_shortfall(unit.item, available, shortfall, shortfall_item);
  }

  QVariantList entries;
  for (const ResourceType type : Game::Systems::k_all_resource_types) {
    const auto index = Game::Systems::resource_type_index(type);
    const bool gatherable = Game::Systems::is_gatherable_resource(type);
    const bool tradeable = type != ResourceType::Gold;
    const int amount = available.get(type);

    QVariantMap entry;
    entry["key"] = QLatin1String(Game::Systems::resource_type_key(type));
    entry["amount"] = amount;
    entry["harvested"] = harvested.get(type);
    entry["gatherable"] = gatherable;
    entry["yield_per_trip"] = Game::Systems::harvest_yield(type);
    entry["gathering_workers"] = scan.gathering_workers[index];
    entry["carrying"] = scan.carrying.get(type);
    entry["display_cap"] = gatherable ? Game::Systems::stockpile_display_cap(type) : 0;
    entry["tradeable"] = tradeable;
    entry["used_by"] = used_by[index];
    entry["shortfall"] = shortfall[index];
    entry["shortfall_item"] = shortfall_item[index];
    entry["objective_target"] = request.objective_resources.get(type);
    entry["relevant"] = amount > 0 || gatherable || !used_by[index].isEmpty() ||
                        request.objective_resources.get(type) > 0 ||
                        (tradeable && scan.marketplace_count > 0);
    entries.push_back(entry);
  }
  return entries;
}

auto build_production_help(const EconomyOverviewRequest& request) -> QVariantMap {
  if (request.resources == nullptr) {
    return {};
  }
  auto& registry = *request.resources;
  const ResourceAmounts available = registry.get_all(request.owner_id);
  const OwnerScan scan = scan_owner(request.world, request.owner_id);
  const int troop_count = troop_manpower_of(request);

  QVariantList buildings;
  for (const auto& item : building_items()) {
    QVariantMap entry;
    entry["item_type"] = item.key;
    entry["resource_costs"] = costs_to_map(item.costs);
    entry["build_time"] = static_cast<double>(
        Game::Systems::construction_build_time(item.key.toStdString()));
    const QVariantMap missing = missing_to_map(item.costs, available);
    entry["missing"] = missing;
    entry["affordable"] = missing.isEmpty();
    entry["prerequisite"] = QStringLiteral("builder");
    entry["prerequisite_met"] = scan.builder_count > 0;
    buildings.push_back(entry);
  }

  QVariantList units;
  for (const auto& unit : unit_items(request.nations, request.nation_id)) {
    QVariantMap entry;
    entry["unit_type"] = unit.item.key;
    entry["display_name"] = unit.item.display_name;
    entry["cost"] = unit.manpower_cost;
    entry["resource_costs"] = costs_to_map(unit.item.costs);
    entry["build_time"] = static_cast<double>(unit.build_time);
    entry["individuals_per_unit"] = unit.individuals_per_unit;
    const QVariantMap missing = missing_to_map(unit.item.costs, available);
    entry["missing"] = missing;
    entry["affordable"] = missing.isEmpty();
    entry["prerequisite"] = QStringLiteral("barracks");
    entry["prerequisite_met"] = scan.barracks_count > 0;
    entry["reserve_met"] = scan.barracks_manpower >= unit.manpower_cost;
    entry["manpower_met"] = request.manpower_cap <= 0 ||
                            troop_count + unit.army_cap_weight <= request.manpower_cap;
    units.push_back(entry);
  }

  {
    const UnitItem civilian = civilian_item(request.nation_id);
    QVariantMap entry;
    entry["unit_type"] = civilian.item.key;
    entry["display_name"] = civilian.item.display_name;
    entry["cost"] = civilian.manpower_cost;
    entry["resource_costs"] = costs_to_map(civilian.item.costs);
    entry["build_time"] = static_cast<double>(civilian.build_time);
    entry["individuals_per_unit"] = civilian.individuals_per_unit;
    const QVariantMap missing = missing_to_map(civilian.item.costs, available);
    entry["missing"] = missing;
    entry["affordable"] = missing.isEmpty();
    entry["prerequisite"] = QStringLiteral("home");
    entry["prerequisite_met"] = scan.home_count > 0;
    entry["reserve_met"] = true;
    entry["manpower_met"] = true;
    units.push_back(entry);
  }

  QVariantMap harvest_yields;
  for (const ResourceType type : Game::Systems::k_all_resource_types) {
    if (Game::Systems::is_gatherable_resource(type)) {
      harvest_yields[QLatin1String(Game::Systems::resource_type_key(type))] =
          Game::Systems::harvest_yield(type);
    }
  }

  QVariantMap help;
  help["buildings"] = buildings;
  help["units"] = units;
  help["harvest_yields"] = harvest_yields;
  help["builder_count"] = scan.builder_count;
  help["idle_builder_count"] = scan.idle_builder_count;
  help["constructing_builder_count"] = scan.constructing_builder_count;
  help["barracks_count"] = scan.barracks_count;
  help["home_count"] = scan.home_count;
  help["marketplace_count"] = scan.marketplace_count;
  help["temple_count"] = scan.temple_count;
  help["farm_count"] = scan.farm_count;
  help["ripe_farm_count"] = scan.ripe_farm_count;
  help["farm_cycle_seconds"] =
      static_cast<double>(Game::Systems::k_farm_growth_cycle_seconds);
  help["sheep_yield"] = Game::Systems::k_slaughter_sheep_food_reward;
  help["civilian_food_cost"] =
      civilian_item(request.nation_id).item.costs.get(ResourceType::Food);
  help["barracks_manpower"] = scan.barracks_manpower;
  help["manpower"] = troop_count;
  help["manpower_cap"] = request.manpower_cap;
  help["home_reserve_bonus"] = k_home_reserve_bonus;
  help["civilian_delivery_grant"] = Game::Systems::k_civilian_delivery_reserve_grant;
  return help;
}

auto capture_economy_coach_baseline(const EconomyOverviewRequest& request)
    -> EconomyCoachBaseline {
  const OwnerScan scan = scan_owner(request.world, request.owner_id);
  return {.troop_manpower = troop_manpower_of(request),
          .building_count = scan.building_count,
          .captured = true};
}

auto build_economy_coach_state(const EconomyOverviewRequest& request,
                               const EconomyCoachBaseline& baseline) -> QVariantMap {
  const OwnerScan scan = scan_owner(request.world, request.owner_id);
  const int troop_manpower = troop_manpower_of(request);
  const ResourceAmounts harvested =
      request.resources != nullptr
          ? request.resources->get_harvested_all(request.owner_id)
          : ResourceAmounts{};

  const bool gather_done = !harvested.empty();
  const bool build_done = scan.building_count > baseline.building_count ||
                          scan.constructing_builder_count > 0;
  const bool recruit_done =
      troop_manpower > baseline.troop_manpower || scan.barracks_producing;
  const bool army_done =
      troop_manpower >= baseline.troop_manpower + k_economy_coach_army_manpower;

  const std::array<std::pair<const char*, bool>, 4> steps = {{
      {"gather", gather_done},
      {"build", build_done},
      {"recruit", recruit_done},
      {"army", army_done},
  }};

  QVariantList step_list;
  QString current = QStringLiteral("done");
  int current_index = static_cast<int>(steps.size());
  for (std::size_t index = 0; index < steps.size(); ++index) {
    QVariantMap step;
    step["id"] = QLatin1String(steps[index].first);
    step["done"] = steps[index].second;
    step_list.push_back(step);
    if (!steps[index].second && current_index == static_cast<int>(steps.size())) {
      current = QLatin1String(steps[index].first);
      current_index = static_cast<int>(index);
    }
  }

  QVariantMap state;
  state["step"] = current;
  state["step_index"] = current_index;
  state["step_count"] = static_cast<int>(steps.size());
  state["steps"] = step_list;
  state["complete"] = current_index >= static_cast<int>(steps.size());
  state["builder_count"] = scan.builder_count;
  state["idle_builder_count"] = scan.idle_builder_count;
  state["barracks_count"] = scan.barracks_count;
  state["home_count"] = scan.home_count;
  state["troop_manpower"] = troop_manpower;
  state["population_raised"] = std::max(0, troop_manpower - baseline.troop_manpower);
  state["manpower_target"] = k_economy_coach_army_manpower;
  state["manpower_cap"] = request.manpower_cap;
  state["gathering_workers"] =
      std::accumulate(scan.gathering_workers.begin(), scan.gathering_workers.end(), 0);
  return state;
}

} // namespace App::Core
