#pragma once

#include <string>

#include "../../registry.h"

namespace Render::GL::Roman {

void register_builder_renderer(EntityRendererRegistry& registry);
void register_civilian_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
