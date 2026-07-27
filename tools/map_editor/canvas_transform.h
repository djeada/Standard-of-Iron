#pragma once

#include <QPoint>
#include <QPointF>

#include <algorithm>
#include <array>

#include "map_data.h"

namespace MapEditor {

struct CanvasTransform {

  [[nodiscard]] static auto grid_step_for_spacing(float scaled_cell_size,
                                                  float min_spacing_px,
                                                  int min_step) -> int {
    static constexpr std::array<int, 13> k_steps = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000};
    if (scaled_cell_size <= 0.0F) {
      return k_steps.back();
    }
    for (int step : k_steps) {
      if (step >= min_step &&
          static_cast<float>(step) * scaled_cell_size >= min_spacing_px) {
        return step;
      }
    }
    return k_steps.back();
  }

  [[nodiscard]] static auto widget_to_grid(const QPoint& widget_pos,
                                           const GridSettings& grid,
                                           float zoom,
                                           const QPointF& pan_offset,
                                           float cell_size) -> QPointF {
    const float scaled_cell = std::max(cell_size * zoom, 0.0001F);
    const float view_x =
        (static_cast<float>(widget_pos.x()) - pan_offset.x()) / scaled_cell;
    const float view_z =
        (static_cast<float>(widget_pos.y()) - pan_offset.y()) / scaled_cell;

    return QPointF(static_cast<float>(grid.width) - view_x,
                   static_cast<float>(grid.height) - view_z);
  }

  [[nodiscard]] static auto grid_to_widget(float grid_x,
                                           float grid_z,
                                           const GridSettings& grid,
                                           float zoom,
                                           const QPointF& pan_offset,
                                           float cell_size) -> QPoint {
    const float scaled_cell = cell_size * zoom;
    const float view_x = static_cast<float>(grid.width) - grid_x;
    const float view_z = static_cast<float>(grid.height) - grid_z;
    const float x = view_x * scaled_cell + pan_offset.x();
    const float y = view_z * scaled_cell + pan_offset.y();
    return QPoint(static_cast<int>(x), static_cast<int>(y));
  }
};

} // namespace MapEditor
