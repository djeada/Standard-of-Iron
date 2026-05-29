#pragma once

#include <string>

#include "../../builder_renderer_common.h"

namespace Render::GL::Roman {

using BuilderStyleConfig = Render::GL::BuilderStyleConfig;

void register_builder_style(const std::string& nation_id,
                            const BuilderStyleConfig& style);
void register_roman_builder_style();

} // namespace Render::GL::Roman
