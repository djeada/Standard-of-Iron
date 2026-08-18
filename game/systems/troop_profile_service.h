#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "../units/troop_catalog.h"
#include "nation_registry.h"

namespace Game::Systems {

struct TroopProfile {
  std::string display_name;
  Game::Units::TroopProductionStats production;
  Game::Units::TroopCombatStats combat;
  Game::Units::TroopVisualStats visuals;
  Game::Units::TroopLore lore;
  std::vector<std::string> abilities;
  std::vector<std::string> documented_abilities;
  int individuals_per_unit = 1;
  int max_units_per_row = 1;
  Game::Formation::FormationDoctrineId doctrine{"rome"};

  [[nodiscard]] auto has_ability(const std::string& ability_id) const -> bool;
};

class TroopProfileService {
public:
  static auto instance() -> TroopProfileService&;

  auto get_profile(NationID nation_id, Game::Units::TroopType type) -> TroopProfile;

  [[nodiscard]] auto get_profile_ref(NationID nation_id, Game::Units::TroopType type)
      -> const TroopProfile&;

  void prime();

  [[nodiscard]] auto find_profile(NationID nation_id, Game::Units::TroopType type) const
      -> const TroopProfile*;

  void clear();

private:
  TroopProfileService() = default;

  auto build_profile(const Nation& nation, Game::Units::TroopType type) -> TroopProfile;

  std::unordered_map<NationID, std::unordered_map<Game::Units::TroopType, TroopProfile>>
      m_cache;
};

} // namespace Game::Systems
