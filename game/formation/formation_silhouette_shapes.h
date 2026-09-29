#pragma once

#include <cstddef>
#include <vector>

#include "army_formation_types.h"

namespace Game::Formation::planning {

inline constexpr int k_hollow_square_minimum = 4;

struct HollowSquareMembers {
  const std::vector<std::size_t>& perimeter;
  const std::vector<std::size_t>& inner;
};

void place_hollow_square(std::vector<FormationSlot>& slot_list,
                         const HollowSquareMembers& members,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float lateral_gap,
                         float rank_gap);

void bend_into_crescent(std::vector<FormationSlot>& slot_list,
                        const std::vector<std::size_t>& left_wing,
                        const std::vector<std::size_t>& right_wing,
                        float front_half_width);

} // namespace Game::Formation::planning
