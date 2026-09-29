#pragma once

#include <QPointF>

struct ViewportState {
  int width = 0;
  int height = 0;
  qreal input_width = 0.0;
  qreal input_height = 0.0;

  [[nodiscard]] auto map_input(qreal sx, qreal sy) const -> QPointF {
    if (width <= 0 || height <= 0 || input_width <= 0.0 || input_height <= 0.0) {
      return {sx, sy};
    }
    return {sx * (static_cast<qreal>(width) / input_width),
            sy * (static_cast<qreal>(height) / input_height)};
  }
};
