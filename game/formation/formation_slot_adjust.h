#pragma once

#include <vector>

#include "army_formation_types.h"

namespace Game::Formation::planning {

void separate_footprints(std::vector<FormationSlot>& slot_list,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float gap);

void recentre_on_centroid(std::vector<FormationSlot>& slot_list);

void recentre(std::vector<FormationSlot>& slot_list);

void apply_reserve_rows(std::vector<FormationSlot>& slot_list,
                        int reserve_rows,
                        float spacing);

void apply_ranged_placement(std::vector<FormationSlot>& slot_list,
                            RangedPlacement placement,
                            float spacing);

void scale_to_frontage(std::vector<FormationSlot>& slot_list,
                       float requested_frontage,
                       float frontage_scale,
                       float depth_scale,
                       float lateral_floor,
                       float max_frontage,
                       float max_depth);

} // namespace Game::Formation::planning
