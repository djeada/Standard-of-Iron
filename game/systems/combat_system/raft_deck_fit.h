#pragma once

#include <algorithm>
#include <limits>

#include "../formation_combat_geometry.h"

namespace Game::Systems {

inline constexpr float k_raft_deck_half_length = 1.9F;

inline constexpr float k_raft_deck_half_width = 2.7F;

// How a passenger unit's formation is laid onto a raft deck.
//
// Boarding narrows the unit to as many files as the deck is wide, but the layout's
// own file and rank spacing (and a deep unit's extra ranks) still made a 24-man
// company about twice the deck, and the outer men stood on open water at deck
// height. The fit centres the live slots on the deck and squeezes each axis just
// enough that every body stays on the planks; a unit that already fits is only
// centred.
struct RaftDeckFit {
  float centre_x{0.0F};
  float centre_z{0.0F};
  float scale_x{1.0F};
  float scale_z{1.0F};

  [[nodiscard]] auto fit_x(float local_x) const noexcept -> float {
    return (local_x - centre_x) * scale_x;
  }
  [[nodiscard]] auto fit_z(float local_z) const noexcept -> float {
    return (local_z - centre_z) * scale_z;
  }
};

// Files run along the deck's width (across the passenger's facing); ranks along its
// length (the crossing direction the passenger faces).
[[nodiscard]] inline auto
raft_deck_fit(const FormationCombat::FormationLayout& layout) noexcept -> RaftDeckFit {
  auto const& placed = layout.live_slots.empty() ? layout.all_slots : layout.live_slots;
  RaftDeckFit fit;
  if (placed.empty()) {
    return fit;
  }
  float min_x = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float min_z = std::numeric_limits<float>::max();
  float max_z = std::numeric_limits<float>::lowest();
  for (auto const& slot : placed) {
    min_x = std::min(min_x, slot.local_x);
    max_x = std::max(max_x, slot.local_x);
    min_z = std::min(min_z, slot.local_z);
    max_z = std::max(max_z, slot.local_z);
  }
  fit.centre_x = (min_x + max_x) * 0.5F;
  fit.centre_z = (min_z + max_z) * 0.5F;
  // A body keeps its radius inside the deck edge, but never less than a hand's
  // breadth of standing room for the slot centres.
  float const body = std::clamp(layout.body_radius, 0.0F, 0.45F);
  float const room_x = std::max(0.1F, k_raft_deck_half_width - body);
  float const room_z = std::max(0.1F, k_raft_deck_half_length - body);
  float const half_x = (max_x - min_x) * 0.5F;
  float const half_z = (max_z - min_z) * 0.5F;
  fit.scale_x = half_x > room_x ? room_x / half_x : 1.0F;
  fit.scale_z = half_z > room_z ? room_z / half_z : 1.0F;
  return fit;
}

} // namespace Game::Systems
