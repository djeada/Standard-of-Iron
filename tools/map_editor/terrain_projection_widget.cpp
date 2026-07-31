#include "terrain_projection_widget.h"

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
constexpr QColor k_entrance_color(31, 139, 245, 210);
constexpr QColor k_overlap_color(210, 121, 255, 220);
constexpr QColor k_mountain_body_color(100, 110, 130, 95);
constexpr QColor k_rim_hint_color(126, 148, 160, 130);
constexpr QColor k_entrance_outline_color(8, 20, 30, 235);
constexpr int k_max_brush_cells = 12;
constexpr int k_entrance_body_reach_cells = 2;
constexpr double k_min_entrance_marker_px = 5.0;

} // namespace

TerrainProjectionWidget::TerrainProjectionWidget(QWidget* parent)
    : QWidget(parent) {
  setMinimumSize(260, 260);
  setMouseTracking(true);
}

void TerrainProjectionWidget::set_map_context(
    const HillProjection::MapContext& context) {
  m_map_context = context;
}

void TerrainProjectionWidget::set_terrain_json(const QJsonObject& json) {
  m_active = true;
  m_drag_mode = DragMode::None;
  m_body_cells.clear();
  m_entrance_cells.clear();
  m_last_drag_cell = QPoint();
  m_rim_dirty = true;

  m_model = HillProjection::build_model(json, m_map_context);
  for (const QPoint& cell : m_model.hill_cells) {
    m_body_cells.insert(encode_cell(cell));
  }
  for (const QPoint& cell : m_model.entrance_cells) {
    m_entrance_cells.insert(encode_cell(cell));
  }

  update();
}

void TerrainProjectionWidget::set_active_layer(int index) {
  m_active_layer = index;
  update();
}

void TerrainProjectionWidget::set_brush_size(int cells) {
  m_brush_size = std::clamp(cells, 1, k_max_brush_cells);
}

QVector<QPoint> TerrainProjectionWidget::body_cells() const {
  QVector<QPoint> cells;
  cells.reserve(m_body_cells.size());
  for (quint64 const encoded : m_body_cells) {
    cells.append(decode_cell(encoded));
  }
  std::sort(cells.begin(), cells.end(), [](const QPoint& lhs, const QPoint& rhs) {
    return lhs.y() != rhs.y() ? lhs.y() < rhs.y() : lhs.x() < rhs.x();
  });
  return cells;
}

QVector<QPoint> TerrainProjectionWidget::entrance_cells() const {
  QVector<QPoint> cells;
  cells.reserve(m_entrance_cells.size());
  for (quint64 const encoded : m_entrance_cells) {
    cells.append(decode_cell(encoded));
  }
  std::sort(cells.begin(), cells.end(), [](const QPoint& lhs, const QPoint& rhs) {
    return lhs.y() != rhs.y() ? lhs.y() < rhs.y() : lhs.x() < rhs.x();
  });
  return cells;
}

QStringList TerrainProjectionWidget::issues() const {
  if (!m_active) {
    return {};
  }
  return HillProjection::entrance_issues(m_model, body_cells(), entrance_cells());
}

const QSet<quint64>& TerrainProjectionWidget::rim_cells() const {
  if (m_rim_dirty) {
    m_rim_cells.clear();
    for (const QPoint& cell : HillProjection::rim_cells(body_cells())) {
      m_rim_cells.insert(encode_cell(cell));
    }
    m_rim_dirty = false;
  }
  return m_rim_cells;
}

void TerrainProjectionWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event)

  QPainter painter(this);
  painter.fillRect(rect(), k_background_color);

  if (!m_active) {
    painter.setPen(QColor(160, 175, 182));
    painter.drawText(
        rect(), Qt::AlignCenter, "Projection is available for this terrain.");
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

  QColor body_color = k_mountain_body_color;
  const int body_idx = body_layer_index();
  const auto defs = layer_definitions();
  if (body_idx >= 0 && body_idx < defs.size()) {
    body_color = defs[body_idx].second;
  }

  const double cell_inset = std::min(1.0, geometry.cell_size * 0.18);
  const auto cell_rect_at = [this, &geometry](const QPoint& cell) {
    return cell_rect(geometry, cell);
  };

  for (quint64 const encoded : m_body_cells) {
    painter.fillRect(cell_rect_at(decode_cell(encoded))
                         .adjusted(cell_inset, cell_inset, -cell_inset, -cell_inset),
                     body_color);
  }

  if (m_active_layer == entrance_layer_index()) {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(k_rim_hint_color, 1));
    for (quint64 const encoded : rim_cells()) {
      painter.drawRect(
          cell_rect_at(decode_cell(encoded)).adjusted(0.5, 0.5, -0.5, -0.5));
    }
  }

  const double entrance_size =
      std::max(k_min_entrance_marker_px, geometry.cell_size - (cell_inset * 2.0));
  for (quint64 const encoded : m_entrance_cells) {
    const QRectF cell_rect = cell_rect_at(decode_cell(encoded));
    QRectF marker(0.0, 0.0, entrance_size, entrance_size);
    marker.moveCenter(cell_rect.center());

    painter.setPen(QPen(k_entrance_outline_color, 1.0));
    painter.setBrush(m_body_cells.contains(encoded) ? k_entrance_color
                                                    : k_overlap_color);
    painter.drawRect(marker);
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
  const QColor grid_color =
      geometry.cell_size >= 4.0 ? QColor(79, 106, 117, 140) : QColor(79, 106, 117, 80);
  painter.setPen(QPen(grid_color, 1));
  for (int x = 0; x <= m_model.grid_width; ++x) {
    const double px = geometry.rect.left() + (geometry.cell_size * x);
    painter.drawLine(QPointF(px, geometry.rect.top()),
                     QPointF(px, geometry.rect.bottom()));
  }
  for (int y = 0; y <= m_model.grid_height; ++y) {
    const double py = geometry.rect.top() + (geometry.cell_size * y);
    painter.drawLine(QPointF(geometry.rect.left(), py),
                     QPointF(geometry.rect.right(), py));
  }
}

void TerrainProjectionWidget::mousePressEvent(QMouseEvent* event) {
  if (!m_active) {
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

  const QSet<quint64> before_body = m_body_cells;
  const QSet<quint64> before_entrance = m_entrance_cells;
  m_drag_mode = (event->button() == Qt::LeftButton) ? DragMode::Paint : DragMode::Erase;
  m_last_drag_cell = *cell;
  m_stroke_changed = false;
  m_stroke_rejected = false;
  stamp_brush(*cell, m_drag_mode == DragMode::Paint);

  if (before_body != m_body_cells || before_entrance != m_entrance_cells) {
    m_stroke_changed = true;
    update();
  }

  event->accept();
}

void TerrainProjectionWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!m_active || m_drag_mode == DragMode::None) {
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

  const QSet<quint64> before_body = m_body_cells;
  const QSet<quint64> before_entrance = m_entrance_cells;
  apply_line(m_last_drag_cell, *cell);
  m_last_drag_cell = *cell;

  if (before_body != m_body_cells || before_entrance != m_entrance_cells) {
    m_stroke_changed = true;
    update();
  }

  event->accept();
}

void TerrainProjectionWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
    m_drag_mode = DragMode::None;
    finish_stroke();
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void TerrainProjectionWidget::finish_stroke() {
  if (m_stroke_rejected) {
    m_stroke_rejected = false;
    emit entrance_rejected(
        QStringLiteral("Entrance cells have to touch the hill — paint on or beside "
                       "the outlined edge ring."));
  }
  if (m_stroke_changed) {
    m_stroke_changed = false;
    emit projection_changed();
  }
}

