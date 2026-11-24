#pragma once

#include <string_view>

#include "render/entity/wall_authoring.h"

namespace Render::GL {

class EntityRendererRegistry;

// The masonry wall style: a curtain of dressed stone with a crenellated walk,
// for the walls of a capital. One look per nation, baked and registered under
// "<nation>_masonry" so the wall network can swap a palisade for it by key.
struct MasonryWallLook {
  std::string_view slug;
  const WallPalette* palette;
  const WallGeometry* geometry;
  // The gate's leaves are timber whatever the wall is built of.
  const WallPalette* timber;
};

[[nodiscard]] auto roman_masonry_wall() -> const MasonryWallLook&;
[[nodiscard]] auto carthage_masonry_wall() -> const MasonryWallLook&;

void register_masonry_wall_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
