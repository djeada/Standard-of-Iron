#pragma once

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <utility>

#include "../../map/map_definition.h"

namespace Game::Map::Minimap {

namespace Constants {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_degrees_to_radians = 3.14159265358979323846F / 180.0F;
constexpr float k_radians_to_degrees = 180.0F / 3.14159265358979323846F;
constexpr float k_default_camera_yaw_deg = 225.0F;

} // namespace Constants

class MinimapOrientation {
public:
  static auto instance() -> MinimapOrientation& {
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
  static constexpr float compute_default_cos() { return -0.70710678118F; }
  static constexpr float compute_default_sin() { return -0.70710678118F; }

  MinimapOrientation() {

    set_yaw_degrees(Constants::k_default_camera_yaw_deg);
    m_dirty = false;
  }

  float m_yaw_deg = Constants::k_default_camera_yaw_deg;

  float m_cos = compute_default_cos();
  float m_sin = compute_default_sin();
  bool m_dirty = false;
};

inline constexpr int k_keep_polygon_points = 8;

inline void
keep_polygon(float cx, float cy, float half, QPointF (&out)[k_keep_polygon_points]) {
  const auto h = static_cast<qreal>(half);
  const auto x = static_cast<qreal>(cx);
  const auto y = static_cast<qreal>(cy);
  const qreal turret_top = y - h;
  const qreal wall_top = y - h * 0.50;
  const qreal base = y + h * 0.90;

  out[0] = QPointF(x - h, base);
  out[1] = QPointF(x - h, turret_top);
  out[2] = QPointF(x - h * 0.44, turret_top);
  out[3] = QPointF(x - h * 0.44, wall_top);
  out[4] = QPointF(x + h * 0.44, wall_top);
  out[5] = QPointF(x + h * 0.44, turret_top);
  out[6] = QPointF(x + h, turret_top);
  out[7] = QPointF(x + h, base);
}

inline auto
grid_to_world_coords(float grid_x,
                     float grid_z,
                     const MapDefinition& map_def) -> std::pair<float, float> {
  float world_x = grid_x;
  float world_z = grid_z;

  if (map_def.coordSystem == CoordSystem::Grid) {
    const float tile = std::max(Constants::k_min_tile_size, map_def.grid.tile_size);
    world_x = (grid_x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
    world_z = (grid_z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
  }

  return {world_x, world_z};
}

inline auto rotated_world_bounds(float world_width,
                                 float world_height) -> std::pair<float, float> {
  const auto& orient = MinimapOrientation::instance();
  const float cos_abs = std::abs(orient.cos_yaw());
  const float sin_abs = std::abs(orient.sin_yaw());
  return {std::max(world_width * cos_abs + world_height * sin_abs,
                   Constants::k_min_tile_size),
          std::max(world_width * sin_abs + world_height * cos_abs,
                   Constants::k_min_tile_size)};
}

inline auto world_to_pixel(float world_x,
                           float world_z,
                           float world_width,
                           float world_height,
                           float img_width,
                           float img_height) -> std::pair<float, float> {
  const auto& orient = MinimapOrientation::instance();
  const float rotated_x = world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z = world_x * orient.sin_yaw() + world_z * orient.cos_yaw();

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py = (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

inline auto world_to_normalized(float world_x,
                                float world_z,
                                float world_width,
                                float world_height,
                                float tile_size) -> std::pair<float, float> {
  const float inv_tile = 1.0F / std::max(tile_size, Constants::k_min_tile_size);
  auto [nx, ny] = world_to_pixel(
      world_x * inv_tile, world_z * inv_tile, world_width, world_height, 1.0F, 1.0F);
  return {std::clamp(nx, 0.0F, 1.0F), std::clamp(ny, 0.0F, 1.0F)};
}

inline auto pixel_to_world(float px,
                           float py,
                           float world_width,
                           float world_height,
                           float img_width,
                           float img_height,
                           float tile_size) -> std::pair<float, float> {

  const float rotated_x = (px / img_width) * world_width - world_width * 0.5F;
  const float rotated_z = (py / img_height) * world_height - world_height * 0.5F;

  const auto& orient = MinimapOrientation::instance();
  const float cos_val = orient.cos_yaw();
  const float sin_val = orient.sin_yaw();

  const float grid_x = rotated_x * cos_val + rotated_z * sin_val;
  const float grid_z = -rotated_x * sin_val + rotated_z * cos_val;

  const float world_x = grid_x * tile_size;
  const float world_z = grid_z * tile_size;

  return {world_x, world_z};
}

} // namespace Game::Map::Minimap