TerrainProjectionWidget::GridGeometry
TerrainProjectionWidget::compute_geometry() const {
  GridGeometry geometry;
  if (!m_active) {
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

QRectF TerrainProjectionWidget::cell_rect(const GridGeometry& geometry,
                                          const QPoint& cell) const {
  const int column = m_model.grid_width - 1 - cell.x();
  const int row = m_model.grid_height - 1 - cell.y();
  return {geometry.rect.left() + (geometry.cell_size * column),
          geometry.rect.top() + (geometry.cell_size * row),
          geometry.cell_size,
          geometry.cell_size};
}

std::optional<QPoint>
TerrainProjectionWidget::cell_from_position(const QPoint& position) const {
  const GridGeometry geometry = compute_geometry();
  if (!geometry.valid || !geometry.rect.contains(position)) {
    return std::nullopt;
  }

  const int column = static_cast<int>(
      std::floor((position.x() - geometry.rect.left()) / geometry.cell_size));
  const int row = static_cast<int>(
      std::floor((position.y() - geometry.rect.top()) / geometry.cell_size));
  const int cell_x = m_model.grid_width - 1 - column;
  const int cell_y = m_model.grid_height - 1 - row;
  if (cell_x < 0 || cell_x >= m_model.grid_width || cell_y < 0 ||
      cell_y >= m_model.grid_height) {
    return std::nullopt;
  }

  return QPoint(cell_x, cell_y);
}

void TerrainProjectionWidget::apply_line(const QPoint& from, const QPoint& to) {
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
    stamp_brush(QPoint(x0, y0), m_drag_mode == DragMode::Paint);
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

bool TerrainProjectionWidget::reaches_body(const QPoint& cell) const {
  for (int offset_z = -k_entrance_body_reach_cells;
       offset_z <= k_entrance_body_reach_cells;
       ++offset_z) {
    for (int offset_x = -k_entrance_body_reach_cells;
         offset_x <= k_entrance_body_reach_cells;
         ++offset_x) {
      if (m_body_cells.contains(
              encode_cell(QPoint(cell.x() + offset_x, cell.y() + offset_z)))) {
        return true;
      }
    }
  }
  return false;
}

void TerrainProjectionWidget::set_entrance_cells(const QVector<QPoint>& cells) {
  QSet<quint64> updated;
  updated.reserve(cells.size());
  for (const QPoint& cell : cells) {
    updated.insert(encode_cell(cell));
  }
  if (updated == m_entrance_cells) {
    return;
  }
  m_entrance_cells = updated;
  update();
}

void TerrainProjectionWidget::stamp_brush(const QPoint& cell, bool marked) {
  const int radius = (std::clamp(m_brush_size, 1, k_max_brush_cells) - 1) / 2;
  for (int offset_z = -radius; offset_z <= radius; ++offset_z) {
    for (int offset_x = -radius; offset_x <= radius; ++offset_x) {
      set_cell_marked(QPoint(cell.x() + offset_x, cell.y() + offset_z), marked);
    }
  }
}

void TerrainProjectionWidget::set_cell_marked(const QPoint& cell, bool marked) {
  if (cell.x() < 0 || cell.x() >= m_model.grid_width || cell.y() < 0 ||
      cell.y() >= m_model.grid_height) {
    return;
  }

  const quint64 encoded = encode_cell(cell);
  const auto defs = layer_definitions();

  if (m_active_layer >= 0 && m_active_layer < defs.size() &&
      defs[m_active_layer].first == QStringLiteral("Nothing")) {
    if (body_cells_user_editable()) {
      m_body_cells.remove(encoded);
      m_rim_dirty = true;
    }
    m_entrance_cells.remove(encoded);
    return;
  }

  if (m_active_layer == body_layer_index() && body_cells_user_editable()) {
    if (marked) {
      m_body_cells.insert(encoded);
    } else {
      m_body_cells.remove(encoded);
    }
    m_rim_dirty = true;
  } else if (m_active_layer == entrance_layer_index()) {
    if (!marked) {
      m_entrance_cells.remove(encoded);
      return;
    }
    if (!reaches_body(cell)) {
      m_stroke_rejected = true;
      return;
    }
    m_entrance_cells.insert(encoded);
  }
}

quint64 TerrainProjectionWidget::encode_cell(const QPoint& cell) {
  return (static_cast<quint64>(static_cast<quint32>(cell.y())) << 32U) |
         static_cast<quint32>(cell.x());
}

QPoint TerrainProjectionWidget::decode_cell(quint64 encoded) {
  const int x = static_cast<int>(encoded & 0xFFFFFFFFULL);
  const int y = static_cast<int>((encoded >> 32U) & 0xFFFFFFFFULL);
  return {x, y};
}

} // namespace MapEditor
