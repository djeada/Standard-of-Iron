#include "unit_layer.h"
#include "minimap_utils.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace Game::Map::Minimap {

void UnitLayer::init(int width, int height, float world_width,
                     float world_height) {
  m_width = width;
  m_height = height;
  m_world_width = world_width;
  m_world_height = world_height;

  m_scale_x = static_cast<float>(width - 1) / world_width;
  m_scale_y = static_cast<float>(height - 1) / world_height;
  m_offset_x = world_width * 0.5F;
  m_offset_y = world_height * 0.5F;

  m_image = QImage(width, height, QImage::Format_RGBA8888);
  m_image.fill(Qt::transparent);
}

auto UnitLayer::world_to_pixel(float world_x,
                               float world_z) const -> std::pair<float, float> {

  const float rotated_x = world_x * Constants::k_camera_yaw_cos -
                          world_z * Constants::k_camera_yaw_sin;
  const float rotated_z = world_x * Constants::k_camera_yaw_sin +
                          world_z * Constants::k_camera_yaw_cos;

  const float px = (rotated_x + m_offset_x) * m_scale_x;
  const float py = (rotated_z + m_offset_y) * m_scale_y;

  return {px, py};
}

void UnitLayer::update(const std::vector<UnitMarker> &markers) {
  if (m_image.isNull()) {
    return;
  }

  m_image.fill(Qt::transparent);

  if (markers.empty()) {
    return;
  }

  QPainter painter(&m_image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  std::vector<const UnitMarker *> buildings;
  std::vector<const UnitMarker *> units;
  std::vector<const UnitMarker *> selected;

  for (const auto &marker : markers) {
    if (marker.is_selected) {
      selected.push_back(&marker);
    } else if (marker.is_building) {
      buildings.push_back(&marker);
    } else {
      units.push_back(&marker);
    }
  }

  for (const auto *marker : buildings) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = TeamColors::get_color(marker->owner_id);
    draw_building_marker(painter, px, py, colors, false);
  }

  for (const auto *marker : units) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = TeamColors::get_color(marker->owner_id);
    draw_unit_marker(painter, px, py, colors, false);
  }

  for (const auto *marker : selected) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = TeamColors::get_color(marker->owner_id);
    if (marker->is_building) {
      draw_building_marker(painter, px, py, colors, true);
    } else {
      draw_unit_marker(painter, px, py, colors, true);
    }
  }
}

void UnitLayer::draw_unit_marker(QPainter &painter, float px, float py,
                                 const TeamColors::ColorSet &colors,
                                 bool is_selected) {
  const QPointF center(static_cast<qreal>(px), static_cast<qreal>(py));

  if (is_selected) {
    painter.setBrush(Qt::NoBrush);
    QPen glow_pen(QColor(TeamColors::SELECT_R, TeamColors::SELECT_G,
                         TeamColors::SELECT_B, 200));
    glow_pen.setWidthF(2.0);
    painter.setPen(glow_pen);
    painter.drawEllipse(center, m_unit_radius + 2.0, m_unit_radius + 2.0);
  }

  QColor fill_color(colors.r, colors.g, colors.b);
  QColor border_color(colors.border_r, colors.border_g, colors.border_b);

  painter.setBrush(fill_color);
  painter.setPen(QPen(border_color, 1.2));
  painter.drawEllipse(center, m_unit_radius, m_unit_radius);
}

void UnitLayer::draw_building_marker(QPainter &painter, float px, float py,
                                     const TeamColors::ColorSet &colors,
                                     bool is_selected) {
  const qreal half = static_cast<qreal>(m_building_half_size);
  const QRectF rect(px - half, py - half, half * 2.0, half * 2.0);

  if (is_selected) {
    painter.setBrush(Qt::NoBrush);
    QPen glow_pen(QColor(TeamColors::SELECT_R, TeamColors::SELECT_G,
                         TeamColors::SELECT_B, 200));
    glow_pen.setWidthF(2.5);
    painter.setPen(glow_pen);
    painter.drawRect(rect.adjusted(-2.5, -2.5, 2.5, 2.5));
  }

  QColor fill_color(colors.r, colors.g, colors.b);
  QColor border_color(colors.border_r, colors.border_g, colors.border_b);

  painter.setBrush(fill_color);
  painter.setPen(QPen(border_color, 1.5));
  painter.drawRect(rect);

  const qreal inner = half * 0.4;
  painter.setBrush(border_color);
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRectF(px - inner, py - inner, inner * 2.0, inner * 2.0));
}

} // namespace Game::Map::Minimap
