#pragma once

#include "render/entity/registry.h"
#include "render/entity/wall_renderer_common.h"

namespace Render::GL::Carthage {

void register_wall_renderer(EntityRendererRegistry& registry);

auto wall_palette() -> const WallPalette&;
auto wall_geometry() -> const WallGeometry&;

} // namespace Render::GL::Carthage
