#pragma once

#include <cmath>
#include <cstdint>

#include "visibility_service.h"

namespace Game::Map {

enum class RenderVisibilityState : std::uint8_t {
  Hidden = 0,
  Explored = 1,
  Visible = 2,
};

[[nodiscard]] inline auto classify_world_visibility(
    const VisibilityService::Snapshot& snapshot,
    float world_x,
    float world_z) -> RenderVisibilityState {
  if (!snapshot.initialized) {
    return RenderVisibilityState::Visible;
  }
  if (snapshot.is_visible_world(world_x, world_z)) {
    return RenderVisibilityState::Visible;
  }
  if (snapshot.is_explored_world(world_x, world_z)) {
    return RenderVisibilityState::Explored;
  }
  return RenderVisibilityState::Hidden;
}

[[nodiscard]] inline auto classify_static_world_cell_visibility(
    const VisibilityService::Snapshot& snapshot,
    int grid_x,
    int grid_z) -> RenderVisibilityState {
  if (!snapshot.initialized) {
    return RenderVisibilityState::Visible;
  }
  switch (snapshot.state_at(grid_x, grid_z)) {
  case VisibilityState::Visible:
    return RenderVisibilityState::Visible;
  case VisibilityState::Explored:
    return RenderVisibilityState::Explored;
  case VisibilityState::Unseen:
  default:
    return RenderVisibilityState::Hidden;
  }
}

[[nodiscard]] inline auto is_world_position_in_visibility_bounds(
    const VisibilityService::Snapshot& snapshot,
    float world_x,
    float world_z) -> bool {
  if (!snapshot.initialized) {
    return true;
  }
  constexpr float k_half_cell_offset = 0.5F;
  const float grid_x = world_x / snapshot.tile_size + snapshot.half_width;
  const float grid_z = world_z / snapshot.tile_size + snapshot.half_height;
  const int ix = static_cast<int>(std::floor(grid_x + k_half_cell_offset));
  const int iz = static_cast<int>(std::floor(grid_z + k_half_cell_offset));
  return ix >= 0 && ix < snapshot.width && iz >= 0 && iz < snapshot.height;
}

[[nodiscard]] inline auto should_render_non_local_unit(
    const VisibilityService::Snapshot& snapshot,
    float world_x,
    float world_z) -> bool {
  return classify_world_visibility(snapshot, world_x, world_z) ==
         RenderVisibilityState::Visible;
}

[[nodiscard]] inline auto should_render_non_local_preview(
    const VisibilityService::Snapshot& snapshot,
    float world_x,
    float world_z) -> bool {
  return should_render_non_local_unit(snapshot, world_x, world_z);
}

[[nodiscard]] inline auto should_render_combat_effect(
    const VisibilityService::Snapshot& snapshot,
    float world_x,
    float world_z) -> bool {
  return classify_world_visibility(snapshot, world_x, world_z) ==
         RenderVisibilityState::Visible;
}

[[nodiscard]] inline auto is_static_world_cell_visible(
    const VisibilityService::Snapshot& snapshot,
    int grid_x,
    int grid_z) -> bool {
  return classify_static_world_cell_visibility(snapshot, grid_x, grid_z) ==
         RenderVisibilityState::Visible;
}

} // namespace Game::Map
