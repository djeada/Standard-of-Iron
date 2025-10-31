#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Kingdom {

struct ArcherStyleConfig;

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style);

void registerArcherRenderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Kingdom
