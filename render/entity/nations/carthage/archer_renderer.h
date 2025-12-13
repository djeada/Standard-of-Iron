#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Carthage {

struct ArcherStyleConfig;

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style);

void register_archer_renderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Carthage
