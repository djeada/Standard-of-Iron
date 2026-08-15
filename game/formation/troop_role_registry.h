#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../units/troop_type.h"
#include "formation_roles.h"

namespace Game::Formation {

class TroopRoleRegistry {
public:
  static auto instance() -> TroopRoleRegistry&;

  void reset_to_defaults();
  void clear_overrides();

  void set_profile(Game::Units::TroopType troop, TroopFormationProfile profile);
  void merge_profile(Game::Units::TroopType troop,
                     const TroopFormationProfile& overrides);

  [[nodiscard]] auto
  profile(Game::Units::TroopType troop) const -> const TroopFormationProfile&;

  [[nodiscard]] auto roles(Game::Units::TroopType troop) const -> RoleTagSet;

  [[nodiscard]] auto
  army_roles(Game::Units::TroopType troop) const -> const std::vector<ArmyRole>&;

  [[nodiscard]] auto prefers_army_role(Game::Units::TroopType troop,
                                       ArmyRole role) const -> bool;

private:
  TroopRoleRegistry();

  std::unordered_map<Game::Units::TroopType, TroopFormationProfile> m_profiles;
  TroopFormationProfile m_fallback;
};

[[nodiscard]] auto
default_troop_formation_profile(Game::Units::TroopType troop) -> TroopFormationProfile;

} // namespace Game::Formation
