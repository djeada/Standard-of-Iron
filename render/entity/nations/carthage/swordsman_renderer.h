#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Carthage {

struct KnightStyleConfig;

void register_swordsman_style(const std::string &nation_id,
                              const KnightStyleConfig &style);

void register_knight_renderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Carthage
