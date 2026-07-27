#pragma once

#include <string>
#include <string_view>

#include "../systems/nation_id.h"

namespace Game::Visuals {

// Stable asset identity for a building, derived from nation and building type.
//
// This is deliberately game-owned rather than renderer-owned: the key is written
// into gameplay state (WallNetworkService stores it in WallAppearance, which is
// serialized), so gameplay must be able to produce it without depending on the
// renderer.  The renderer consumes these keys to pick an archetype.
[[nodiscard]] auto building_asset_key(std::string_view nation_slug,
                                      std::string_view building_type) -> std::string;

[[nodiscard]] auto building_asset_key(Game::Systems::NationID nation_id,
                                      std::string_view building_type) -> std::string;

// Rewrites legacy flat keys ("barracks_roman") to the canonical nested form.
[[nodiscard]] auto canonicalize_building_asset_key(std::string_view asset_key)
    -> std::string_view;

// Returns the authored key when one is set, otherwise derives it.
[[nodiscard]] auto resolve_building_asset_key(std::string_view asset_key,
                                              std::string_view building_type,
                                              Game::Systems::NationID nation_id)
    -> std::string;

} // namespace Game::Visuals
