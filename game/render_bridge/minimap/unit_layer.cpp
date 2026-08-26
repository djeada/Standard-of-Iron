#include "unit_layer.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {

constexpr float k_minor_radius_scale = 0.65F;
constexpr int k_minor_alpha = 180;
constexpr float k_stronghold_scale = 1.86F;
constexpr float k_tower_scale = 0.85F;
constexpr float k_stronghold_pen_width = 1.5F;
constexpr float k_capture_ring_gap = 3.6F;
constexpr float k_capture_ring_width = 2.0F;
constexpr int k_arc_units_per_degree = 16;
constexpr int k_arc_start_degrees = 90;
constexpr int k_full_circle_degrees = 360;
constexpr int k_neutral_fill_alpha = 245;
constexpr float k_ink_shadow_offset = 0.9F;
constexpr int k_minor_ink_blend = 55;

constexpr auto by_owner = [](const auto& lhs, const auto& rhs) -> bool {
  return lhs.owner_id < rhs.owner_id;
};

auto is_neutral(int owner_id) -> bool {
  return owner_id <= 0;
}

auto blend_to_ink(std::uint8_t channel, std::uint8_t ink) -> int {
  return (static_cast<int>(channel) * (100 - k_minor_ink_blend) +
          static_cast<int>(ink) * k_minor_ink_blend) /
         100;
}

} // namespace

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

