#include "nation_registry.h"
#include "systems/formation_system.h"
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
  clear();

  Nation kingdom_of_iron;
  kingdom_of_iron.id = "kingdom_of_iron";
  kingdom_of_iron.displayName = "Kingdom of Iron";
  kingdom_of_iron.primaryBuilding = "barracks";
  kingdom_of_iron.formation_type = FormationType::Roman;

  TroopType archer;
  archer.unit_type = Game::Units::TroopType::Archer;
  archer.displayName = "Archer";
  archer.isMelee = false;
  archer.cost = 50;
  archer.buildTime = 5.0F;
  archer.priority = 10;
  kingdom_of_iron.availableTroops.push_back(archer);

  TroopType knight;
  knight.unit_type = Game::Units::TroopType::Knight;
  knight.displayName = "Knight";
  knight.isMelee = true;
  knight.cost = 100;
  knight.buildTime = 8.0F;
  knight.priority = 10;
  kingdom_of_iron.availableTroops.push_back(knight);

  TroopType spearman;
  spearman.unit_type = Game::Units::TroopType::Spearman;
  spearman.displayName = "Spearman";
  spearman.isMelee = true;
  spearman.cost = 75;
  spearman.buildTime = 6.0F;
  spearman.priority = 5;
  kingdom_of_iron.availableTroops.push_back(spearman);

  TroopType mounted_knight;
  mounted_knight.unit_type = Game::Units::TroopType::MountedKnight;
  mounted_knight.displayName = "Mounted Knight";
  mounted_knight.isMelee = true;
  mounted_knight.cost = 150;
  mounted_knight.buildTime = 10.0F;
  mounted_knight.priority = 15;
  kingdom_of_iron.availableTroops.push_back(mounted_knight);

  registerNation(std::move(kingdom_of_iron));

  m_defaultNation = "kingdom_of_iron";
}

void NationRegistry::clear() {
  m_nations.clear();
  m_nationIndex.clear();
  m_playerNations.clear();
}

} // namespace Game::Systems
