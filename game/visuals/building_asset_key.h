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

// A building style is presentation only: it picks a different renderer for a
// structure and leaves its type, footprint, collision body and economy alone.
// Maps author it per structure ("style" on a structures[] entry); a style a
// nation has no renderer for draws that nation's plain building instead.
struct BuildingVisualStyle {
  std::string_view building_type;
  std::string_view style;
  std::string_view nation_slug;
};

inline constexpr std::array<BuildingVisualStyle, 12> k_building_visual_styles{{
    // Multi-storey town housing that fills its lot and abuts its neighbours:
    // the Roman insula and the Punic tall house.
    {"home", "tenement", "roman"},
    {"home", "tenement", "carthage"},
    // A colonnaded walk with shops behind it; a row of them is a stoa.
    {"marketplace", "portico", "roman"},
    {"marketplace", "portico", "carthage"},
    // The portico at its grandest: always two storeys, a gallery over the
    // colonnade. The long sides of a forum or an agora.
    {"marketplace", "basilica", "roman"},
    {"marketplace", "basilica", "carthage"},
    // A quay with its warehouse and a vessel alongside, front to the water.
    {"marketplace", "dock", "roman"},
    {"marketplace", "dock", "carthage"},
    // A stone curtain wall where a palisade would stand.
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

// The asset key a styled structure draws with, or the plain building key when
// the style is empty, unknown, or not built for that nation.
[[nodiscard]] auto styled_building_asset_key(Game::Systems::NationID nation_id,
                                             std::string_view building_type,
                                             std::string_view style) -> std::string;

// A wall's renderer key is recomputed whenever its neighbours change. This
// turns the plain key the wall network arrived at ("troops/roman/wall_...")
// into the styled one ("troops/roman_masonry/wall_..."), or returns it as it
// is when the style does not apply.
[[nodiscard]] auto apply_wall_style(std::string asset_key,
                                    std::string_view style) -> std::string;

[[nodiscard]] auto
canonicalize_building_asset_key(std::string_view asset_key) -> std::string_view;

[[nodiscard]] auto
resolve_building_asset_key(std::string_view asset_key,
                           std::string_view building_type,
                           Game::Systems::NationID nation_id) -> std::string;

} // namespace Game::Visuals
