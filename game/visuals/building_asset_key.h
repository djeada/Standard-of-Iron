#pragma once

#include <array>
#include <string>
#include <string_view>

#include "../systems/nation_id.h"

namespace Game::Visuals {

inline constexpr std::string_view k_cursed_gold_vein_flag_asset_key =
    "troops/cursed_gold_vein/barracks";

[[nodiscard]] auto
nation_asset_slug(Game::Systems::NationID nation_id) -> std::string_view;

[[nodiscard]] auto building_asset_key(std::string_view nation_slug,
                                      std::string_view building_type) -> std::string;

[[nodiscard]] auto building_asset_key(Game::Systems::NationID nation_id,
                                      std::string_view building_type) -> std::string;

struct BuildingVisualStyle {
  std::string_view building_type;
  std::string_view style;
  std::string_view nation_slug;
};

inline constexpr std::array<BuildingVisualStyle, 12> k_building_visual_styles{{

    {"home", "tenement", "roman"},
    {"home", "tenement", "carthage"},

    {"marketplace", "portico", "roman"},
    {"marketplace", "portico", "carthage"},

    {"marketplace", "basilica", "roman"},
    {"marketplace", "basilica", "carthage"},

    {"marketplace", "dock", "roman"},
    {"marketplace", "dock", "carthage"},

    {"wall_segment", "masonry", "roman"},
    {"wall_segment", "masonry", "carthage"},
    {"wall_gate", "masonry", "roman"},
    {"wall_gate", "masonry", "carthage"},
}};

[[nodiscard]] constexpr auto is_building_visual_style(std::string_view building_type,
                                                      std::string_view style) -> bool {
  for (const auto& entry : k_building_visual_styles) {
    if (entry.building_type == building_type && entry.style == style) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto styled_building_type(std::string_view building_type,
                                        std::string_view style) -> std::string;

[[nodiscard]] auto styled_building_asset_key(Game::Systems::NationID nation_id,
                                             std::string_view building_type,
                                             std::string_view style) -> std::string;

[[nodiscard]] auto apply_wall_style(std::string asset_key,
                                    std::string_view style) -> std::string;

[[nodiscard]] auto
canonicalize_building_asset_key(std::string_view asset_key) -> std::string_view;

[[nodiscard]] auto
resolve_building_asset_key(std::string_view asset_key,
                           std::string_view building_type,
                           Game::Systems::NationID nation_id) -> std::string;

} // namespace Game::Visuals
