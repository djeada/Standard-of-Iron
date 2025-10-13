#pragma once

#include "formation_system.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

struct TroopType {
  std::string unitType;
  std::string displayName;
  bool isMelee = false;
  int cost = 100;
  float buildTime = 5.0f;
  int priority = 0;
};

struct Nation {
  std::string id;
  std::string displayName;
  std::vector<TroopType> availableTroops;
  std::string primaryBuilding = "barracks";
  FormationType formationType = FormationType::Roman;

  std::vector<const TroopType *> getMeleeTroops() const;

  std::vector<const TroopType *> getRangedTroops() const;

  const TroopType *getTroop(const std::string &unitType) const;

  const TroopType *getBestMeleeTroop() const;
  const TroopType *getBestRangedTroop() const;
};

class NationRegistry {
public:
  static NationRegistry &instance();

  void registerNation(Nation nation);

  const Nation *getNation(const std::string &nationId) const;

  const Nation *getNationForPlayer(int playerId) const;

  void setPlayerNation(int playerId, const std::string &nationId);

  const std::vector<Nation> &getAllNations() const { return m_nations; }

  void initializeDefaults();

  void clear();

private:
  NationRegistry() = default;

  std::vector<Nation> m_nations;
  std::unordered_map<std::string, size_t> m_nationIndex;
  std::unordered_map<int, std::string> m_playerNations;
  std::string m_defaultNation = "kingdom_of_iron";
};

} // namespace Game::Systems
