#pragma once

#include <string>
#include <string_view>

#include "../systems/nation_id.h"

namespace Game::Visuals {

inline constexpr std::string_view k_cursed_gold_vein_flag_asset_key =
    "troops/cursed_gold_vein/barracks";

[[nodiscard]] auto building_asset_key(std::string_view nation_slug,
                                      std::string_view building_type) -> std::string;

[[nodiscard]] auto building_asset_key(Game::Systems::NationID nation_id,
                                      std::string_view building_type) -> std::string;

[[nodiscard]] auto
canonicalize_building_asset_key(std::string_view asset_key) -> std::string_view;

[[nodiscard]] auto
resolve_building_asset_key(std::string_view asset_key,
                           std::string_view building_type,
                           Game::Systems::NationID nation_id) -> std::string;

} // namespace Game::Visuals
