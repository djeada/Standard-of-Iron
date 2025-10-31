#pragma once

#include "../units/troop_type.h"
#include "formation_system.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

struct NationTroopVariant {
  Game::Units::TroopType unit_type;
  std::optional<int> health;
  std::optional<int> max_health;
  std::optional<float> speed;
  std::optional<float> vision_range;
  std::optional<int> attack_damage;
  std::optional<float> attack_range;
  std::optional<int> melee_damage;
  std::optional<float> melee_range;
  std::optional<float> attack_cooldown;
  std::optional<float> melee_cooldown;
  std::optional<int> individuals_per_unit;
  std::optional<int> max_units_per_row;
  std::optional<float> selection_ring_size;
  std::optional<float> selection_ring_y_offset;
  std::optional<float> selection_ring_ground_offset;
  std::optional<float> render_scale;
  std::optional<FormationType> formation_type;
  std::optional<std::string> renderer_id;
  std::optional<bool> can_ranged;
  std::optional<bool> can_melee;
};

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
  std::unordered_map<Game::Units::TroopType, NationTroopVariant> troopVariants;

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

  void clearPlayerAssignments();

  auto default_nation_id() const -> const std::string & {
    return m_defaultNation;
  }

private:
  NationRegistry() = default;

  std::vector<Nation> m_nations;
  std::unordered_map<std::string, size_t> m_nationIndex;
  std::unordered_map<int, std::string> m_playerNations;
  std::string m_defaultNation = "kingdom_of_iron";
  bool m_initialized = false;
};

} // namespace Game::Systems
