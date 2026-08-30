#pragma once

#include <string_view>

#include "game/visuals/building_asset_key.h"

namespace Render::GL {

class EntityRendererRegistry;

// The capture anchor a cursed gold vein spawns is a Barracks entity that draws only
// a claim flag beside the vein prop; the game layer stamps this key on it.
inline constexpr std::string_view k_cursed_gold_vein_flag_renderer_key =
    Game::Visuals::k_cursed_gold_vein_flag_asset_key;

void register_cursed_gold_vein_flag_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL
