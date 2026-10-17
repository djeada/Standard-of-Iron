#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace Game::Units {

struct BuildingBodyExtent {
  std::string_view type;
  float width;
  float depth;
  float offset_x;
  float offset_z;
};

inline constexpr std::array<BuildingBodyExtent, 8> k_building_body_extents{{
    {"barracks", 3.85F, 3.80F, 0.0F, -0.02F},
    {"home", 2.36F, 2.42F, 0.0F, 0.03F},
    {"marketplace", 2.80F, 2.80F, 0.0F, 0.0F},
    {"temple", 6.32F, 4.66F, -0.38F, 0.0F},
    {"farm", 1.98F, 2.04F, 0.0F, 0.03F},
    {"defense_tower", 2.60F, 2.60F, 0.0F, 0.0F},
    {"wall_segment", 2.02F, 0.76F, 0.0F, 0.0F},
    {"wall_gate", 2.02F, 0.76F, 0.0F, 0.0F},
}};

struct BuildingSpawnScale {
  std::string_view type;
  float horizontal;
  float vertical;
};

inline constexpr std::array<BuildingSpawnScale, 7> k_building_spawn_scales{{
    {"barracks", 1.8F, 1.2F},
    {"home", 1.8F, 1.5F},
    {"temple", 1.95F, 1.725F},
    {"marketplace", 1.95F, 1.5F},
    {"farm", 7.14F, 5.1F},
    {"defense_tower", 1.25F, 2.0F},
    {"wall_gate", 1.5F, 1.5F},
}};

[[nodiscard]] constexpr auto
building_spawn_scale(std::string_view type) -> BuildingSpawnScale {
  for (const auto& scale : k_building_spawn_scales) {
    if (scale.type == type) {
      return scale;
    }
  }
  return {type, 1.0F, 1.0F};
}

[[nodiscard]] constexpr auto
find_building_body_extent(std::string_view type) -> std::optional<BuildingBodyExtent> {
  for (const auto& extent : k_building_body_extents) {
    if (extent.type == type) {
      return extent;
    }
  }
  return std::nullopt;
}

inline constexpr std::array<BuildingBodyExtent, 1> k_building_foundation_extents{{
    {"barracks", 8.65F, 4.20F, 2.325F, 0.0F},
}};

[[nodiscard]] constexpr auto find_building_foundation_extent(std::string_view type)
    -> std::optional<BuildingBodyExtent> {
  for (const auto& extent : k_building_foundation_extents) {
    if (extent.type == type) {
      return extent;
    }
  }
  return find_building_body_extent(type);
}

[[nodiscard]] constexpr auto find_building_body_world_extent(std::string_view type)
    -> std::optional<BuildingBodyExtent> {
  auto extent = find_building_body_extent(type);
  if (!extent.has_value()) {
    return std::nullopt;
  }
  float const scale = building_spawn_scale(type).horizontal;
  extent->width *= scale;
  extent->depth *= scale;
  extent->offset_x *= scale;
  extent->offset_z *= scale;
  return extent;
}

} // namespace Game::Units
