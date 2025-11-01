#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Kingdom {

struct KnightStyleConfig;

void register_swordsman_style(const std::string &nation_id,
                           const KnightStyleConfig &style);

void registerKnightRenderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Kingdom
