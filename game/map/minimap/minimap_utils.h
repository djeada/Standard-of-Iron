#pragma once

#include "../map_definition.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Game::Map::Minimap {

namespace Constants {

constexpr float k_camera_yaw_cos = -0.70710678118F;
constexpr float k_camera_yaw_sin = -0.70710678118F;
constexpr float k_min_tile_size = 0.0001F;
constexpr float k_degrees_to_radians = 3.14159265358979323846F / 180.0F;

} // namespace Constants

inline auto
grid_to_world_coords(float grid_x, float grid_z,
                     const MapDefinition &map_def) -> std::pair<float, float> {
  float world_x = grid_x;
  float world_z = grid_z;

  if (map_def.coordSystem == CoordSystem::Grid) {
    const float tile =
        std::max(Constants::k_min_tile_size, map_def.grid.tile_size);
    world_x = (grid_x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
    world_z = (grid_z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
  }

  return {world_x, world_z};
}

inline auto world_to_pixel(float world_x, float world_z, float world_width,
                           float world_height, float img_width,
                           float img_height) -> std::pair<float, float> {
  const float rotated_x = world_x * Constants::k_camera_yaw_cos -
                          world_z * Constants::k_camera_yaw_sin;
  const float rotated_z = world_x * Constants::k_camera_yaw_sin +
                          world_z * Constants::k_camera_yaw_cos;

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py =
      (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

inline auto pixel_to_world(float px, float py, float world_width,
                           float world_height, float img_width,
                           float img_height) -> std::pair<float, float> {
  // Convert pixel to rotated world space
  const float rotated_x = (px * (world_width / img_width)) - world_width * 0.5F;
  const float rotated_z = (py * (world_height / img_height)) - world_height * 0.5F;

  // Inverse rotation (camera yaw is -45 degrees, so inverse is +45 degrees)
  // cos(-45) = -0.707, sin(-45) = -0.707
  // For inverse: cos(+45) = 0.707, sin(+45) = 0.707
  const float inv_cos = -Constants::k_camera_yaw_cos;
  const float inv_sin = -Constants::k_camera_yaw_sin;
  
  const float world_x = rotated_x * inv_cos - rotated_z * inv_sin;
  const float world_z = rotated_x * inv_sin + rotated_z * inv_cos;

  return {world_x, world_z};
}

} // namespace Game::Map::Minimap
