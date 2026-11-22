#pragma once

#include "render/entity/wall_authoring.h"

namespace Render::GL::Carthage {

// The Carthage wall look the shared wall authoring is baked with.
auto wall_palette() -> const WallPalette&;
auto wall_geometry() -> const WallGeometry&;

} // namespace Render::GL::Carthage
