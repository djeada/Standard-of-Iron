#include "hill_projection_widget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace MapEditor {

namespace {

constexpr double k_margin = 12.0;
constexpr QColor k_background_color(7, 16, 24);
constexpr QColor k_grid_background_color(12, 30, 42);
constexpr QColor k_grid_border_color(31, 139, 245);
constexpr QColor k_hill_color(153, 121, 76, 95);
constexpr QColor k_entrance_color(31, 139, 245, 210);
constexpr QColor k_overlap_color(210, 121, 255, 220);

} // namespace

HillProjectionWidget::HillProjectionWidget(QWidget* parent)
    : QWidget(parent) {
  setMinimumSize(260, 260);
  setMouseTracking(true);
}

void HillProjectionWidget::set_hill_json(const QJsonObject& hill_json) {
  m_hill_active = true;
  m_drag_mode = DragMode::None;
  m_hill_cells.clear();
  m_entrance_cells.clear();
  m_last_drag_cell = QPoint();

  m_model = HillProjection::build_model(hill_json);
  for (const QPoint& cell : m_model.hill_cells) {
    m_hill_cells.insert(encode_cell(cell));
  }
  for (const QPoint& cell : m_model.entrance_cells) {
    m_entrance_cells.insert(encode_cell(cell));
  }

  update();
}

auto HillProjectionWidget::hill_cells() const -> QVector<QPoint> {
  QVector<QPoint> cells;
  cells.reserve(m_hill_cells.size());
  for (quint64 encoded : m_hill_cells) {
    cells.append(decode_cell(encoded));
  }
  std::sort(cells.begin(), cells.end(), [](const QPoint& lhs, const QPoint& rhs) {
    if (lhs.y() == rhs.y()) {
      return lhs.x() < rhs.x();
    }
    return lhs.y() < rhs.y();
  });
  return cells;
}

auto HillProjectionWidget::entrance_cells() const -> QVector<QPoint> {
  QVector<QPoint> cells;
  cells.reserve(m_entrance_cells.size());
  for (quint64 encoded : m_entrance_cells) {
    cells.append(decode_cell(encoded));
  }
  std::sort(cells.begin(), cells.end(), [](const QPoint& lhs, const QPoint& rhs) {
    if (lhs.y() == rhs.y()) {
      return lhs.x() < rhs.x();
    }
    return lhs.y() < rhs.y();
  });
  return cells;
}

void HillProjectionWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.fillRect(rect(), k_background_color);

  if (!m_hill_active) {
    painter.setPen(QColor(160, 175, 182));
    painter.drawText(rect(), Qt::AlignCenter, "Projection is available for hills.");
    return;
  }

  const GridGeometry geometry = compute_geometry();
  if (!geometry.valid) {
    painter.setPen(QColor(160, 175, 182));
    painter.drawText(rect(), Qt::AlignCenter, "Resize this panel to edit entrances.");
    return;
  }

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(geometry.rect, k_grid_background_color);
  painter.setPen(QPen(k_grid_border_color, 1));
  painter.drawRect(geometry.rect);

  for (quint64 encoded : m_hill_cells) {
    const QPoint cell = decode_cell(encoded);
    const QRectF cell_rect(geometry.rect.left() + geometry.cell_size * cell.x(),
                           geometry.rect.top() + geometry.cell_size * cell.y(),
                           geometry.cell_size,
                           geometry.cell_size);
    painter.fillRect(cell_rect.adjusted(1.0, 1.0, -1.0, -1.0), k_hill_color);
  }

  for (quint64 encoded : m_entrance_cells) {
    const QPoint cell = decode_cell(encoded);
    const QRectF cell_rect(geometry.rect.left() + geometry.cell_size * cell.x(),
                           geometry.rect.top() + geometry.cell_size * cell.y(),
                           geometry.cell_size,
                           geometry.cell_size);
    const bool overlaps_hill = m_hill_cells.contains(encoded);
    painter.fillRect(cell_rect.adjusted(1.0, 1.0, -1.0, -1.0),
                     overlaps_hill ? k_overlap_color : k_entrance_color);
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
  const QColor grid_color =
      geometry.cell_size >= 4.0 ? QColor(79, 106, 117, 140) : QColor(79, 106, 117, 80);
  painter.setPen(QPen(grid_color, 1));
  for (int x = 0; x <= m_model.grid_width; ++x) {
    const double px = geometry.rect.left() + (geometry.cell_size * x);
    painter.drawLine(QPointF(px, geometry.rect.top()), QPointF(px, geometry.rect.bottom()));
  }
  for (int y = 0; y <= m_model.grid_height; ++y) {
    const double py = geometry.rect.top() + (geometry.cell_size * y);
    painter.drawLine(QPointF(geometry.rect.left(), py), QPointF(geometry.rect.right(), py));
  }
}

