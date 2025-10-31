#pragma once

#include "../units/building_type.h"
#include "../units/troop_type.h"
#include "formation_system.h"
#include "nation_id.h"
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
  NationID id;
  std::string displayName;
  std::vector<TroopType> availableTroops;
  Game::Units::BuildingType primaryBuilding = Game::Units::BuildingType::Barracks;
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

  auto getNation(NationID nationId) const -> const Nation *;

  auto getNationForPlayer(int player_id) const -> const Nation *;

  void setPlayerNation(int player_id, NationID nationId);

  auto getAllNations() const -> const std::vector<Nation> & {
    return m_nations;
  }

  void initializeDefaults();

  void clear();

  void clearPlayerAssignments();

  auto default_nation_id() const -> NationID { return m_defaultNation; }

private:
  NationRegistry() = default;

  std::vector<Nation> m_nations;
  std::unordered_map<NationID, size_t> m_nationIndex;
  std::unordered_map<int, NationID> m_playerNations;
  NationID m_defaultNation = NationID::KingdomOfIron;
  bool m_initialized = false;
};

} // namespace Game::Systems
