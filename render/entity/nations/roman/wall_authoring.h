#pragma once

#include "render/entity/wall_authoring.h"

namespace Render::GL::Roman {

auto wall_palette() -> const WallPalette&;
auto wall_geometry() -> const WallGeometry&;

} // namespace Render::GL::Roman