void HillProjectionWidget::mousePressEvent(QMouseEvent* event) {
  if (!m_hill_active) {
    QWidget::mousePressEvent(event);
    return;
  }

  if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  const std::optional<QPoint> cell = cell_from_position(event->pos());
  if (!cell.has_value()) {
    QWidget::mousePressEvent(event);
    return;
  }

  const QSet<quint64> before_hill = m_hill_cells;
  const QSet<quint64> before_entrance = m_entrance_cells;
  m_drag_mode = (event->button() == Qt::LeftButton) ? DragMode::Paint : DragMode::Erase;
  m_last_drag_cell = *cell;
  set_cell_marked(*cell, m_drag_mode == DragMode::Paint);

  if (before_hill != m_hill_cells || before_entrance != m_entrance_cells) {
    emit_projection_changed();
    update();
  }

  event->accept();
}

void HillProjectionWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!m_hill_active || m_drag_mode == DragMode::None) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  const std::optional<QPoint> cell = cell_from_position(event->pos());
  if (!cell.has_value()) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  if (*cell == m_last_drag_cell) {
    event->accept();
    return;
  }

  const QSet<quint64> before_hill = m_hill_cells;
  const QSet<quint64> before_entrance = m_entrance_cells;
  apply_line(m_last_drag_cell, *cell);
  m_last_drag_cell = *cell;

  if (before_hill != m_hill_cells || before_entrance != m_entrance_cells) {
    emit_projection_changed();
    update();
  }

  event->accept();
}

void HillProjectionWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
    m_drag_mode = DragMode::None;
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

auto HillProjectionWidget::compute_geometry() const -> GridGeometry {
  GridGeometry geometry;
  if (!m_hill_active) {
    return geometry;
  }

  const QRectF area = rect().adjusted(k_margin, k_margin, -k_margin, -k_margin);
  if (area.width() <= 4.0 || area.height() <= 4.0) {
    return geometry;
  }

  const double cell_size =
      std::min(area.width() / static_cast<double>(m_model.grid_width),
               area.height() / static_cast<double>(m_model.grid_height));
  if (cell_size <= 0.0) {
    return geometry;
  }

  const double grid_width = cell_size * static_cast<double>(m_model.grid_width);
  const double grid_height = cell_size * static_cast<double>(m_model.grid_height);
  const double left = area.left() + ((area.width() - grid_width) * 0.5);
  const double top = area.top() + ((area.height() - grid_height) * 0.5);

  geometry.rect = QRectF(left, top, grid_width, grid_height);
  geometry.cell_size = cell_size;
  geometry.valid = true;
  return geometry;
}

auto HillProjectionWidget::cell_from_position(const QPoint& position) const
    -> std::optional<QPoint> {
  const GridGeometry geometry = compute_geometry();
  if (!geometry.valid || !geometry.rect.contains(position)) {
    return std::nullopt;
  }

  const int cell_x = static_cast<int>(
      std::floor((position.x() - geometry.rect.left()) / geometry.cell_size));
  const int cell_y = static_cast<int>(
      std::floor((position.y() - geometry.rect.top()) / geometry.cell_size));
  if (cell_x < 0 || cell_x >= m_model.grid_width || cell_y < 0 ||
      cell_y >= m_model.grid_height) {
    return std::nullopt;
  }

  return QPoint(cell_x, cell_y);
}

void HillProjectionWidget::apply_line(const QPoint& from, const QPoint& to) {
  int x0 = from.x();
  int y0 = from.y();
  const int x1 = to.x();
  const int y1 = to.y();

  const int dx = std::abs(x1 - x0);
  const int dy = std::abs(y1 - y0);
  const int step_x = (x0 < x1) ? 1 : -1;
  const int step_y = (y0 < y1) ? 1 : -1;

  int error = dx - dy;
  while (true) {
    set_cell_marked(QPoint(x0, y0), m_drag_mode == DragMode::Paint);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int doubled_error = 2 * error;
    if (doubled_error > -dy) {
      error -= dy;
      x0 += step_x;
    }
    if (doubled_error < dx) {
      error += dx;
      y0 += step_y;
    }
  }
}

void HillProjectionWidget::set_cell_marked(const QPoint& cell, bool marked) {
  const bool in_bounds = cell.x() >= 0 && cell.x() < m_model.grid_width &&
                         cell.y() >= 0 && cell.y() < m_model.grid_height;
  if (!in_bounds) {
    return;
  }

  const quint64 encoded = encode_cell(cell);
  if (m_edit_layer == EditLayer::Nothing) {
    m_hill_cells.remove(encoded);
    m_entrance_cells.remove(encoded);
    return;
  }

  QSet<quint64>* target = (m_edit_layer == EditLayer::Hill) ? &m_hill_cells
                                                             : &m_entrance_cells;
  if (marked) {
    target->insert(encoded);
  } else {
    target->remove(encoded);
  }
}

void HillProjectionWidget::emit_projection_changed() {
  emit projection_changed();
}

auto HillProjectionWidget::encode_cell(const QPoint& cell) -> quint64 {
  return (static_cast<quint64>(static_cast<quint32>(cell.y())) << 32U) |
         static_cast<quint32>(cell.x());
}

auto HillProjectionWidget::decode_cell(quint64 encoded) -> QPoint {
  const int x = static_cast<int>(encoded & 0xFFFFFFFFULL);
  const int y = static_cast<int>((encoded >> 32U) & 0xFFFFFFFFULL);
  return QPoint(x, y);
}

} // namespace MapEditor
