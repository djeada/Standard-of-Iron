#pragma once

#include "../map_definition.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Game::Map::Minimap {

namespace Constants {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_degrees_to_radians = 3.14159265358979323846F / 180.0F;
constexpr float k_radians_to_degrees = 180.0F / 3.14159265358979323846F;
constexpr float k_default_camera_yaw_deg = 225.0F;

} // namespace Constants

/// Stores precomputed sin/cos for the minimap rotation.
/// Call set_yaw_degrees() to update when the camera yaw changes.
class MinimapOrientation {
public:
  static auto instance() -> MinimapOrientation & {
    static MinimapOrientation s_instance;
    return s_instance;
  }

  void set_yaw_degrees(float yaw_deg) {
    if (std::abs(m_yaw_deg - yaw_deg) < 0.001F) {
      return;
    }
    m_yaw_deg = yaw_deg;
    const float rad = yaw_deg * Constants::k_degrees_to_radians;
    m_cos = std::cos(rad);
    m_sin = std::sin(rad);
    m_dirty = true;
  }

  [[nodiscard]] auto yaw_degrees() const -> float { return m_yaw_deg; }
  [[nodiscard]] auto cos_yaw() const -> float { return m_cos; }
  [[nodiscard]] auto sin_yaw() const -> float { return m_sin; }
  [[nodiscard]] auto is_dirty() const -> bool { return m_dirty; }
  void clear_dirty() { m_dirty = false; }

private:
  // Compute initial cos/sin from the default yaw angle (225 degrees)
  static constexpr float compute_default_cos() {
    // cos(225°) = cos(180° + 45°) = -cos(45°) ≈ -0.70710678118
    // Using numeric constant since constexpr std::cos isn't available in C++17
    return -0.70710678118F;
  }
  static constexpr float compute_default_sin() {
    // sin(225°) = sin(180° + 45°) = -sin(45°) ≈ -0.70710678118
    return -0.70710678118F;
  }

  MinimapOrientation() {
    // Initialize from default, which sets m_cos and m_sin via set_yaw_degrees
    set_yaw_degrees(Constants::k_default_camera_yaw_deg);
    m_dirty = false;
  }

  float m_yaw_deg = Constants::k_default_camera_yaw_deg;
  // Initial values are overwritten by constructor, but provide defaults
  // matching 225° (the default camera yaw)
  float m_cos = compute_default_cos();
  float m_sin = compute_default_sin();
  bool m_dirty = false;
};

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
  const auto &orient = MinimapOrientation::instance();
  const float rotated_x =
      world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z =
      world_x * orient.sin_yaw() + world_z * orient.cos_yaw();

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

  const auto &orient = MinimapOrientation::instance();
  const float cos_val = orient.cos_yaw();
  const float sin_val = orient.sin_yaw();

  const float grid_x = rotated_x * cos_val + rotated_z * sin_val;
  const float grid_z = -rotated_x * sin_val + rotated_z * cos_val;

  const float world_x = grid_x * tile_size;
  const float world_z = grid_z * tile_size;

  return {world_x, world_z};
}

} // namespace Game::Map::Minimap