auto UnitLayer::stronghold_half_size() const -> float {
  return m_building_half_size * k_stronghold_scale;
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

  m_minor_structures.clear();
  m_structures.clear();
  m_strongholds.clear();
  m_troops.clear();
  m_selected.clear();

  const float cull_margin =
      std::max(m_unit_radius,
               stronghold_half_size() + k_capture_ring_gap + k_capture_ring_width) +
      3.0F;
  const float min_px = -cull_margin;
  const float min_py = -cull_margin;
  const float max_px = static_cast<float>(m_width) + cull_margin;
  const float max_py = static_cast<float>(m_height) + cull_margin;

  float bounds_left = max_px;
  float bounds_top = max_py;
  float bounds_right = min_px;
  float bounds_bottom = min_py;

  for (const auto& marker : markers) {

    const bool always_charted = marker.marker_class == MarkerClass::Stronghold;
    if (!always_charted && visibility_check && marker.owner_id != local_owner_id &&
        local_owner_id > 0) {
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

    const PlacedMarker placed{px, py, marker.owner_id, &marker};

    if (marker.is_selected) {
      m_selected.push_back(placed);
      continue;
    }

    switch (marker.marker_class) {
    case MarkerClass::MinorStructure:
      m_minor_structures.push_back(placed);
      break;
    case MarkerClass::Tower:
    case MarkerClass::Landmark:
      m_structures.push_back(placed);
      break;
    case MarkerClass::Stronghold:
      m_strongholds.push_back(placed);
      break;
    case MarkerClass::Troop:
      m_troops.push_back(placed);
      break;
    }
  }

  draw_minor_structures(painter, player_color_fn);
  draw_troops(painter, player_color_fn);
  draw_structures(painter, player_color_fn);
  draw_strongholds(painter, player_color_fn);
  draw_selected(painter, player_color_fn);

  if (bounds_right >= bounds_left && bounds_bottom >= bounds_top) {
    const QRect drawn(QPoint(static_cast<int>(std::floor(bounds_left)),
                             static_cast<int>(std::floor(bounds_top))),
                      QPoint(static_cast<int>(std::ceil(bounds_right)),
                             static_cast<int>(std::ceil(bounds_bottom))));
    m_content_rect = drawn.intersected(m_image.rect());
  }
}

void UnitLayer::draw_minor_structures(QPainter& painter,
                                      const PlayerColorFn& player_color_fn) {
  if (m_minor_structures.empty()) {
    return;
  }
  std::sort(m_minor_structures.begin(), m_minor_structures.end(), by_owner);

  const qreal radius = static_cast<qreal>(m_unit_radius * k_minor_radius_scale);
  painter.setPen(Qt::NoPen);

  int current_owner = m_minor_structures.front().owner_id - 1;
  for (const auto& placed : m_minor_structures) {
    if (placed.owner_id != current_owner) {
      current_owner = placed.owner_id;
      const auto colors = get_color_for_owner(current_owner, player_color_fn);
      painter.setBrush(QColor(blend_to_ink(colors.r, TeamColors::INK_R),
                              blend_to_ink(colors.g, TeamColors::INK_G),
                              blend_to_ink(colors.b, TeamColors::INK_B),
                              k_minor_alpha));
    }
    painter.drawEllipse(
        QPointF(static_cast<qreal>(placed.px), static_cast<qreal>(placed.py)),
        radius,
        radius);
  }
}

void UnitLayer::draw_troops(QPainter& painter, const PlayerColorFn& player_color_fn) {
  if (m_troops.empty()) {
    return;
  }
  std::sort(m_troops.begin(), m_troops.end(), by_owner);

  const qreal radius = static_cast<qreal>(m_unit_radius);

  int current_owner = m_troops.front().owner_id - 1;
  for (const auto& placed : m_troops) {
    if (placed.owner_id != current_owner) {
      current_owner = placed.owner_id;
      const auto colors = get_color_for_owner(current_owner, player_color_fn);
      painter.setBrush(QColor(colors.r, colors.g, colors.b));
      painter.setPen(
          QPen(QColor(colors.border_r, colors.border_g, colors.border_b), 1.1));
    }
    painter.drawEllipse(
        QPointF(static_cast<qreal>(placed.px), static_cast<qreal>(placed.py)),
        radius,
        radius);
  }
}

void UnitLayer::draw_structures(QPainter& painter,
                                const PlayerColorFn& player_color_fn) {
  if (m_structures.empty()) {
    return;
  }
  std::sort(m_structures.begin(), m_structures.end(), by_owner);

  int current_owner = m_structures.front().owner_id - 1;
  for (const auto& placed : m_structures) {
    if (placed.owner_id != current_owner) {
      current_owner = placed.owner_id;
      const auto colors = get_color_for_owner(current_owner, player_color_fn);
      painter.setBrush(QColor(colors.r, colors.g, colors.b));
      painter.setPen(
          QPen(QColor(colors.border_r, colors.border_g, colors.border_b), 1.2));
    }
    if (placed.marker->marker_class == MarkerClass::Tower) {
      draw_tower_shape(painter, placed.px, placed.py);
    } else {
      draw_temple_shape(painter, placed.px, placed.py);
    }
  }
}

void UnitLayer::draw_strongholds(QPainter& painter,
                                 const PlayerColorFn& player_color_fn) {
  for (const auto& placed : m_strongholds) {
    const auto colors = get_color_for_owner(placed.owner_id, player_color_fn);
    draw_stronghold_shape(
        painter, placed.px, placed.py, colors, is_neutral(placed.owner_id));
    draw_capture_ring(painter, placed, player_color_fn);
  }
}

void UnitLayer::draw_selected(QPainter& painter, const PlayerColorFn& player_color_fn) {
  for (const auto& placed : m_selected) {
    draw_selection_halo(painter, placed);

    const auto colors = get_color_for_owner(placed.owner_id, player_color_fn);
    const QColor fill(colors.r, colors.g, colors.b);
    const QColor border(colors.border_r, colors.border_g, colors.border_b);

    switch (placed.marker->marker_class) {
    case MarkerClass::Stronghold:
      draw_stronghold_shape(
          painter, placed.px, placed.py, colors, is_neutral(placed.owner_id));
      draw_capture_ring(painter, placed, player_color_fn);
      break;
    case MarkerClass::Tower:
      painter.setBrush(fill);
      painter.setPen(QPen(border, 1.2));
      draw_tower_shape(painter, placed.px, placed.py);
      break;
    case MarkerClass::Landmark:
    case MarkerClass::MinorStructure:
      painter.setBrush(fill);
      painter.setPen(QPen(border, 1.2));
      draw_temple_shape(painter, placed.px, placed.py);
      break;
    case MarkerClass::Troop:
      painter.setBrush(fill);
      painter.setPen(QPen(border, 1.1));
      painter.drawEllipse(
          QPointF(static_cast<qreal>(placed.px), static_cast<qreal>(placed.py)),
          static_cast<qreal>(m_unit_radius),
          static_cast<qreal>(m_unit_radius));
      break;
    }
  }
}

void UnitLayer::draw_selection_halo(QPainter& painter, const PlacedMarker& placed) {
  const QColor halo(
      TeamColors::SELECT_R, TeamColors::SELECT_G, TeamColors::SELECT_B, 220);
  painter.setBrush(Qt::NoBrush);
  QPen glow_pen(halo);
  glow_pen.setWidthF(1.4);
  painter.setPen(glow_pen);

  const QPointF center(static_cast<qreal>(placed.px), static_cast<qreal>(placed.py));
  if (placed.marker->marker_class == MarkerClass::Troop) {
    painter.drawEllipse(center, m_unit_radius + 1.8, m_unit_radius + 1.8);
    return;
  }

  const qreal half = placed.marker->marker_class == MarkerClass::Stronghold
                         ? static_cast<qreal>(stronghold_half_size())
                         : static_cast<qreal>(m_building_half_size);
  painter.drawRect(QRectF(center.x() - half - 1.8,
                          center.y() - half - 1.8,
                          (half + 1.8) * 2.0,
                          (half + 1.8) * 2.0));
}

void UnitLayer::draw_tower_shape(QPainter& painter, float px, float py) const {
  const auto half = static_cast<qreal>(m_building_half_size * k_tower_scale);
  const qreal cx = static_cast<qreal>(px);
  const qreal cy = static_cast<qreal>(py);
  const QPointF points[7] = {QPointF(cx - half * 0.52, cy + half),
                             QPointF(cx - half * 0.52, cy - half * 0.10),
                             QPointF(cx - half * 0.78, cy - half * 0.10),
                             QPointF(cx, cy - half),
                             QPointF(cx + half * 0.78, cy - half * 0.10),
                             QPointF(cx + half * 0.52, cy - half * 0.10),
                             QPointF(cx + half * 0.52, cy + half)};
  painter.drawPolygon(points, 7);
}

void UnitLayer::draw_temple_shape(QPainter& painter, float px, float py) const {
  const auto half = static_cast<qreal>(m_building_half_size);
  const qreal cx = static_cast<qreal>(px);
  const qreal cy = static_cast<qreal>(py);
  const QPointF points[5] = {QPointF(cx - half * 0.92, cy + half * 0.86),
                             QPointF(cx - half * 0.92, cy - half * 0.12),
                             QPointF(cx, cy - half * 0.98),
                             QPointF(cx + half * 0.92, cy - half * 0.12),
                             QPointF(cx + half * 0.92, cy + half * 0.86)};
  painter.drawPolygon(points, 5);
}

void UnitLayer::draw_keep_outline(QPainter& painter,
                                  float px,
                                  float py,
                                  float half) const {
  QPointF points[k_keep_polygon_points];
  keep_polygon(px, py, half, points);
  painter.drawPolygon(points, k_keep_polygon_points);
}

void UnitLayer::draw_stronghold_shape(QPainter& painter,
                                      float px,
                                      float py,
                                      const TeamColors::ColorSet& colors,
                                      bool neutral) {
  const float half = stronghold_half_size();
  const QColor ink(TeamColors::INK_R, TeamColors::INK_G, TeamColors::INK_B);

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(TeamColors::INK_R, TeamColors::INK_G, TeamColors::INK_B, 78));
  draw_keep_outline(painter, px + k_ink_shadow_offset, py + k_ink_shadow_offset, half);

  QColor fill(colors.r, colors.g, colors.b);
  if (neutral) {
    fill.setAlpha(k_neutral_fill_alpha);
  }
  QColor border(colors.border_r, colors.border_g, colors.border_b);
  if (neutral) {
    border = ink;
  }

  painter.setBrush(fill);
  painter.setPen(QPen(border, k_stronghold_pen_width));
  draw_keep_outline(painter, px, py, half);
}

