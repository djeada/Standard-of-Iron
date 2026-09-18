#pragma once

#include <string>

#include "render/entity/registry.h"
#include "render/entity/swordsman_renderer_common.h"

namespace Render::GL::Roman {

using SwordsmanStyleConfig = ::Render::GL::SwordsmanStyleConfig;

inline void register_swordsman_style(const std::string& nation_id,
                                     const SwordsmanStyleConfig& style) {
  ::Render::GL::register_swordsman_style(nation_id, style);
}

void register_swordsman_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
