#pragma once

#include "../units/troop_type.h"
#include "formation_system.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

struct TroopType {
  Game::Units::TroopType unit_type;
  std::string displayName;
  bool isMelee = false;
  int cost = 100;
  float buildTime = 5.0F;
  int priority = 0;
};

struct Nation {
  std::string id;
  std::string displayName;
  std::vector<TroopType> availableTroops;
  std::string primaryBuilding = "barracks";
  FormationType formation_type = FormationType::Roman;

  [[nodiscard]] auto getMeleeTroops() const -> std::vector<const TroopType *>;

  [[nodiscard]] auto getRangedTroops() const -> std::vector<const TroopType *>;

  [[nodiscard]] auto
  getTroop(Game::Units::TroopType unit_type) const -> const TroopType *;

  [[nodiscard]] auto getBestMeleeTroop() const -> const TroopType *;
  [[nodiscard]] auto getBestRangedTroop() const -> const TroopType *;

  [[nodiscard]] auto
  isMeleeUnit(Game::Units::TroopType unit_type) const -> bool;
  [[nodiscard]] auto
  is_ranged_unit(Game::Units::TroopType unit_type) const -> bool;
};

class NationRegistry {
public:
  static auto instance() -> NationRegistry &;

  void registerNation(Nation nation);

  auto getNation(const std::string &nationId) const -> const Nation *;

  auto getNationForPlayer(int player_id) const -> const Nation *;

  void setPlayerNation(int player_id, const std::string &nationId);

  auto getAllNations() const -> const std::vector<Nation> & {
    return m_nations;
  }

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
