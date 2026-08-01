#pragma once

#include <string>

#include "../systems/nation_id.h"
#include "../units/troop_type.h"
#include "army_formation_types.h"
#include "unit_layout.h"

namespace Game::Formation {

[[nodiscard]] auto
default_doctrine_for_nation(Game::Systems::NationID nation) -> FormationDoctrineId;

struct UnitLayoutSelection {
  UnitLayoutId layout{k_invalid_layout};
  UnitLayoutState state{UnitLayoutState::Normal};
};

[[nodiscard]] auto select_unit_layout(const FormationDoctrineId& doctrine,
                                      Game::Units::TroopType troop,
                                      UnitLayoutState state) -> UnitLayoutId;

[[nodiscard]] auto select_unit_layout(const FormationDoctrineId& doctrine,
                                      Game::Units::TroopType troop,
                                      UnitLayoutState state,
                                      bool is_constructing) -> UnitLayoutId;

} // namespace Game::Formation
