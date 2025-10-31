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

auto Nation::getMeleeTroops() const -> std::vector<const TroopType *> {
  std::vector<const TroopType *> result;
  for (const auto &troop : availableTroops) {
    if (troop.isMelee) {
      result.push_back(&troop);
    }
  }
  return result;
}

auto Nation::getRangedTroops() const -> std::vector<const TroopType *> {
  std::vector<const TroopType *> result;
  for (const auto &troop : availableTroops) {
    if (!troop.isMelee) {
      result.push_back(&troop);
    }
  }
  return result;
}

auto Nation::getTroop(Game::Units::TroopType unit_type) const
    -> const TroopType * {
  for (const auto &troop : availableTroops) {
    if (troop.unit_type == unit_type) {
      return &troop;
    }
  }
  return nullptr;
}

auto Nation::getBestMeleeTroop() const -> const TroopType * {
  auto melee = getMeleeTroops();
  if (melee.empty()) {
    return nullptr;
  }

  auto it = std::max_element(melee.begin(), melee.end(),
                             [](const TroopType *a, const TroopType *b) {
                               return a->priority < b->priority;
                             });

  return *it;
}

auto Nation::getBestRangedTroop() const -> const TroopType * {
  auto ranged = getRangedTroops();
  if (ranged.empty()) {
    return nullptr;
  }

  auto it = std::max_element(ranged.begin(), ranged.end(),
                             [](const TroopType *a, const TroopType *b) {
                               return a->priority < b->priority;
                             });

  return *it;
}

auto Nation::isMeleeUnit(Game::Units::TroopType unit_type) const -> bool {
  const auto *troop = getTroop(unit_type);
  return troop != nullptr && troop->isMelee;
}

auto Nation::is_ranged_unit(Game::Units::TroopType unit_type) const -> bool {
  const auto *troop = getTroop(unit_type);
  return troop != nullptr && !troop->isMelee;
}

auto NationRegistry::instance() -> NationRegistry & {
  static NationRegistry inst;
  return inst;
}

void NationRegistry::registerNation(Nation nation) {

  auto it = m_nationIndex.find(nation.id);
  if (it != m_nationIndex.end()) {

    m_nations[it->second] = std::move(nation);
    return;
  }

  size_t const index = m_nations.size();
  m_nations.push_back(std::move(nation));
  m_nationIndex[m_nations.back().id] = index;
}

auto NationRegistry::getNation(const std::string &nationId) const
    -> const Nation * {
  auto it = m_nationIndex.find(nationId);
  if (it == m_nationIndex.end()) {
    return nullptr;
  }
  return &m_nations[it->second];
}

auto NationRegistry::getNationForPlayer(int player_id) const -> const Nation * {

  auto it = m_playerNations.find(player_id);
  if (it != m_playerNations.end()) {
    const auto *nation = getNation(it->second);
    return nation;
  }

  const auto *nation = getNation(m_defaultNation);
  if (nation == nullptr) {
  }
  return nation;
}

void NationRegistry::setPlayerNation(int player_id,
                                     const std::string &nationId) {
  m_playerNations[player_id] = nationId;
}

void NationRegistry::initializeDefaults() {
  if (m_initialized) {
    return;
  }

  clear();
  Game::Units::TroopCatalogLoader::load_default_catalog();

  auto nations = NationLoader::load_default_nations();
  if (nations.empty()) {
    Nation kingdom_of_iron;
    kingdom_of_iron.id = "kingdom_of_iron";
    kingdom_of_iron.displayName = "Kingdom of Iron";
    kingdom_of_iron.primaryBuilding = "barracks";
    kingdom_of_iron.formation_type = FormationType::Roman;

    auto appendTroop = [&kingdom_of_iron](Game::Units::TroopType type) {
      TroopType troop_entry;
      troop_entry.unit_type = type;

      const auto &troop_class =
          Game::Units::TroopCatalog::instance().get_class_or_fallback(type);
      troop_entry.displayName = troop_class.display_name;
      troop_entry.isMelee = troop_class.production.is_melee;
      troop_entry.cost = troop_class.production.cost;
      troop_entry.buildTime = troop_class.production.build_time;
      troop_entry.priority = troop_class.production.priority;

      kingdom_of_iron.availableTroops.push_back(std::move(troop_entry));
    };

    appendTroop(Game::Units::TroopType::Archer);
    appendTroop(Game::Units::TroopType::Swordsman);
    appendTroop(Game::Units::TroopType::Spearman);
    appendTroop(Game::Units::TroopType::MountedKnight);

    registerNation(std::move(kingdom_of_iron));
    m_defaultNation = "kingdom_of_iron";
  } else {
    const std::string desired_default = "kingdom_of_iron";
    std::string fallback_default = nations.front().id;
    for (auto &nation : nations) {
      if (nation.id == desired_default) {
        fallback_default = nation.id;
      }
      registerNation(std::move(nation));
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

void NationRegistry::clearPlayerAssignments() { m_playerNations.clear(); }

} // namespace Game::Systems
