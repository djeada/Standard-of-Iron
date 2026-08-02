#include "unit_layer.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

void UnitLayer::init(
    int width, int height, float world_width, float world_height, float tile_size) {
  m_width = width;
  m_height = height;
  m_world_width = world_width;
  m_world_height = world_height;
  m_inv_tile_size = 1.0F / std::max(tile_size, Constants::k_min_tile_size);

  m_scale_x = static_cast<float>(width - 1) / world_width;
  m_scale_y = static_cast<float>(height - 1) / world_height;
  m_offset_x = world_width * 0.5F;
  m_offset_y = world_height * 0.5F;

  m_image = QImage(width, height, QImage::Format_ARGB32);
  m_image.fill(Qt::transparent);
  m_content_rect = QRect();
}

auto UnitLayer::world_to_pixel(float world_x,
                               float world_z) const -> std::pair<float, float> {

  const float grid_x = world_x * m_inv_tile_size;
  const float grid_z = world_z * m_inv_tile_size;

  const auto& orient = MinimapOrientation::instance();
  const float rotated_x = grid_x * orient.cos_yaw() - grid_z * orient.sin_yaw();
  const float rotated_z = grid_x * orient.sin_yaw() + grid_z * orient.cos_yaw();

  const float px = (rotated_x + m_offset_x) * m_scale_x;
  const float py = (rotated_z + m_offset_y) * m_scale_y;

  return {px, py};
}

void UnitLayer::update(const std::vector<UnitMarker>& markers) {

  update(markers, 0, nullptr, nullptr);
}

void UnitLayer::update(const std::vector<UnitMarker>& markers,
                       int local_owner_id,
                       const VisibilityCheckFn& visibility_check,
                       const PlayerColorFn& player_color_fn) {
  if (m_image.isNull()) {
    return;
  }

  const QRect previous_content = m_content_rect;
  m_content_rect = QRect();

  if (markers.empty()) {
    if (!previous_content.isEmpty()) {
      QPainter clear_painter(&m_image);
      clear_painter.setCompositionMode(QPainter::CompositionMode_Source);
      clear_painter.fillRect(previous_content, Qt::transparent);
    }
    return;
  }

  QPainter painter(&m_image);
  if (!previous_content.isEmpty()) {
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(previous_content, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  }
  painter.setRenderHint(QPainter::Antialiasing, true);

  auto& buildings = m_buildings;
  auto& units = m_units;
  auto& selected = m_selected;
  buildings.clear();
  units.clear();
  selected.clear();

  const float cull_margin = std::max(m_unit_radius, m_building_half_size) + 3.0F;
  const float min_px = -cull_margin;
  const float min_py = -cull_margin;
  const float max_px = static_cast<float>(m_width) + cull_margin;
  const float max_py = static_cast<float>(m_height) + cull_margin;

  float bounds_left = max_px;
  float bounds_top = max_py;
  float bounds_right = min_px;
  float bounds_bottom = min_py;

  for (const auto& marker : markers) {

    if (visibility_check && marker.owner_id != local_owner_id && local_owner_id > 0) {
      if (!visibility_check(marker.world_x, marker.world_z)) {
        continue;
      }
    }

    const auto [px, py] = world_to_pixel(marker.world_x, marker.world_z);
    if (px < min_px || px > max_px || py < min_py || py > max_py) {
      continue;
    }

    bounds_left = std::min(bounds_left, px - cull_margin);
    bounds_top = std::min(bounds_top, py - cull_margin);
    bounds_right = std::max(bounds_right, px + cull_margin);
    bounds_bottom = std::max(bounds_bottom, py + cull_margin);

    if (marker.is_selected) {
      selected.push_back(&marker);
    } else if (marker.is_building) {
      buildings.push_back(&marker);
    } else {
      units.push_back(&marker);
    }
  }

  for (const auto* marker : buildings) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = get_color_for_owner(marker->owner_id, player_color_fn);
    draw_building_marker(painter, px, py, colors, false);
  }

  for (const auto* marker : units) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = get_color_for_owner(marker->owner_id, player_color_fn);
    draw_unit_marker(painter, px, py, colors, false);
  }

  for (const auto* marker : selected) {
    const auto [px, py] = world_to_pixel(marker->world_x, marker->world_z);
    const auto colors = get_color_for_owner(marker->owner_id, player_color_fn);
    if (marker->is_building) {
      draw_building_marker(painter, px, py, colors, true);
    } else {
      draw_unit_marker(painter, px, py, colors, true);
    }
  }

  if (bounds_right >= bounds_left && bounds_bottom >= bounds_top) {
    const QRect drawn(QPoint(static_cast<int>(std::floor(bounds_left)),
                             static_cast<int>(std::floor(bounds_top))),
                      QPoint(static_cast<int>(std::ceil(bounds_right)),
                             static_cast<int>(std::ceil(bounds_bottom))));
    m_content_rect = drawn.intersected(m_image.rect());
  }
}

auto UnitLayer::get_color_for_owner(int owner_id, const PlayerColorFn& player_color_fn)
    -> TeamColors::ColorSet {

  if (player_color_fn) {
    std::uint8_t r, g, b;
    if (player_color_fn(owner_id, r, g, b)) {

      return TeamColors::ColorSet{r,
                                  g,
                                  b,
                                  static_cast<std::uint8_t>(r / 2),
                                  static_cast<std::uint8_t>(g / 2),
                                  static_cast<std::uint8_t>(b / 2)};
    }
  }

  return TeamColors::get_color(owner_id);
}

void UnitLayer::draw_unit_marker(QPainter& painter,
                                 float px,
                                 float py,
                                 const TeamColors::ColorSet& colors,
                                 bool is_selected) {
  const QPointF center(static_cast<qreal>(px), static_cast<qreal>(py));

  if (is_selected) {
    painter.setBrush(Qt::NoBrush);
    QPen glow_pen(
        QColor(TeamColors::SELECT_R, TeamColors::SELECT_G, TeamColors::SELECT_B, 220));
    glow_pen.setWidthF(1.4);
    painter.setPen(glow_pen);
    painter.drawEllipse(center, m_unit_radius + 1.8, m_unit_radius + 1.8);
  }

  QColor fill_color(colors.r, colors.g, colors.b);
  QColor border_color(colors.border_r, colors.border_g, colors.border_b);

  painter.setBrush(fill_color);
  painter.setPen(QPen(border_color, 1.1));
  painter.drawEllipse(center, m_unit_radius, m_unit_radius);
}

void UnitLayer::draw_building_marker(QPainter& painter,
                                     float px,
                                     float py,
                                     const TeamColors::ColorSet& colors,
                                     bool is_selected) {
  const qreal half = static_cast<qreal>(m_building_half_size);
  const QRectF rect(px - half, py - half, half * 2.0, half * 2.0);

  if (is_selected) {
    painter.setBrush(Qt::NoBrush);
    QPen glow_pen(
        QColor(TeamColors::SELECT_R, TeamColors::SELECT_G, TeamColors::SELECT_B, 220));
    glow_pen.setWidthF(1.4);
    painter.setPen(glow_pen);
    painter.drawRect(rect.adjusted(-1.8, -1.8, 1.8, 1.8));
  }

  QColor fill_color(colors.r, colors.g, colors.b);
  QColor border_color(colors.border_r, colors.border_g, colors.border_b);

  painter.setBrush(fill_color);
  painter.setPen(QPen(border_color, 1.2));
  painter.drawRect(rect);
}

} // namespace Game::Map::Minimap
