#pragma once

#include <string>

#include "../../archer_renderer_common.h"
#include "../../registry.h"

namespace Render::GL::Roman {

using ArcherStyleConfig = ::Render::GL::ArcherStyleConfig;

inline void register_archer_style(const std::string& nation_id,
                                  const ArcherStyleConfig& style) {
  ::Render::GL::register_archer_style(nation_id, style);
}

void register_archer_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
