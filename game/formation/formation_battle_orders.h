#pragma once

#include <vector>

#include "army_formation_types.h"
#include "formation_silhouette.h"

namespace Game::Formation::planning {

inline constexpr float k_min_maniple_lane = 6.0F;

inline constexpr float k_crescent_bulge_share = 0.32F;

inline constexpr float k_crescent_yield_reach = 2.0F;

struct BattleOrderGaps {
  float lateral{1.6F};
  float rank{2.0F};
};

auto place_battle_order(std::vector<FormationSlot>& slot_list,
                        const SlotExtents& extents,
                        const SilhouetteParams& params,
                        const BattleOrderGaps& gaps) -> bool;

[[nodiscard]] auto
lattice_positions(int count, float phase, float pitch) -> std::vector<float>;

} // namespace Game::Formation::planning
