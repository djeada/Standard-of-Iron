#pragma once

#include <string_view>

#include "render/entity/wall_authoring.h"

namespace Render::GL {

class EntityRendererRegistry;

struct MasonryWallLook {
  std::string_view slug;
  const WallPalette* palette;
  const WallGeometry* geometry;

  const WallPalette* timber;
};

[[nodiscard]] auto roman_masonry_wall() -> const MasonryWallLook&;
[[nodiscard]] auto carthage_masonry_wall() -> const MasonryWallLook&;

void register_masonry_wall_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
