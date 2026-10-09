#pragma once

#include <vector>

#include "army_formation_types.h"
#include "formation_silhouette.h"

namespace Game::Formation::planning {

// The narrowest open lane a triplex acies leaves between two maniples of one
// line. A war elephant's body is about 4.6 m across, so a lane this wide lets
// one pass without its trample reaching the maniples on either side.
inline constexpr float k_min_maniple_lane = 6.0F;

// How far the crescent's centre bulges ahead of its wings, as a share of the
// centre's half width (never less than two troop depths).
inline constexpr float k_crescent_bulge_share = 0.32F;

// How far a fully yielded centre has travelled back, as a multiple of the bulge:
// two bulges turn the convex arc into the mirrored concave one.
inline constexpr float k_crescent_yield_reach = 2.0F;

struct BattleOrderGaps {
  float lateral{1.6F};
  float rank{2.0F};
};

// Lays out the three historical battle orders (triplex acies, convex crescent,
// elephant screen). Returns false for any other intent.
auto place_battle_order(std::vector<FormationSlot>& slot_list,
                        const SlotExtents& extents,
                        const SilhouetteParams& params,
                        const BattleOrderGaps& gaps) -> bool;

// Positions of `count` troops on a lattice of the given pitch, centred on zero as
// closely as the lattice phase allows (phase 0 = integer multiples of the pitch,
// 0.5 = half-way between them).
[[nodiscard]] auto lattice_positions(int count,
                                     float phase,
                                     float pitch) -> std::vector<float>;

} // namespace Game::Formation::planning
