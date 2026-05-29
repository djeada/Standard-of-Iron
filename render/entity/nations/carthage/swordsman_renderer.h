#pragma once

#include <string>

#include "../../registry.h"
#include "../../swordsman_renderer_common.h"

namespace Render::GL::Carthage {

using KnightStyleConfig = ::Render::GL::SwordsmanStyleConfig;

inline void register_swordsman_style(const std::string& nation_id,
                                     const KnightStyleConfig& style) {
  ::Render::GL::register_swordsman_style(nation_id, style);
}

void register_knight_renderer(EntityRendererRegistry& registry);
void register_skeleton_swordsman_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
