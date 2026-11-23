#pragma once

#include <QPainter>

namespace MapEditor {

inline const QColor k_selection_color(255, 223, 120);
inline const QColor k_grabbed_color(119, 249, 224);
inline const QColor k_handle_background(20, 24, 25);

[[nodiscard]] inline auto move_handle_rect(QPointF anchor,
                                           const QRect& viewport) -> QRectF {
  constexpr double size = 24.0;
  const double dx = anchor.x() + 30.0 <= viewport.right() ? 18.0 : -18.0;
  const double dy = anchor.y() - 40.0 >= viewport.top() ? -28.0 : 28.0;
  return {anchor + QPointF(dx - size * 0.5, dy - size * 0.5), QSizeF(size, size)};
}

inline void paint_move_handle(
    QPainter& painter, QPointF anchor, const QRectF& box, bool active, bool hovered) {
  painter.save();
  const QColor accent = active || hovered ? k_grabbed_color : k_selection_color;
  painter.setPen(QPen(k_handle_background, 4.0));
  painter.drawLine(anchor, box.center());
  painter.setPen(QPen(accent, 1.4));
  painter.drawLine(anchor, box.center());
  painter.setBrush(active ? accent : k_handle_background);
  painter.drawRoundedRect(box, 4.0, 4.0);
  painter.setPen(QPen(active ? k_handle_background : accent,
                      1.5,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin));
  const QPointF c = box.center();
  painter.drawLine(c - QPointF(6, 0), c + QPointF(6, 0));
  painter.drawLine(c - QPointF(0, 6), c + QPointF(0, 6));
  const QPolygonF arrows[] = {
      QPolygonF{c + QPointF(-3, -3), c + QPointF(-6, 0), c + QPointF(-3, 3)},
      QPolygonF{c + QPointF(3, -3), c + QPointF(6, 0), c + QPointF(3, 3)},
      QPolygonF{c + QPointF(-3, -3), c + QPointF(0, -6), c + QPointF(3, -3)},
      QPolygonF{c + QPointF(-3, 3), c + QPointF(0, 6), c + QPointF(3, 3)},
  };
  for (const auto& arrow : arrows) {
    painter.drawPolyline(arrow);
  }
  painter.restore();
}

} // namespace MapEditor
