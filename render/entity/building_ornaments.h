#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>

namespace Render::GL {

// Deterministic, subtle per-part value variation so large flat surfaces read as
// weathered masonry/timber instead of a single flat fill. `seed` should differ
// per part (e.g. a loop index); the result stays close to the input color.
inline auto
weathered(const QVector3D& color, int seed, float amount = 0.06F) -> QVector3D {
  const float n = std::sin(static_cast<float>(seed) * 12.9898F) * 43758.5453F;
  const float frac = n - std::floor(n);              // [0,1)
  const float delta = (frac - 0.5F) * 2.0F * amount; // [-amount, amount]
  const float scale = 1.0F + delta;
  return QVector3D(std::clamp(color.x() * scale, 0.0F, 1.0F),
                   std::clamp(color.y() * scale, 0.0F, 1.0F),
                   std::clamp(color.z() * scale, 0.0F, 1.0F));
}

template <typename AddBox>
void add_merlon_strip_x(AddBox&& add_box,
                        float y,
                        float z,
                        float start_x,
                        float spacing,
                        int count,
                        const QVector3D& half_size,
                        const QVector3D& color) {
  for (int i = 0; i < count; ++i) {
    add_box(
        QVector3D(start_x + spacing * static_cast<float>(i), y, z), half_size, color);
  }
}

template <typename AddBox>
void add_merlon_strip_z(AddBox&& add_box,
                        float x,
                        float y,
                        float start_z,
                        float spacing,
                        int count,
                        const QVector3D& half_size,
                        const QVector3D& color) {
  for (int i = 0; i < count; ++i) {
    add_box(
        QVector3D(x, y, start_z + spacing * static_cast<float>(i)), half_size, color);
  }
}

template <typename AddBox>
void add_tile_rows_z(AddBox&& add_box,
                     float y,
                     float start_z,
                     float end_z,
                     float spacing,
                     const QVector3D& half_size,
                     const QVector3D& color) {
  if (spacing == 0.0F) {
    return;
  }

  const float step = std::fabs(spacing);
  const float direction = (end_z >= start_z) ? 1.0F : -1.0F;
  for (float z = start_z;
       (direction > 0.0F) ? (z <= end_z + 0.001F) : (z >= end_z - 0.001F);
       z += direction * step) {
    add_box(QVector3D(0.0F, y, z), half_size, color);
  }
}

// Builds a two-slope (gable) roof whose ridge runs along the X axis. All size
// parameters are half-extents (matching add_box / box_local_model, since the
// unit cube spans [-1,1]). The roof sits on a footprint of half-width
// `half_span_x` (X, the ridge length) and half-depth `half_depth_z` (Z, the
// direction the slopes fall), rising `rise` above `eave_y`. `add_rot` is a
// callable (center, scale, euler_deg, color) forwarding to add_rotated_box.
template <typename AddRotatedBox>
void add_gable_roof_x(AddRotatedBox&& add_rot,
                      float center_x,
                      float center_z,
                      float eave_y,
                      float half_span_x,
                      float half_depth_z,
                      float rise,
                      float half_thick,
                      const QVector3D& color,
                      float overhang = 0.0F) {
  const float theta = std::atan2(rise, half_depth_z);
  const float theta_deg = theta * 180.0F / 3.14159265F;
  const float slope = std::sqrt(half_depth_z * half_depth_z + rise * rise);
  const float half_len = slope * 0.5F + overhang;
  const QVector3D scale(half_span_x + overhang, half_thick, half_len);
  const float cy = eave_y + rise * 0.5F;
  add_rot(QVector3D(center_x, cy, center_z + half_depth_z * 0.5F),
          scale,
          QVector3D(theta_deg, 0.0F, 0.0F),
          color);
  add_rot(QVector3D(center_x, cy, center_z - half_depth_z * 0.5F),
          scale,
          QVector3D(-theta_deg, 0.0F, 0.0F),
          color);
}

// Same as above but the ridge runs along the Z axis (slopes fall along X).
template <typename AddRotatedBox>
void add_gable_roof_z(AddRotatedBox&& add_rot,
                      float center_x,
                      float center_z,
                      float eave_y,
                      float half_span_z,
                      float half_depth_x,
                      float rise,
                      float half_thick,
                      const QVector3D& color,
                      float overhang = 0.0F) {
  const float theta = std::atan2(rise, half_depth_x);
  const float theta_deg = theta * 180.0F / 3.14159265F;
  const float slope = std::sqrt(half_depth_x * half_depth_x + rise * rise);
  const float half_len = slope * 0.5F + overhang;
  const QVector3D scale(half_len, half_thick, half_span_z + overhang);
  const float cy = eave_y + rise * 0.5F;
  add_rot(QVector3D(center_x + half_depth_x * 0.5F, cy, center_z),
          scale,
          QVector3D(0.0F, 0.0F, -theta_deg),
          color);
  add_rot(QVector3D(center_x - half_depth_x * 0.5F, cy, center_z),
          scale,
          QVector3D(0.0F, 0.0F, theta_deg),
          color);
}

} // namespace Render::GL