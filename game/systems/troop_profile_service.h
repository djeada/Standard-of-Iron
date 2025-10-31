#pragma once

#include "../units/troop_catalog.h"
#include "nation_registry.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace Game::Systems {

struct TroopProfile {
  std::string display_name;
  Game::Units::TroopProductionStats production;
  Game::Units::TroopCombatStats combat;
  Game::Units::TroopVisualStats visuals;
  int individuals_per_unit = 1;
  int max_units_per_row = 1;
  FormationType formation_type = FormationType::Roman;
};

class TroopProfileService {
public:
  static auto instance() -> TroopProfileService &;

  auto get_profile(NationID nation_id, Game::Units::TroopType type)
      -> TroopProfile;

  void clear();

private:
  TroopProfileService() = default;

  auto build_profile(const Nation &nation,
                     Game::Units::TroopType type) -> TroopProfile;

  std::unordered_map<NationID,
                     std::unordered_map<Game::Units::TroopType, TroopProfile>>
      m_cache;
};

} // namespace Game::Systems
