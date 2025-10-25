#include "nation_registry.h"
#include <QDebug>
#include <algorithm>

namespace Game::Systems {

std::vector<const TroopType *> Nation::getMeleeTroops() const {
  std::vector<const TroopType *> result;
  for (const auto &troop : availableTroops) {
    if (troop.isMelee) {
      result.push_back(&troop);
    }
  }
  return result;
}

std::vector<const TroopType *> Nation::getRangedTroops() const {
  std::vector<const TroopType *> result;
  for (const auto &troop : availableTroops) {
    if (!troop.isMelee) {
      result.push_back(&troop);
    }
  }
  return result;
}

const TroopType *Nation::getTroop(Game::Units::TroopType unitType) const {
  for (const auto &troop : availableTroops) {
    if (troop.unitType == unitType) {
      return &troop;
    }
  }
  return nullptr;
}

const TroopType *Nation::getBestMeleeTroop() const {
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

const TroopType *Nation::getBestRangedTroop() const {
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

bool Nation::isMeleeUnit(Game::Units::TroopType unitType) const {
  const auto *troop = getTroop(unitType);
  return troop != nullptr && troop->isMelee;
}

bool Nation::isRangedUnit(Game::Units::TroopType unitType) const {
  const auto *troop = getTroop(unitType);
  return troop != nullptr && !troop->isMelee;
}

NationRegistry &NationRegistry::instance() {
  static NationRegistry inst;
  return inst;
}

void NationRegistry::registerNation(Nation nation) {

  auto it = m_nationIndex.find(nation.id);
  if (it != m_nationIndex.end()) {

    m_nations[it->second] = std::move(nation);
    return;
  }

  size_t index = m_nations.size();
  m_nations.push_back(std::move(nation));
  m_nationIndex[m_nations.back().id] = index;
}

const Nation *NationRegistry::getNation(const std::string &nationId) const {
  auto it = m_nationIndex.find(nationId);
  if (it == m_nationIndex.end()) {
    return nullptr;
  }
  return &m_nations[it->second];
}

const Nation *NationRegistry::getNationForPlayer(int playerId) const {

  auto it = m_playerNations.find(playerId);
  if (it != m_playerNations.end()) {
    auto *nation = getNation(it->second);
    return nation;
  }

  auto *nation = getNation(m_defaultNation);
  if (!nation) {
  }
  return nation;
}

void NationRegistry::setPlayerNation(int playerId,
                                     const std::string &nationId) {
  m_playerNations[playerId] = nationId;
}

void NationRegistry::initializeDefaults() {
  clear();

  Nation kingdomOfIron;
  kingdomOfIron.id = "kingdom_of_iron";
  kingdomOfIron.displayName = "Kingdom of Iron";
  kingdomOfIron.primaryBuilding = "barracks";
  kingdomOfIron.formationType = FormationType::Roman;

  TroopType archer;
  archer.unitType = Game::Units::TroopType::Archer;
  archer.displayName = "Archer";
  archer.isMelee = false;
  archer.cost = 50;
  archer.buildTime = 5.0f;
  archer.priority = 10;
  kingdomOfIron.availableTroops.push_back(archer);

  TroopType knight;
  knight.unitType = Game::Units::TroopType::Knight;
  knight.displayName = "Knight";
  knight.isMelee = true;
  knight.cost = 100;
  knight.buildTime = 8.0f;
  knight.priority = 10;
  kingdomOfIron.availableTroops.push_back(knight);

  TroopType spearman;
  spearman.unitType = Game::Units::TroopType::Spearman;
  spearman.displayName = "Spearman";
  spearman.isMelee = true;
  spearman.cost = 75;
  spearman.buildTime = 6.0f;
  spearman.priority = 5;
  kingdomOfIron.availableTroops.push_back(spearman);

  TroopType mountedKnight;
  mountedKnight.unitType = Game::Units::TroopType::MountedKnight;
  mountedKnight.displayName = "Mounted Knight";
  mountedKnight.isMelee = true;
  mountedKnight.cost = 150;
  mountedKnight.buildTime = 10.0f;
  mountedKnight.priority = 15;
  kingdomOfIron.availableTroops.push_back(mountedKnight);

  registerNation(std::move(kingdomOfIron));

  m_defaultNation = "kingdom_of_iron";
}

void NationRegistry::clear() {
  m_nations.clear();
  m_nationIndex.clear();
  m_playerNations.clear();
}

} // namespace Game::Systems
