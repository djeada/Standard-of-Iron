#include "nation_registry.h"
#include "systems/formation_system.h"
#include "systems/nation_loader.h"
#include "systems/troop_profile_service.h"
#include "units/troop_catalog.h"
#include "units/troop_catalog_loader.h"
#include "units/troop_type.h"
#include <QDebug>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Game::Systems {

auto Nation::get_melee_troops() const -> std::vector<const TroopType *> {
  std::vector<const TroopType *> result;
  for (const auto &troop : available_troops) {
    if (troop.is_melee) {
      result.push_back(&troop);
    }
  }
  return result;
}

auto Nation::get_ranged_troops() const -> std::vector<const TroopType *> {
  std::vector<const TroopType *> result;
  for (const auto &troop : available_troops) {
    if (!troop.is_melee) {
      result.push_back(&troop);
    }
  }
  return result;
}

auto Nation::get_troop(Game::Units::TroopType unit_type) const
    -> const TroopType * {
  for (const auto &troop : available_troops) {
    if (troop.unit_type == unit_type) {
      return &troop;
    }
  }
  return nullptr;
}

auto Nation::get_best_melee_troop() const -> const TroopType * {
  auto melee = get_melee_troops();
  if (melee.empty()) {
    return nullptr;
  }

  auto it = std::max_element(melee.begin(), melee.end(),
                             [](const TroopType *a, const TroopType *b) {
                               return a->priority < b->priority;
                             });

  return *it;
}

auto Nation::get_best_ranged_troop() const -> const TroopType * {
  auto ranged = get_ranged_troops();
  if (ranged.empty()) {
    return nullptr;
  }

  auto it = std::max_element(ranged.begin(), ranged.end(),
                             [](const TroopType *a, const TroopType *b) {
                               return a->priority < b->priority;
                             });

  return *it;
}

auto Nation::is_melee_unit(Game::Units::TroopType unit_type) const -> bool {
  const auto *troop = get_troop(unit_type);
  return troop != nullptr && troop->is_melee;
}

auto Nation::is_ranged_unit(Game::Units::TroopType unit_type) const -> bool {
  const auto *troop = get_troop(unit_type);
  return troop != nullptr && !troop->is_melee;
}

auto NationRegistry::instance() -> NationRegistry & {
  static NationRegistry inst;
  return inst;
}

void NationRegistry::register_nation(Nation nation) {

  auto it = m_nationIndex.find(nation.id);
  if (it != m_nationIndex.end()) {

    m_nations[it->second] = std::move(nation);
    return;
  }

  size_t const index = m_nations.size();
  m_nations.push_back(std::move(nation));
  m_nationIndex[m_nations.back().id] = index;
}

auto NationRegistry::get_nation(NationID nationId) const -> const Nation * {
  auto it = m_nationIndex.find(nationId);
  if (it == m_nationIndex.end()) {
    return nullptr;
  }
  return &m_nations[it->second];
}

auto NationRegistry::get_nation_for_player(int player_id) const
    -> const Nation * {

  auto it = m_playerNations.find(player_id);
  if (it != m_playerNations.end()) {
    const auto *nation = get_nation(it->second);
    return nation;
  }

  const auto *nation = get_nation(m_defaultNation);
  if (nation == nullptr) {
  }
  return nation;
}

void NationRegistry::set_player_nation(int player_id, NationID nationId) {
  m_playerNations[player_id] = nationId;
}

void NationRegistry::initialize_defaults() {
  if (m_initialized) {
    return;
  }

  clear();
  Game::Units::TroopCatalogLoader::load_default_catalog();

  auto nations = NationLoader::load_default_nations();
  if (nations.empty()) {
    Nation roman;
    roman.id = NationID::RomanRepublic;
    roman.display_name = "Roman Republic";
    roman.primary_building = Game::Units::BuildingType::Barracks;
    roman.formation_type = FormationType::Roman;

    auto appendTroop = [&roman](Game::Units::TroopType type) {
      TroopType troop_entry;
      troop_entry.unit_type = type;

      const auto &troop_class =
          Game::Units::TroopCatalog::instance().get_class_or_fallback(type);
      troop_entry.display_name = troop_class.display_name;
      troop_entry.is_melee = troop_class.production.is_melee;
      troop_entry.cost = troop_class.production.cost;
      troop_entry.build_time = troop_class.production.build_time;
      troop_entry.priority = troop_class.production.priority;

      roman.available_troops.push_back(std::move(troop_entry));
    };

    appendTroop(Game::Units::TroopType::Archer);
    appendTroop(Game::Units::TroopType::Swordsman);
    appendTroop(Game::Units::TroopType::Spearman);
    appendTroop(Game::Units::TroopType::MountedKnight);

    register_nation(std::move(roman));
    m_defaultNation = NationID::RomanRepublic;
  } else {
    NationID fallback_default = nations.front().id;
    for (auto &nation : nations) {
      register_nation(std::move(nation));
    }
    m_defaultNation = fallback_default;
  }

  TroopProfileService::instance().clear();
  m_initialized = true;
}

void NationRegistry::clear() {
  m_nations.clear();
  m_nationIndex.clear();
  m_playerNations.clear();
  m_initialized = false;
}

void NationRegistry::clear_player_assignments() { m_playerNations.clear(); }

} 
