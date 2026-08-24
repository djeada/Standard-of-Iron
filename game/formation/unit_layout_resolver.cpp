#include "unit_layout_resolver.h"

#include "troop_role_registry.h"

namespace Game::Formation {

auto default_doctrine_for_nation(Game::Systems::NationID nation)
    -> FormationDoctrineId {
  switch (nation) {
  case Game::Systems::NationID::RomanRepublic:
    return "rome";
  case Game::Systems::NationID::Carthage:
    return "carthage";
  case Game::Systems::NationID::IronSepulcher:
    return "iron_sepulcher";
  }
  return k_neutral_doctrine;
}

auto select_unit_layout(const FormationDoctrineId& doctrine,
                        Game::Units::TroopType troop,
                        UnitLayoutState state) -> UnitLayoutId {
  const auto& profile = TroopRoleRegistry::instance().profile(troop);
  const auto& library = UnitLayoutLibrary::instance();

  std::string const* generic = &profile.unit_layout;
  switch (state) {
  case UnitLayoutState::Defensive:
  case UnitLayoutState::Braced:
    if (!profile.defensive_layout.empty()) {
      generic = &profile.defensive_layout;
    }
    break;
  case UnitLayoutState::Marching:
    if (!profile.marching_layout.empty()) {
      generic = &profile.marching_layout;
    }
    break;
  case UnitLayoutState::Working:
    if (!profile.working_layout.empty()) {
      generic = &profile.working_layout;
    }
    break;
  case UnitLayoutState::Normal:
  case UnitLayoutState::Attacking:
  case UnitLayoutState::Routing:
  case UnitLayoutState::Disrupted:
    break;
  }

  return library.resolve(doctrine, *generic);
}

auto select_unit_layout(const FormationDoctrineId& doctrine,
                        Game::Units::TroopType troop,
                        UnitLayoutState state,
                        bool is_constructing) -> UnitLayoutId {
  if (is_constructing) {
    return select_unit_layout(doctrine, troop, UnitLayoutState::Working);
  }
  return select_unit_layout(doctrine, troop, state);
}

} // namespace Game::Formation
