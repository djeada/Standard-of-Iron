#pragma once

#include "render/entity/wall_authoring.h"

namespace Render::GL::Carthage {

auto wall_palette() -> const WallPalette&;
auto wall_geometry() -> const WallGeometry&;

} // namespace Render::GL::Carthage