void UnitLayer::draw_capture_ring(QPainter& painter,
                                  const PlacedMarker& placed,
                                  const PlayerColorFn& player_color_fn) {
  const UnitMarker& marker = *placed.marker;
  if (marker.capture_step == 0) {
    return;
  }

  const qreal radius = static_cast<qreal>(stronghold_half_size()) + k_capture_ring_gap;
  const QRectF rect(static_cast<qreal>(placed.px) - radius,
                    static_cast<qreal>(placed.py) - radius,
                    radius * 2.0,
                    radius * 2.0);

  QColor ring_color;
  if (marker.contested) {
    ring_color = QColor(
        TeamColors::CONTESTED_R, TeamColors::CONTESTED_G, TeamColors::CONTESTED_B);
  } else {
    const auto colors = get_color_for_owner(marker.capture_owner_id, player_color_fn);
    ring_color = QColor(colors.r, colors.g, colors.b);
  }

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(QColor(20, 16, 12, 150), k_capture_ring_width + 1.0F));
  painter.drawEllipse(rect);

  QPen progress_pen(ring_color, k_capture_ring_width);
  progress_pen.setCapStyle(Qt::FlatCap);
  painter.setPen(progress_pen);

  const int span_degrees =
      (k_full_circle_degrees * marker.capture_step) / static_cast<int>(k_capture_steps);
  painter.drawArc(rect,
                  k_arc_start_degrees * k_arc_units_per_degree,
                  -span_degrees * k_arc_units_per_degree);
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

} // namespace Game::Map::Minimap
