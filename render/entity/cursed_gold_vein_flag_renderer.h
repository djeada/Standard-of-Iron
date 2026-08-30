#pragma once

#include <string_view>

#include "game/visuals/building_asset_key.h"

namespace Render::GL {

class EntityRendererRegistry;

inline constexpr std::string_view k_cursed_gold_vein_flag_renderer_key =
    Game::Visuals::k_cursed_gold_vein_flag_asset_key;

void register_cursed_gold_vein_flag_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL
