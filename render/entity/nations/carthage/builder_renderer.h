#pragma once

#include "../../registry.h"
#include <string>

namespace Render::GL::Carthage {

struct BuilderStyleConfig;

void register_builder_style(const std::string &nation_id,
                            const BuilderStyleConfig &style);

void register_builder_renderer(EntityRendererRegistry &registry);

} // namespace Render::GL::Carthage
