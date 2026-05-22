#pragma once

#include <QVector3D>

#include <cmath>

namespace Render::GL {

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

} // namespace Render::GL
