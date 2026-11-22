#pragma once

#include <string>

#include "render/creature/humanoid/humanoid_types.h"
#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void register_healer_renderer(EntityRendererRegistry& registry);
void register_grave_priest_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
