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

} 

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
                           float img_height,
                           float tile_size) -> std::pair<float, float> {

  const float rotated_x = (px / img_width) * world_width - world_width * 0.5F;
  const float rotated_z =
      (py / img_height) * world_height - world_height * 0.5F;

  const float cos_val = Constants::k_camera_yaw_cos;
  const float sin_val = Constants::k_camera_yaw_sin;

  const float grid_x = rotated_x * cos_val + rotated_z * sin_val;
  const float grid_z = -rotated_x * sin_val + rotated_z * cos_val;

  const float world_x = grid_x * tile_size;
  const float world_z = grid_z * tile_size;

  return {world_x, world_z};
}

} 
