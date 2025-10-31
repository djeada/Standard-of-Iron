#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Roman {

struct KnightStyleConfig;

void register_knight_style(const std::string &nation_id,
                           const KnightStyleConfig &style);

void registerKnightRenderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Roman
