#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace Game::Units {

// The drawn body of each building type, in local mesh units (width along
// local X, depth along local Z, and the body centre's offset from the origin).
// Multiply by the entity's transform scale for world units. This is static
// content, shared by the collision registry (walkability, line of sight) and
// the renderer (foundations), so it lives here rather than in either system.
struct BuildingBodyExtent {
  std::string_view type;
  float width;
  float depth;
  float offset_x;
  float offset_z;
};

inline constexpr std::array<BuildingBodyExtent, 8> k_building_body_extents{{
    {"barracks", 8.65F, 4.20F, 2.325F, 0.0F},
    {"home", 2.36F, 2.42F, 0.0F, 0.03F},
    {"marketplace", 2.80F, 2.80F, 0.0F, 0.0F},
    {"temple", 6.32F, 4.66F, -0.38F, 0.0F},
    {"farm", 1.98F, 2.04F, 0.0F, 0.03F},
    {"defense_tower", 2.60F, 2.60F, 0.0F, 0.0F},
    {"wall_segment", 2.02F, 0.76F, 0.0F, 0.0F},
    {"wall_gate", 2.02F, 0.76F, 0.0F, 0.0F},
}};

[[nodiscard]] constexpr auto
find_building_body_extent(std::string_view type) -> std::optional<BuildingBodyExtent> {
  for (const auto& extent : k_building_body_extents) {
    if (extent.type == type) {
      return extent;
    }
  }
  return std::nullopt;
}

} // namespace Game::Units
