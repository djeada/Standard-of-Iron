#include "app/economy/production_readouts.h"

#include <algorithm>

#include "app/economy/resource_text.h"
#include "app/economy/unit_profile.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/food_targets.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

namespace App::Economy {

auto selected_barracks_state(Engine::Core::World* world,
                             int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_barracks"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  m["manpower_available"] = 0;

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_barracks_state(
      *world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_barracks"] = st.has_barracks;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["manpower_available"] = st.manpower_available;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto& unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto selected_builder_state(Engine::Core::World* world) -> QVariantMap {
  QVariantMap m;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 10.0;
  m["product_type"] = "";

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  const auto& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* builder_prod = e->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder_prod != nullptr) {
      m["in_progress"] =
          builder_prod->in_progress || builder_prod->has_construction_site;
      m["time_remaining"] = builder_prod->time_remaining;
      m["build_time"] = builder_prod->build_time;
      m["product_type"] = QString::fromStdString(builder_prod->product_type);
      return m;
    }
  }

  return m;
}

auto selected_farm_state(Engine::Core::World* world,
                         int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_farm"] = false;
  m["nation_id"] = "";
  m["growth"] = 0.0;
  m["ripe"] = false;
  m["seconds_to_ripe"] = 0.0;
  m["cycle_seconds"] = static_cast<double>(Game::Systems::k_farm_growth_cycle_seconds);
  m["yield"] = Game::Systems::k_harvest_grain_food_reward;
  m["harvests"] = 0;
  m["claimed"] = false;

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  for (auto id : selection_system->get_selected_units()) {
    auto* e = world->get_entity(id);
    if (e == nullptr) {
      continue;
    }
    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    auto* farm = e->get_component<Engine::Core::FarmComponent>();
    if (unit == nullptr || farm == nullptr ||
        unit->spawn_type != Game::Units::SpawnType::Farm ||
        unit->owner_id != local_owner_id) {
      continue;
    }

    m["has_farm"] = true;
    m["nation_id"] =
        QString::fromStdString(Game::Systems::nation_id_to_string(unit->nation_id));
    m["growth"] = static_cast<double>(std::clamp(farm->growth, 0.0F, 1.0F));
    m["ripe"] = farm->ripe();
    m["seconds_to_ripe"] = static_cast<double>(
        std::max(0.0F, (1.0F - farm->growth) * farm->cycle_seconds));
    m["cycle_seconds"] = static_cast<double>(farm->cycle_seconds);
    m["harvests"] = farm->harvests;
    m["claimed"] = Game::Systems::food_target_claimed(*world, id);
    return m;
  }
  return m;
}

auto selected_marketplace_state(Engine::Core::World* world,
                                int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_marketplace"] = false;
  m["nation_id"] = "";
  m["trade_quantity"] = 0;
  m["buy_prices"] = QVariantMap{};
  m["sell_prices"] = QVariantMap{};

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  auto const& selected = selection_system->get_selected_units();
  for (auto id : selected) {
    auto* e = world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Marketplace ||
        unit->owner_id != local_owner_id) {
      continue;
    }

    auto const& rates = Game::Session::session_for(*world).marketplace().get_rates();
    QVariantMap buy_prices;
    buy_prices["food"] = rates.buy_price_food;
    buy_prices["wood"] = rates.buy_price_wood;
    buy_prices["stone"] = rates.buy_price_stone;
    buy_prices["iron"] = rates.buy_price_iron;

    QVariantMap sell_prices;
    sell_prices["food"] = rates.sell_price_food;
    sell_prices["wood"] = rates.sell_price_wood;
    sell_prices["stone"] = rates.sell_price_stone;
    sell_prices["iron"] = rates.sell_price_iron;

    m["has_marketplace"] = true;
    m["nation_id"] =
        QString::fromStdString(Game::Systems::nation_id_to_string(unit->nation_id));
    m["trade_quantity"] = rates.trade_quantity;
    m["buy_prices"] = buy_prices;
    m["sell_prices"] = sell_prices;
    return m;
  }

  return m;
}

auto selected_home_state(Engine::Core::World* world,
                         int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_home"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  m["manpower_available"] = 0;
  m["queue_size"] = 0;
  m["product_type"] = "";
  m["production_queue"] = QVariantList{};
  m["nation_id"] = "";

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_home_state(
      *world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_home"] = st.has_home;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["manpower_available"] = st.manpower_available;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto& unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto selected_temple_state(Engine::Core::World* world,
                           int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_temple"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  m["manpower_available"] = 0;
  m["queue_size"] = 0;
  m["product_type"] = "";
  m["production_queue"] = QVariantList{};
  m["nation_id"] = "";

  if (world == nullptr) {
    return m;
  }

  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }

  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::get_selected_temple_state(
      *world, selection_system->get_selected_units(), local_owner_id, st);

  m["has_temple"] = st.has_temple;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["manpower_available"] = st.manpower_available;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(st.nation_id));

  QVariantList queue_list;
  for (const auto& unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto unit_production_info(const Game::Systems::NationRegistry& nations,
                          const QString& unit_type,
                          const QString& nation_id) -> QVariantMap {
  QVariantMap info = unit_profile(nations, unit_type, nation_id);
  if (info.value("valid").toBool()) {
    return info;
  }

  const auto& config = Game::Units::TroopConfig::instance();
  const std::string type_str = unit_type.toStdString();
  info["cost"] = config.get_production_cost(type_str);
  info["population_cost"] = info["cost"];
  info["resource_costs"] = QVariantMap{};
  info["build_time"] = static_cast<double>(config.get_build_time(type_str));
  info["individuals_per_unit"] = config.get_individuals_per_unit(type_str);
  return info;
}

auto construction_info(const QString& item_type) -> QVariantMap {
  QVariantMap info;
  std::string const item_str = item_type.toStdString();
  info["build_time"] =
      static_cast<double>(Game::Systems::construction_build_time(item_str));
  info["resource_costs"] = to_variant_map(construction_costs(item_type));
  info["display_name"] = item_type;
  return info;
}

} // namespace App::Economy
