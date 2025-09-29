#pragma once

#include "registry.h"

namespace Render::GL {

// Registers the archer renderer which draws a slender capsule and optional selection ring
void registerArcherRenderer(EntityRendererRegistry& registry);

} // namespace Render::GL
