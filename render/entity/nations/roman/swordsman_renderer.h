#pragma once

#include <string>

#include "../../registry.h"
#include "../../swordsman_renderer_common.h"

namespace Render::GL::Roman {

using KnightStyleConfig = ::Render::GL::SwordsmanStyleConfig;

inline void register_swordsman_style(const std::string& nation_id,
                                     const KnightStyleConfig& style) {
  ::Render::GL::register_swordsman_style(nation_id, style);
}

void register_knight_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
