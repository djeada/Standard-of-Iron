#pragma once

#include <cmath>
#include <cstdint>

// Shared measurements of the palisade wall walk: the timber balcony that hangs
// on the town face of a wall, the stairs that reach it, and the crest a siege
// tower's bridge lands on. The simulation walks troops along these lines and
// the renderer builds the balcony from the same numbers, so a soldier's feet
// meet the planks.
namespace Game::Systems::WallWalk {

// Top of the balcony planks above the terrain.
inline constexpr float k_deck_height = 1.80F;
// Lateral reach of the balcony from the wall's centre line.
inline constexpr float k_deck_inner_edge = 0.17F;
inline constexpr float k_deck_outer_edge = 0.70F;
// Line soldiers walk along, between the two deck edges.
inline constexpr float k_deck_lane = 0.45F;
inline constexpr float k_deck_thickness = 0.06F;

// Stairs run straight down from the balcony edge into the town.
inline constexpr float k_stair_run = 1.85F;
inline constexpr float k_stair_half_width = 0.28F;
inline constexpr int k_stair_steps = 9;
// A stair stands on every k_stair_period-th straight segment of a run.
inline constexpr int k_stair_period = 4;
inline constexpr int k_stair_phase = 1;

// Where a siege tower's bridge rests: on the stake tips, between two posts.
inline constexpr float k_crest_height = 2.95F;

struct Point {
  float x{0.0F};
  float z{0.0F};
};

[[nodiscard]] constexpr auto lane_point(float node_x,
                                        float node_z,
                                        std::int8_t inner_x,
                                        std::int8_t inner_z) noexcept -> Point {
  return {node_x + static_cast<float>(inner_x) * k_deck_lane,
          node_z + static_cast<float>(inner_z) * k_deck_lane};
}

[[nodiscard]] constexpr auto stair_top(float node_x,
                                       float node_z,
                                       std::int8_t inner_x,
                                       std::int8_t inner_z) noexcept -> Point {
  return lane_point(node_x, node_z, inner_x, inner_z);
}

[[nodiscard]] constexpr auto stair_foot(float node_x,
                                        float node_z,
                                        std::int8_t inner_x,
                                        std::int8_t inner_z) noexcept -> Point {
  constexpr float reach = k_deck_outer_edge + k_stair_run + 0.25F;
  return {node_x + static_cast<float>(inner_x) * reach,
          node_z + static_cast<float>(inner_z) * reach};
}

[[nodiscard]] constexpr auto stair_slot(int along_grid) noexcept -> bool {
  int const index = along_grid / 2;
  int const phase = ((index % k_stair_period) + k_stair_period) % k_stair_period;
  return phase == k_stair_phase;
}

// Whether a point ordered on the ground means "onto this segment's balcony":
// on the balcony itself, or on the stakes, within half a segment along the run.
[[nodiscard]] inline auto is_wall_walk_order(float node_x,
                                             float node_z,
                                             std::int8_t inner_x,
                                             std::int8_t inner_z,
                                             float x,
                                             float z) noexcept -> bool {
  constexpr float k_lane_reach = 0.8F;
  constexpr float k_line_reach = 0.6F;
  auto const lane = lane_point(node_x, node_z, inner_x, inner_z);
  if (std::hypot(lane.x - x, lane.z - z) <= k_lane_reach ||
      std::hypot(node_x - x, node_z - z) <= k_line_reach) {
    return true;
  }
  bool const runs_x = inner_z != 0 && inner_x == 0;
  bool const runs_z = inner_x != 0 && inner_z == 0;
  if (!runs_x && !runs_z) {
    return false;
  }
  float const along = runs_x ? std::abs(x - node_x) : std::abs(z - node_z);
  float const across = runs_x ? z - node_z : x - node_x;
  float const inward = across * static_cast<float>(runs_x ? inner_z : inner_x);
  return along <= 1.05F && inward >= -k_line_reach &&
         inward <= k_deck_outer_edge + 0.25F;
}

} // namespace Game::Systems::WallWalk
