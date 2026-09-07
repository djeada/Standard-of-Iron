#include "edge_scroll.h"

#include <QCursor>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

namespace Ui::EdgeScrollGeometry {

namespace {

auto sane_scale(double value) -> double {
  if (!std::isfinite(value) || value <= 0.0) {
    return 1.0;
  }
  return value;
}

auto approach(double distance, double zone) -> double {
  if (zone <= 0.0) {
    return 0.0;
  }
  return std::clamp(1.0 - (distance / zone), 0.0, 1.0);
}

} // namespace

auto horizontal_zone(double sensitivity, double ui_scale) -> double {
  return std::max(k_min_zone,
                  k_base_horizontal_zone * sane_scale(sensitivity) *
                      sane_scale(ui_scale));
}

auto vertical_zone(double sensitivity, double ui_scale) -> double {
  return std::max(k_min_zone,
                  k_base_vertical_zone * sane_scale(sensitivity) *
                      sane_scale(ui_scale));
}

auto push_ramp(double approach_amount) -> double {
  if (!(approach_amount > 0.0)) {
    return 0.0;
  }
  const double amount = std::min(approach_amount, 1.0);
  return k_entry_push + (1.0 - k_entry_push) * amount * amount;
}

auto vector_at(double x,
               double y,
               double width,
               double height,
               double sensitivity,
               double ui_scale) -> Vector {
  if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0 || y < 0.0) {
    return {};
  }
  if (!(width > 0.0) || !(height > 0.0)) {
    return {};
  }
  if (x > width || y > height) {
    return {};
  }

  const double speed = sane_scale(sensitivity);
  const double horizontal = horizontal_zone(sensitivity, ui_scale);
  const double vertical = vertical_zone(sensitivity, ui_scale);

  const double left = approach(x, horizontal);
  const double right = approach(width - x, horizontal);
  const double up = approach(y, vertical);
  const double down = approach(height - y, vertical);

  double dx = (push_ramp(right) - push_ramp(left)) * speed;
  double dz = (push_ramp(up) - push_ramp(down)) * speed;

  const double magnitude = std::hypot(dx, dz);
  if (magnitude > speed && magnitude > 0.0) {
    const double trim = speed / magnitude;
    dx *= trim;
    dz *= trim;
  }

  return {.dx = dx, .dz = dz};
}

} // namespace Ui::EdgeScrollGeometry

EdgeScroll* EdgeScroll::instance() {
  static EdgeScroll scroll;
  return &scroll;
}

EdgeScroll* EdgeScroll::create(QQmlEngine* engine, QJSEngine* script_engine) {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)
  auto* scroll = instance();
  QQmlEngine::setObjectOwnership(scroll, QQmlEngine::CppOwnership);
  return scroll;
}

qreal EdgeScroll::base_horizontal_zone() {
  return Ui::EdgeScrollGeometry::k_base_horizontal_zone;
}

qreal EdgeScroll::base_vertical_zone() {
  return Ui::EdgeScrollGeometry::k_base_vertical_zone;
}

qreal EdgeScroll::horizontalZone(qreal sensitivity, qreal ui_scale) {
  return Ui::EdgeScrollGeometry::horizontal_zone(sensitivity, ui_scale);
}

qreal EdgeScroll::verticalZone(qreal sensitivity, qreal ui_scale) {
  return Ui::EdgeScrollGeometry::vertical_zone(sensitivity, ui_scale);
}

QPointF EdgeScroll::vector(
    qreal x, qreal y, qreal width, qreal height, qreal sensitivity, qreal ui_scale) {
  const auto result =
      Ui::EdgeScrollGeometry::vector_at(x, y, width, height, sensitivity, ui_scale);
  return {result.dx, result.dz};
}

QPointF EdgeScroll::cursorIn(QQuickItem* item) {
  static const QPointF k_unknown{-1.0, -1.0};
  if (item == nullptr) {
    return k_unknown;
  }
  QQuickWindow* window = item->window();
  if (window == nullptr || !window->isVisible()) {
    return k_unknown;
  }
  const QPointF scene = QPointF(window->mapFromGlobal(QCursor::pos()));
  const QPointF local = item->mapFromScene(scene);
  if (!std::isfinite(local.x()) || !std::isfinite(local.y())) {
    return k_unknown;
  }
  return local;
}
