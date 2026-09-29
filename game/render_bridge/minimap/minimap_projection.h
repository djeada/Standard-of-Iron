#pragma once

#include <utility>

#include "../../map/map_definition.h"
#include "minimap_utils.h"

namespace Game::Map::Minimap {

class MinimapProjection {
public:
  MinimapProjection(const GridDefinition& grid, float pixels_per_tile)
      : m_extent(rotated_world_bounds(grid.width * grid.tile_size,
                                      grid.height * grid.tile_size))
      , m_image_width(grid.width * pixels_per_tile)
      , m_image_height(grid.height * pixels_per_tile) {}

  [[nodiscard]] auto to_pixel(float world_x,
                              float world_z) const -> std::pair<float, float> {
    return world_to_pixel(world_x,
                          world_z,
                          m_extent.first,
                          m_extent.second,
                          m_image_width,
                          m_image_height);
  }

  [[nodiscard]] auto to_pixel_size(float world_size) const -> float {
    return world_size * (m_image_width / m_extent.first);
  }

private:
  std::pair<float, float> m_extent;
  float m_image_width;
  float m_image_height;
};

} // namespace Game::Map::Minimap
