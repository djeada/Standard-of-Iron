#pragma once

#include "render/entity/wall_authoring.h"

namespace Render::GL::Roman {

// The Roman wall look the shared wall authoring is baked with.
auto wall_palette() -> const WallPalette&;
auto wall_geometry() -> const WallGeometry&;

} // namespace Render::GL::Roman
