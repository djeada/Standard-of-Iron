#pragma once

#include <string>

#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void register_builder_renderer(EntityRendererRegistry& registry);
void register_civilian_renderer(EntityRendererRegistry& registry);
void ensure_civilian_equipment_registered();

} // namespace Render::GL::Carthage
