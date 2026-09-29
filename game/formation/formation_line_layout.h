#pragma once

#include <vector>

#include "army_formation_planner.h"

namespace Game::Formation::planning {

struct LineLayout {
  std::vector<FormationSlot> slot_list;
  float slot_spacing{0.0F};
  int row_cap_used{0};
};

[[nodiscard]] auto lane_for(float base_spacing) -> float;

[[nodiscard]] auto plan_line_layout(const std::vector<ArmyFormationMember>& members,
                                    const DoctrineIntentTemplate& tmpl,
                                    const ArmyFormationOptions& options,
                                    float spacing,
                                    float requested_frontage,
                                    float facing,
                                    int row_cap_override) -> LineLayout;

} // namespace Game::Formation::planning
