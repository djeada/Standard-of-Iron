#pragma once

#include <string>

#include "render/entity/builder_renderer_common.h"

namespace Render::GL::Carthage {

using BuilderStyleConfig = Render::GL::BuilderStyleConfig;

void register_builder_style(const std::string& nation_id,
                            const BuilderStyleConfig& style);
void register_carthage_builder_style();

} // namespace Render::GL::Carthage
