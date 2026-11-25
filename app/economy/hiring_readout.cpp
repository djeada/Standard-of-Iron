#include "app/economy/hiring_readout.h"

#include <QVariantList>

#include "app/economy/resource_text.h"
#include "game/core/component_core.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/economy/nation_economy_system.h"
#include "game/systems/economy/nation_recruitment.h"
#include "game/systems/nation_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

namespace App::Economy {

namespace {

using Game::Util::k_units_context;
using Game::Util::tr_asset;

auto offer(const QString& key,
           const QString& name,
           Game::Units::TroopType troop,
           Game::Systems::NationID troop_nation,
           const Game::Systems::RecruitQuote& quote,
           int upkeep,
           int have_gold) -> QVariantMap {
  QVariantMap entry;
  entry["key"] = key;
  entry["unit_type"] = QString::fromStdString(Game::Units::troop_typeToString(troop));
  entry["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(troop_nation));
  entry["display_name"] = name;
  entry["resource_costs"] = to_variant_map(quote.resources);
  entry["build_time"] = static_cast<double>(quote.build_time);
  entry["men"] = quote.individuals;
  entry["upkeep_per_minute"] = upkeep;
  entry["affordable"] =
      have_gold >= quote.resources.get(Game::Systems::ResourceType::Gold);
  return entry;
}

} // namespace

auto selected_hiring_state(Engine::Core::World* world,
                           int local_owner_id) -> QVariantMap {
  QVariantMap m;
  m["has_hiring"] = false;
  if (world == nullptr) {
    return m;
  }
  const auto& selected =
      Game::Session::session_for(*world).selection().get_selected_units();
  const Engine::Core::UnitComponent* building = nullptr;
  Engine::Core::EntityID building_id = 0;
  for (const auto id : selected) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(id);
    if (unit != nullptr && unit->owner_id == local_owner_id &&
        Game::Units::is_hiring_building(unit->spawn_type)) {
      building = unit;
      building_id = id;
      break;
    }
  }
  const auto* nation = Game::Systems::owner_nation(*world, local_owner_id);
  if (building == nullptr || nation == nullptr) {
    return m;
  }
  const int gold = Game::Session::session_for(*world).economy().get(
      local_owner_id, Game::Systems::ResourceType::Gold);

  m["has_hiring"] = true;
  m["building_type"] =
      QString::fromStdString(Game::Units::spawn_typeToString(building->spawn_type));
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nation_id_to_string(nation->id));
  m["upkeep_per_minute"] =
      Game::Systems::NationEconomySystem::upkeep_per_minute(*world, local_owner_id);
  m["income_per_minute"] = Game::Systems::NationEconomySystem::trade_income_per_minute(
      *world, local_owner_id);
  if (const auto* production =
          world->try_get<Engine::Core::ProductionComponent>(building_id)) {
    m["in_progress"] = production->in_progress;
    m["time_remaining"] = static_cast<double>(production->time_remaining);
    m["build_time"] = static_cast<double>(production->build_time);
    m["product_type"] = QString::fromStdString(
        Game::Units::troop_typeToString(production->product_type));
  }

  QVariantList offers;
  if (building->spawn_type == Game::Units::SpawnType::MercenaryCamp) {
    for (const auto& band : nation->mercenary_bands) {
      const auto quote = Game::Systems::quote_recruit(
          *world, local_owner_id, band.unit_type, band.nation);
      offers.append(offer(
          QString::fromStdString(Game::Systems::nation_id_to_string(band.nation) + "/" +
                                 Game::Units::troop_typeToString(band.unit_type)),
          tr_asset(k_units_context, band.display_name),
          band.unit_type,
          band.nation,
          quote,
          band.upkeep_per_minute,
          gold));
    }
  } else {
    for (const auto& troop : nation->available_troops) {
      if (!troop.recruitable ||
          nation->recruiting_building(troop.unit_type) != building->spawn_type) {
        continue;
      }
      const auto quote =
          Game::Systems::quote_recruit(*world, local_owner_id, troop.unit_type);
      offers.append(offer(
          QString::fromStdString(Game::Units::troop_typeToString(troop.unit_type)),
          tr_asset(k_units_context, troop.display_name),
          troop.unit_type,
          nation->id,
          quote,
          0,
          gold));
    }
  }
  m["offers"] = offers;
  return m;
}

} // namespace App::Economy
