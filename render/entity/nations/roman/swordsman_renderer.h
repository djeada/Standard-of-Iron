#pragma once

#include <string>

#include "../../registry.h"

namespace Render::GL::Roman {

struct KnightStyleConfig;

void register_swordsman_style(const std::string& nation_id,
                              const KnightStyleConfig& style);

void register_knight_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
