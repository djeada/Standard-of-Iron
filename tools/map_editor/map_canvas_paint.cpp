#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QSizeF>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "canvas_transform.h"
#include "commander_preview.h"
#include "element_ops.h"
#include "map_canvas.h"
#include "map_canvas_internal.h"
#include "map_json_keys.h"
#include "spawn_icon_library.h"
#include "tool_catalog.h"
#include "ui/theme.h"
#include "wall_geometry.h"

namespace MapEditor {

using CanvasDetail::default_terrain_element;
using CanvasDetail::default_wildlife_radius;

namespace {

const QColor k_canvas_background = Theme::backgroundDeep();
const QColor k_canvas_border = Theme::borderSubtle();
const QColor k_grid_line_color = Theme::panelIron();
const QColor k_grid_outline_color = Theme::borderSubtle();
const QColor k_grid_text_color = Theme::textSecondary();
const QColor k_empty_state_text(159, 217, 255);
constexpr int k_base_grid_step = 10;
constexpr float k_major_grid_spacing_px = 48.0F;
constexpr float k_minor_grid_spacing_px = 12.0F;
constexpr int k_minor_grid_alpha = 90;
const QColor k_hover_select_color(100, 200, 255);
const QColor k_hover_erase_color(255, 80, 80);
constexpr float k_entry_crest_width_scale = 1.12F;

auto is_mask_hill(const TerrainElement& elem) -> bool {
  return elem.type == QStringLiteral("hill") && !elem.cells.isEmpty() &&
         elem.shape.trimmed().compare(QStringLiteral("mask"), Qt::CaseInsensitive) == 0;
}

auto terrain_footprint_cells(const TerrainElement& elem) -> float {
  const float extent = std::max(elem.width, elem.depth);
  return std::max(extent > 0.0F ? extent : 0.0F, elem.radius);
}

constexpr std::array<int, 8> k_category_paint_order = {
    static_cast<int>(ElementKind::Terrain),
    static_cast<int>(ElementKind::Forest),
    static_cast<int>(ElementKind::Linear),
    static_cast<int>(ElementKind::WildlifeArea),
    static_cast<int>(ElementKind::WorldProp),
    static_cast<int>(ElementKind::Structure),
    static_cast<int>(ElementKind::TroopSpawn),
    static_cast<int>(ElementKind::UndeadZone),
};

struct WildlifeAreaStyle {
  QColor fill;
  QColor outline;
  QString glyph;
};

auto wildlife_area_style(const QString& species) -> WildlifeAreaStyle {
  if (species == QLatin1String("wolves")) {
    return {QColor(120, 60, 50, 46), QColor(214, 118, 92, 190), QStringLiteral("W")};
  }
  if (species == QLatin1String("birds")) {
    return {QColor(60, 96, 130, 44), QColor(126, 184, 226, 185), QStringLiteral("∧")};
  }
  return {QColor(190, 190, 170, 48), QColor(226, 224, 190, 195), QStringLiteral("S")};
}

void draw_troop_marker(QPainter& painter,
                       const QPoint& pos,
                       const QString& type,
                       int player_id,
                       float badge_size) {
  const QRectF bounds(
      pos.x() - badge_size * 0.5F, pos.y() - badge_size * 0.5F, badge_size, badge_size);
  paint_troop_badge(painter, bounds, type, player_color_for_editor(player_id));
}

} // namespace

void MapCanvas::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.fillRect(rect(), k_canvas_background);
  painter.setPen(QPen(k_canvas_border, 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  if (m_map_data == nullptr) {
    painter.setPen(k_empty_state_text);
    painter.drawText(rect(), Qt::AlignCenter, "No map loaded");
    return;
  }

  draw_grid(painter);
  draw_fog_zones(painter);

  for (const ElementRef& ref : m_back_order) {
    draw_one_element(painter, ref);
  }
  for (int kind : k_category_paint_order) {
    draw_category(painter, kind);
  }
  draw_mission_overlays(painter);
  for (const ElementRef& ref : m_front_order) {
    draw_one_element(painter, ref);
  }

  draw_linear_preview(painter);
  draw_current_placement(painter);
  draw_rubber_band(painter);

  const bool is_empty =
      m_map_data->terrain_elements().isEmpty() && m_map_data->world_props().isEmpty() &&
      m_map_data->linear_elements().isEmpty() && m_map_data->structures().isEmpty() &&
      m_map_data->troop_spawns().isEmpty() && m_map_data->undead_zones().isEmpty();
  if (is_empty) {
    painter.setOpacity(0.55);
    painter.setPen(k_empty_state_text);
    QFont hint_font = painter.font();
    hint_font.setPointSize(11);
    painter.setFont(hint_font);
    painter.drawText(rect().adjusted(0, 24, 0, 0),
                     Qt::AlignHCenter | Qt::AlignTop,
                     "Select a tool from the panel and click to add elements");
    painter.setOpacity(1.0);
  }
}

void MapCanvas::draw_fog_zones(QPainter& painter) {
  if (m_map_data == nullptr || !m_layer_visible[LayerFogZone]) {
    return;
  }

  painter.save();
  for (const FogZoneElement& zone : m_map_data->fog_zones()) {
    const QPoint center = grid_to_widget(zone.x, zone.z);
    const int width_px =
        std::max(1, static_cast<int>(zone.width * m_zoom * grid_cell_size));
    const int height_px =
        std::max(1, static_cast<int>(zone.height * m_zoom * grid_cell_size));
    const QRect bounds(
        center.x() - width_px / 2, center.y() - height_px / 2, width_px, height_px);
    const int alpha = std::clamp(static_cast<int>(zone.density * 105.0F), 20, 125);
    painter.setBrush(QColor(150, 175, 190, alpha));
    painter.setPen(QPen(QColor(190, 220, 235, 170), 1, Qt::DashLine));
    painter.drawRect(bounds);
    painter.setPen(QColor(220, 235, 245, 210));
    painter.drawText(bounds.adjusted(4, 2, -4, -2),
                     Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("FOG %1%").arg(qRound(zone.density * 100.0F)));
  }
  painter.restore();
}

void MapCanvas::draw_grid(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  float const cell_size = grid_cell_size * m_zoom;

  if (cell_size <= 0.0F || grid.width <= 0 || grid.height <= 0) {
    return;
  }

  auto const start_x = static_cast<float>(m_pan_offset.x());
  auto const start_y = static_cast<float>(m_pan_offset.y());
  float const end_x = start_x + grid.width * cell_size;
  float const end_y = start_y + grid.height * cell_size;

  float const clip_left = std::max(0.0F, start_x);
  float const clip_right = std::min(static_cast<float>(width()), end_x);
  float const clip_top = std::max(0.0F, start_y);
  float const clip_bottom = std::min(static_cast<float>(height()), end_y);

  if (clip_right <= clip_left || clip_bottom <= clip_top) {
    return;
  }

  int const major_step = CanvasTransform::grid_step_for_spacing(
      cell_size, k_major_grid_spacing_px, k_base_grid_step);
  int const minor_step =
      CanvasTransform::grid_step_for_spacing(cell_size, k_minor_grid_spacing_px, 1);

  auto draw_grid_lines = [&](int step, const QPen& pen) {
    painter.setPen(pen);

    int const first_col =
        static_cast<int>(std::floor((clip_left - start_x) / cell_size)) / step * step;
    int const last_col = std::min(
        grid.width, static_cast<int>(std::ceil((clip_right - start_x) / cell_size)));
    for (int i = std::max(0, first_col); i <= last_col; i += step) {
      float const x = start_x + static_cast<float>(i) * cell_size;
      painter.drawLine(QPointF(x, clip_top), QPointF(x, clip_bottom));
    }

    int const first_row =
        static_cast<int>(std::floor((clip_top - start_y) / cell_size)) / step * step;
    int const last_row = std::min(
        grid.height, static_cast<int>(std::ceil((clip_bottom - start_y) / cell_size)));
    for (int i = std::max(0, first_row); i <= last_row; i += step) {
      float const y = start_y + static_cast<float>(i) * cell_size;
      painter.drawLine(QPointF(clip_left, y), QPointF(clip_right, y));
    }
  };

  if (minor_step < major_step && major_step % minor_step == 0) {
    QColor minor_color = k_grid_line_color;
    minor_color.setAlpha(k_minor_grid_alpha);
    draw_grid_lines(minor_step, QPen(minor_color, 1));
  }
  draw_grid_lines(major_step, QPen(k_grid_line_color, 1));

  painter.setPen(QPen(k_grid_outline_color, 2));
  painter.drawRect(
      QRectF(start_x, start_y, grid.width * cell_size, grid.height * cell_size));

  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);

  painter.setPen(k_grid_text_color);
  const float label_y = std::clamp(start_y, clip_top, clip_bottom - 4.0F) + 11.0F;
  const float label_x = std::clamp(start_x, clip_left, clip_right - 4.0F) + 3.0F;

  int const first_col =
      static_cast<int>(std::floor((clip_left - start_x) / cell_size)) / major_step *
      major_step;
  int const last_col = std::min(
      grid.width, static_cast<int>(std::ceil((clip_right - start_x) / cell_size)));
  for (int i = std::max(0, first_col); i <= last_col; i += major_step) {
    float const x = start_x + static_cast<float>(i) * cell_size;
    if (x < clip_left || x > clip_right - 12.0F) {
      continue;
    }
    painter.drawText(QPointF(x + 3.0F, label_y), QString::number(grid.width - i));
  }

  int const first_row = static_cast<int>(std::floor((clip_top - start_y) / cell_size)) /
                        major_step * major_step;
  int const last_row = std::min(
      grid.height, static_cast<int>(std::ceil((clip_bottom - start_y) / cell_size)));
  for (int i = std::max(0, first_row); i <= last_row; i += major_step) {
    float const y = start_y + static_cast<float>(i) * cell_size;
    if (y < clip_top + 12.0F || y > clip_bottom) {
      continue;
    }
    painter.drawText(QPointF(label_x, y - 3.0F), QString::number(grid.height - i));
  }

  const QPoint origin = grid_to_widget(0.0F, 0.0F);
  painter.setPen(k_grid_text_color);
  painter.drawText(QPointF(origin.x() - 26, origin.y() - 4), QStringLiteral("0,0"));
}

QVector<int> MapCanvas::category_draw_order(int kind) const {
  QVector<int> order;
  if (m_map_data == nullptr) {
    return order;
  }

  const int count = ElementOps::count(*m_map_data, kind);
  order.reserve(count);
  for (int i = 0; i < count; ++i) {
    order.append(i);
  }

  if (kind == static_cast<int>(ElementKind::Terrain)) {

    const auto& terrain = m_map_data->terrain_elements();
    std::stable_sort(order.begin(), order.end(), [&terrain](int lhs, int rhs) {
      return terrain_footprint_cells(terrain[lhs]) >
             terrain_footprint_cells(terrain[rhs]);
    });
  }

  return order;
}

void MapCanvas::draw_category(QPainter& painter, int kind) {
  if (m_map_data == nullptr || !layer_visible(kind)) {
    return;
  }

  for (int index : category_draw_order(kind)) {
    const ElementRef ref{kind, index};
    if (m_front_order.contains(ref) || m_back_order.contains(ref)) {
      continue;
    }
    draw_one_element(painter, ref);
  }
}

void MapCanvas::draw_one_element(QPainter& painter, const ElementRef& ref) {
  if (m_map_data == nullptr || !layer_visible(ref.kind) ||
      !ElementOps::index_valid(*m_map_data, ref.kind, ref.index)) {
    return;
  }

  switch (static_cast<ElementKind>(ref.kind)) {
  case ElementKind::Terrain:
    draw_terrain_element(painter, ref.index);
    break;
  case ElementKind::WorldProp:
    draw_world_prop_element(painter, ref.index);
    break;
  case ElementKind::Linear:
    draw_linear_element(painter, ref.index);
    break;
  case ElementKind::Structure:
    draw_structure_element(painter, ref.index);
    break;
  case ElementKind::TroopSpawn:
    draw_troop_spawn_element(painter, ref.index);
    break;
  case ElementKind::UndeadZone:
    draw_undead_zone_element(painter, ref.index);
    break;
  case ElementKind::Forest:
    draw_forest_element(painter, ref.index);
    break;
  case ElementKind::WildlifeArea:
    draw_wildlife_area_element(painter, ref.index);
    break;
  }
}

void MapCanvas::draw_terrain_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->terrain_elements()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(0, i);
  bool const is_hovered = (m_hovered_type == 0 && m_hovered_index == i);

  draw_terrain_feature(painter, elem, pos);

  QPen outline_pen;
  if (is_selected) {
    outline_pen = QPen(Qt::yellow, 2);
  } else if (is_hovered) {
    outline_pen = QPen(hover_ring_color, 2);
  } else {
    outline_pen = QPen(Qt::white, 1);
  }
  draw_terrain_outline(painter, elem, pos, outline_pen, 0.0);

  if (is_hovered && !is_selected) {
    draw_terrain_outline(painter, elem, pos, QPen(hover_ring_color, 2), 4.0);
  }

  draw_terrain_entrances(painter, elem);
}

void MapCanvas::draw_terrain_entrances(QPainter& painter, const TerrainElement& elem) {
  if (elem.entrances.isEmpty() || elem.type != QStringLiteral("hill")) {
    return;
  }

  const float tile_size =
      m_map_data != nullptr ? std::max(m_map_data->grid().tile_size, 0.0001F) : 1.0F;
  const bool campaign_scale =
      m_map_data != nullptr && Game::Map::is_campaign_landform_scale(
                                   m_map_data->grid().width, m_map_data->grid().height);
  const Game::Map::FootprintCells footprint = terrain_footprint(elem);
  const Game::Map::HillCrownCells crown =
      Game::Map::hill_crown_cells(footprint, elem.height, tile_size, campaign_scale);
  const float cell_px = static_cast<float>(grid_cell_size) * m_zoom;

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  for (const QJsonValue entrance_value : elem.entrances) {
    const QJsonObject entrance = entrance_value.toObject();
    if (!entrance.contains(MapJsonKeys::x) || !entrance.contains(MapJsonKeys::z)) {
      continue;
    }
    const auto entrance_x =
        static_cast<float>(entrance.value(MapJsonKeys::x).toDouble());
    const auto entrance_z =
        static_cast<float>(entrance.value(MapJsonKeys::z).toDouble());
    const auto entrance_radius = static_cast<float>(
        entrance.value(MapJsonKeys::radius)
            .toDouble(entrance.value(MapJsonKeys::width).toDouble(0.0) * 0.5));

    const float entry_half_width_cells = Game::Map::hill_entry_half_width_cells(
        crown, entrance_radius / tile_size, campaign_scale);
    const float mouth_half_width_cells =
        Game::Map::hill_entry_mouth_half_width_cells(entry_half_width_cells);

    const QPointF pos(grid_to_widget(entrance_x, entrance_z));
    const QPointF centre(grid_to_widget(elem.x, elem.z));
    QPointF ramp_dir = centre - pos;
    const auto ramp_length = static_cast<float>(std::hypot(ramp_dir.x(), ramp_dir.y()));
    if (ramp_length > 0.001F) {
      ramp_dir /= ramp_length;
    } else {
      ramp_dir = QPointF(1.0, 0.0);
    }
    const QPointF gate_dir(-ramp_dir.y(), ramp_dir.x());

    const auto mouth_px = static_cast<double>(mouth_half_width_cells * cell_px);
    const auto marker_px =
        static_cast<double>(std::max(entrance_radius / tile_size, 0.6F) * cell_px);
    const double ramp_px = std::min(
        static_cast<double>(ramp_length),
        static_cast<double>(std::min(crown.half_width, crown.half_depth) * cell_px));

    const auto crest_px = static_cast<double>(entry_half_width_cells *
                                              k_entry_crest_width_scale * cell_px);
    const QPointF gate_a = pos + (gate_dir * mouth_px);
    const QPointF gate_b = pos - (gate_dir * mouth_px);
    const QPointF crest = pos + (ramp_dir * ramp_px);

    QPolygonF ramp;
    ramp << gate_a << (crest + (gate_dir * crest_px)) << (crest - (gate_dir * crest_px))
         << gate_b;
    painter.setBrush(QColor(90, 190, 255, 45));
    painter.setPen(QPen(QColor(120, 205, 255, 130), 1.0, Qt::DashLine));
    painter.drawPolygon(ramp);

    const double gate_pen_px =
        std::clamp(static_cast<double>(cell_px) * 0.22, 2.0, 6.0);
    painter.setPen(QPen(QColor(150, 220, 255, 235), gate_pen_px));
    painter.drawLine(gate_a, gate_b);

    painter.setPen(QPen(QColor(16, 34, 52, 220), 1.4));
    painter.setBrush(QColor(90, 190, 255, 190));
    painter.drawEllipse(pos, marker_px, marker_px);

    const double arrow_px = std::max(6.0, std::min(mouth_px, ramp_px) * 0.55);
    const QPointF tip = pos + (ramp_dir * arrow_px);
    QPolygonF arrow;
    arrow << tip << (pos + (gate_dir * arrow_px * 0.45))
          << (pos - (gate_dir * arrow_px * 0.45));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(235, 250, 255, 220));
    painter.drawPolygon(arrow);
  }
  painter.restore();
}

void MapCanvas::draw_world_prop_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->world_props()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(1, i);
  bool const is_hovered = (m_hovered_type == 1 && m_hovered_index == i);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(Qt::white, 1));
  }

  draw_element(painter, elem.type, pos);

  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() + 4, marker_radius_px() + 4);
    painter.restore();
  }
}

void MapCanvas::draw_gate_element(QPainter& painter,
                                  const StructureElement& elem,
                                  bool is_selected,
                                  bool is_hovered,
                                  const QColor& hover_ring_color) {
  const QPoint pos = grid_to_widget(elem.x, elem.z);
  const auto cell_px = static_cast<double>(grid_cell_size) * m_zoom;
  const double half_span = std::max(WallGeometry::k_gate_span * 0.5 * cell_px, 6.0);
  const double half_depth = std::max(WallGeometry::k_gate_depth * 0.5 * cell_px, 3.0);
  const double pier = std::max(half_span * 0.28, 3.0);

  const QColor body = player_color_for_editor(elem.player_id);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(pos);
  painter.rotate(static_cast<double>(elem.rotation));

  painter.setPen(QPen(body.darker(160), 1));
  painter.setBrush(body);
  painter.drawRect(QRectF(-half_span, -half_depth, pier, half_depth * 2.0));
  painter.drawRect(QRectF(half_span - pier, -half_depth, pier, half_depth * 2.0));

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(body.lighter(130), 1, Qt::DotLine));
  painter.drawLine(QPointF(-half_span + pier, 0.0), QPointF(half_span - pier, 0.0));

  QPen outline(is_selected  ? QColor(Qt::yellow)
               : is_hovered ? hover_ring_color
                            : body.lighter(150),
               is_selected || is_hovered ? 2 : 1);
  painter.setPen(outline);
  painter.drawRect(QRectF(-half_span, -half_depth, half_span * 2.0, half_depth * 2.0));
  painter.restore();
}

void MapCanvas::draw_structure_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->structures()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(3, i);
  bool const is_hovered = (m_hovered_type == 3 && m_hovered_index == i);

  if (elem.type == QStringLiteral("wall_gate")) {
    draw_gate_element(painter, elem, is_selected, is_hovered, hover_ring_color);
    return;
  }

  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(Qt::white, 1));
  }

  draw_element(painter, elem.type, pos, elem.player_id);

  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() + 4, marker_radius_px() + 4);
    painter.restore();
  }
}

void MapCanvas::draw_troop_spawn_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->troop_spawns()[i];
  const QPoint pos = grid_to_widget(elem.x, elem.z);

  const bool is_selected = is_selected_element(4, i);
  const bool is_hovered = (m_hovered_type == 4 && m_hovered_index == i);
  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() / 2 + 6, marker_radius_px() / 2 + 6);
    painter.restore();
  }

  draw_troop_marker(painter,
                    pos,
                    elem.type,
                    elem.player_id,
                    static_cast<float>(marker_radius_px()) * 1.375F);
}

float MapCanvas::linear_width_px(const LinearElement& elem) const {
  const float tile_size =
      m_map_data != nullptr ? std::max(m_map_data->grid().tile_size, 0.0001F) : 1.0F;
  const float width_cells = std::max(elem.width, 0.0F) / tile_size;
  return std::max(2.0F, width_cells * static_cast<float>(grid_cell_size) * m_zoom);
}

QPolygonF MapCanvas::linear_polyline_px(const LinearElement& elem) const {
  QPolygonF points;
  for (const QPointF& point : linear_polyline(elem)) {
    points.append(QPointF(
        grid_to_widget(static_cast<float>(point.x()), static_cast<float>(point.y()))));
  }
  return points;
}

void MapCanvas::draw_linear_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->linear_elements()[i];
  const QPolygonF path = linear_polyline_px(elem);
  if (path.size() < 2) {
    return;
  }
  const QPointF start_pos = path.first();
  const QPointF end_pos = path.last();

  bool const is_selected = is_selected_element(2, i);
  bool const is_hovered = (m_hovered_type == 2 && m_hovered_index == i);

  QColor color;
  if (elem.type == "river") {
    color = QColor(70, 130, 200);
  } else if (elem.type == "road") {
    color = QColor(139, 119, 101);
  } else if (elem.type == "bridge") {
    color = QColor(160, 140, 100);
  } else if (elem.type == "wall") {
    color = player_color_for_editor(elem.player_id);
  }

  const qreal band_width = linear_width_px(elem);
  const qreal centre_width = std::clamp(band_width * 0.18, 1.0, 3.0);

  painter.save();
  if (is_selected || is_hovered) {
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color,
                        band_width + 4.0,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));
    painter.drawPolyline(path);
  }

  QColor band_color = color;
  band_color.setAlpha(150);
  painter.setPen(
      QPen(band_color, band_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPolyline(path);

  painter.setPen(QPen(
      color.lighter(115), centre_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPolyline(path);
  painter.restore();

  if (path.size() > 2) {
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color.lighter(140));
    for (int index = 1; index < path.size() - 1; ++index) {
      painter.drawEllipse(path[index], 2.5, 2.5);
    }
    painter.restore();
  }

  int const endpoint_size = 6;
  painter.setBrush(color.lighter());
  painter.setPen(Qt::white);
  painter.drawEllipse(start_pos, endpoint_size, endpoint_size);
  painter.drawEllipse(end_pos, endpoint_size, endpoint_size);
}

void MapCanvas::draw_linear_preview(QPainter& painter) {
  if (m_is_placing_linear) {
    QPoint const start_pos = grid_to_widget(static_cast<float>(m_linear_start.x()),
                                            static_cast<float>(m_linear_start.y()));
    QPointF const current_grid = map_to_grid(m_last_mouse_pos);
    QPoint const end_pos = grid_to_widget(static_cast<float>(current_grid.x()),
                                          static_cast<float>(current_grid.y()));

    QPen const pen(Qt::white, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(start_pos, end_pos);

    painter.save();
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(100, 200, 255, 180));
    painter.drawEllipse(start_pos, 7, 7);
    painter.restore();
  }
}

void MapCanvas::draw_undead_zone_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;

  static const QColor k_zone_fill(100, 40, 140, 55);
  static const QColor k_zone_border(180, 80, 220, 200);
  static const QColor k_leash_ring(140, 100, 180, 80);

  const auto& elem = m_map_data->undead_zones()[i];
  QPoint const center = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(5, i);
  bool const is_hovered = (m_hovered_type == 5 && m_hovered_index == i);

  const int radius_px = static_cast<int>(elem.radius * m_zoom * grid_cell_size);
  const int leash_px = static_cast<int>(elem.leash_radius * m_zoom * grid_cell_size);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(k_leash_ring, 1, Qt::DotLine));
  painter.drawEllipse(center, leash_px, leash_px);

  painter.setBrush(k_zone_fill);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(k_zone_border, 1));
  }
  painter.drawEllipse(center, radius_px, radius_px);

  const QString icon = (elem.anchor_type == QStringLiteral("ruins"))
                           ? QStringLiteral("\u25A9")
                           : QStringLiteral("\u2726");
  painter.setPen(QColor(230, 180, 255));
  QFont f = painter.font();
  f.setPointSize(9);
  painter.setFont(f);
  painter.drawText(
      QRect(center.x() - 10, center.y() - 10, 20, 20), Qt::AlignCenter, icon);

  if (!elem.id.isEmpty() && labels_visible()) {
    f.setPointSize(7);
    painter.setFont(f);
    painter.setPen(QColor(210, 170, 240, 200));
    painter.drawText(QRect(center.x() - 40, center.y() + radius_px + 2, 80, 14),
                     Qt::AlignCenter,
                     elem.id);
  }

  painter.restore();
}

void MapCanvas::draw_forest_element(QPainter& painter, int i) {
  if (i < 0 || i >= m_map_data->forests().size()) {
    return;
  }

  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;

  const auto& elem = m_map_data->forests()[i];
  QPoint const center = grid_to_widget(elem.x, elem.z);

  auto const kind = static_cast<int>(ElementKind::Forest);
  bool const is_selected = is_selected_element(kind, i);
  bool const is_hovered = (m_hovered_type == kind && m_hovered_index == i);

  const int radius_px = static_cast<int>(elem.radius * m_zoom * grid_cell_size);

  const QColor fill(46, 92, 52, 90);
  const QColor outline(104, 168, 108);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setBrush(fill);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(outline, 1, Qt::DashLine));
  }
  painter.drawEllipse(center, radius_px, radius_px);

  painter.setPen(outline);
  QFont f = painter.font();
  f.setPointSize(9);
  painter.setFont(f);
  painter.drawText(QRect(center.x() - 10, center.y() - 10, 20, 20),
                   Qt::AlignCenter,
                   QStringLiteral("\u2660"));

  if (labels_visible() && !elem.id.isEmpty()) {
    painter.drawText(center.x() + radius_px + 4, center.y(), elem.id);
  }

  painter.restore();
}

void MapCanvas::draw_wildlife_area_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;

  const auto& elem = m_map_data->wildlife_areas()[i];
  const WildlifeAreaStyle style = wildlife_area_style(elem.species);
  QPoint const center = grid_to_widget(elem.x, elem.z);

  auto const kind = static_cast<int>(ElementKind::WildlifeArea);
  bool const is_selected = is_selected_element(kind, i);
  bool const is_hovered = (m_hovered_type == kind && m_hovered_index == i);

  const int radius_px = static_cast<int>(elem.radius * m_zoom * grid_cell_size);

  const int marker_px = std::max(marker_radius_px(), 4);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(style.outline, 1, Qt::DotLine));
  painter.drawEllipse(center, radius_px, radius_px);

  painter.setBrush(style.fill);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(style.outline, 1));
  }
  painter.drawEllipse(center, marker_px, marker_px);

  painter.setPen(style.outline);
  QFont f = painter.font();
  f.setPointSize(std::clamp(marker_px, 7, 14));
  painter.setFont(f);
  painter.drawText(
      QRect(
          center.x() - marker_px, center.y() - marker_px, marker_px * 2, marker_px * 2),
      Qt::AlignCenter,
      style.glyph);

  if (labels_visible()) {
    f.setPointSize(7);
    painter.setFont(f);
    painter.drawText(QRect(center.x() - 50, center.y() + marker_px + 2, 100, 14),
                     Qt::AlignCenter,
                     wildlife_species_label(elem.species));
  }

  painter.restore();
}

void MapCanvas::draw_derived_commanders(QPainter& painter) {
  if (m_mission_data == nullptr || m_map_data == nullptr) {
    return;
  }

  const QVector<DerivedCommander> commanders =
      derive_mission_commanders(*m_map_data, m_mission_data->root());
  const auto badge_size = static_cast<float>(marker_radius_px()) * 1.55F;

  painter.save();
  for (const DerivedCommander& commander : commanders) {
    if (commander.authored_in_map) {
      continue;
    }

    const QPoint pos = grid_to_widget(static_cast<float>(commander.position.x()),
                                      static_cast<float>(commander.position.y()));

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 96, 96, 230), 2, Qt::DashLine));
    const int ring = static_cast<int>(badge_size * 0.5F) + 5;
    painter.drawEllipse(pos, ring, ring);
    painter.drawLine(pos.x() - ring, pos.y() - ring, pos.x() + ring, pos.y() + ring);
    painter.drawLine(pos.x() - ring, pos.y() + ring, pos.x() + ring, pos.y() - ring);

    if (labels_visible()) {
      painter.setPen(QColor(255, 150, 150));
      painter.drawText(QRect(pos.x() - 90, pos.y() + ring + 2, 180, 16),
                       Qt::AlignHCenter | Qt::AlignTop,
                       QStringLiteral("%1 has no commander - place %2")
                           .arg(commander.label, commander.suggested_troop_type));
    }
  }
  painter.restore();
}

void MapCanvas::draw_mission_overlays(QPainter& painter) {
  if (m_mission_data == nullptr || m_map_data == nullptr ||
      !m_layer_visible[LayerMissionOverlay]) {
    return;
  }

  painter.save();
  const QJsonArray ai_setups = m_mission_data->array(QStringLiteral("ai_setups"));
  int owner_id = 2;
  for (const QJsonValue ai_value : ai_setups) {
    const QJsonObject ai = ai_value.toObject();
    const QString ai_id = ai.value(QStringLiteral("id")).toString();

    for (const QJsonValue wave_value : ai.value(QStringLiteral("waves")).toArray()) {
      const QJsonObject wave = wave_value.toObject();
      const QJsonObject entry = wave.value(QStringLiteral("entry_point")).toObject();
      const QPoint pos =
          grid_to_widget(static_cast<float>(entry.value("x").toDouble()),
                         static_cast<float>(entry.value("z").toDouble()));
      painter.setBrush(QColor(255, 145, 40, 55));
      painter.setPen(QPen(QColor(255, 170, 70, 230), 2, Qt::DashLine));
      painter.drawEllipse(pos, 14, 14);
      painter.drawLine(pos + QPoint(-19, 0), pos + QPoint(19, 0));
      painter.drawLine(pos + QPoint(0, -19), pos + QPoint(0, 19));
      painter.setPen(QColor(255, 205, 130));
      painter.drawText(QRect(pos.x() - 70, pos.y() + 18, 140, 18),
                       Qt::AlignHCenter | Qt::AlignTop,
                       QStringLiteral("%1 wave @ %2s")
                           .arg(ai_id)
                           .arg(wave.value("timing").toDouble(), 0, 'f', 0));
    }

    const auto draw_setup_marker = [this, &painter, owner_id](const QJsonObject& setup,
                                                              const QString& label,
                                                              bool building) {
      const QJsonObject position = setup.value("position").toObject();
      const QPoint pos =
          grid_to_widget(static_cast<float>(position.value("x").toDouble()),
                         static_cast<float>(position.value("z").toDouble()));
      if (building) {
        painter.setBrush(player_color_for_editor(owner_id));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(QRect(pos.x() - 6, pos.y() - 6, 12, 12));
      } else {
        draw_troop_marker(painter,
                          pos,
                          setup.value("type").toString(),
                          owner_id,
                          static_cast<float>(marker_radius_px()) * 1.375F);
      }
      painter.setPen(QColor(180, 225, 255));
      painter.drawText(QRect(pos.x() - 55, pos.y() + 10, 110, 16),
                       Qt::AlignHCenter | Qt::AlignTop,
                       label);
    };

    for (const QJsonValue unit_value :
         ai.value(QStringLiteral("starting_units")).toArray()) {
      const QJsonObject unit = unit_value.toObject();
      draw_setup_marker(
          unit, ai_id + QStringLiteral(": ") + unit.value("type").toString(), false);
    }
    for (const QJsonValue building_value :
         ai.value(QStringLiteral("starting_buildings")).toArray()) {
      const QJsonObject building = building_value.toObject();
      draw_setup_marker(building,
                        ai_id + QStringLiteral(": ") +
                            building.value("type").toString(),
                        true);
    }
    ++owner_id;
  }

  draw_derived_commanders(painter);

  const QStringList objective_keys = {QStringLiteral("victory_conditions"),
                                      QStringLiteral("optional_objectives")};
  for (const QString& key : objective_keys) {
    for (const QJsonValue condition_value : m_mission_data->array(key)) {
      const QJsonObject condition = condition_value.toObject();
      const QString zone_id = condition.value(QStringLiteral("zone_id")).toString();
      if (zone_id.isEmpty()) {
        continue;
      }
      for (const UndeadZoneElement& zone : m_map_data->undead_zones()) {
        if (zone.id != zone_id) {
          continue;
        }
        const QPoint pos = grid_to_widget(zone.x, zone.z);
        const int radius =
            std::max(12, static_cast<int>(zone.radius * m_zoom * grid_cell_size));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 220, 70), 3));
        painter.drawEllipse(pos, radius + 4, radius + 4);
        painter.setPen(QColor(255, 235, 130));
        painter.drawText(QRect(pos.x() - 90, pos.y() - radius - 24, 180, 18),
                         Qt::AlignCenter,
                         QStringLiteral("OBJECTIVE: %1").arg(zone_id));
      }
    }
  }
  painter.restore();
}

void MapCanvas::draw_current_placement(QPainter& painter) {
  if (m_current_tool == ToolType::Select || m_current_tool == ToolType::Eraser) {
    return;
  }

  QPointF const grid_pos = map_to_grid(m_last_mouse_pos);
  QPoint const widget_pos = grid_to_widget(static_cast<float>(grid_pos.x()),
                                           static_cast<float>(grid_pos.y()));

  painter.setOpacity(0.5);
  painter.setPen(QPen(Qt::white, 1, Qt::DashLine));

  const QString type = element_type_for_tool(m_current_tool);
  switch (tool_placement(m_current_tool)) {
  case ToolPlacement::Troop:
    draw_troop_marker(painter,
                      widget_pos,
                      type,
                      m_current_player_id,
                      static_cast<float>(marker_radius_px()) * 1.375F);
    break;
  case ToolPlacement::Wildlife: {
    const WildlifeAreaStyle style = wildlife_area_style(type);
    const int radius_px =
        static_cast<int>(default_wildlife_radius(type) * m_zoom * grid_cell_size);
    painter.setBrush(style.fill);
    painter.setPen(QPen(style.outline, 1, Qt::DashLine));
    painter.drawEllipse(widget_pos, radius_px, radius_px);
    painter.setPen(style.outline);
    QFont f = painter.font();
    f.setPointSize(9);
    painter.setFont(f);
    painter.drawText(QRect(widget_pos.x() - 10, widget_pos.y() - 10, 20, 20),
                     Qt::AlignCenter,
                     style.glyph);
    break;
  }
  case ToolPlacement::UndeadZone: {
    const int radius_px = static_cast<int>(8.0F * m_zoom * grid_cell_size);
    painter.setBrush(QColor(100, 40, 140, 55));
    painter.setPen(QPen(QColor(180, 80, 220, 160), 1, Qt::DashLine));
    painter.drawEllipse(widget_pos, radius_px, radius_px);
    painter.setPen(QColor(230, 180, 255, 160));
    QFont f = painter.font();
    f.setPointSize(9);
    painter.setFont(f);
    painter.drawText(
        QRect(widget_pos.x() - 10, widget_pos.y() - 10, 20, 20), Qt::AlignCenter, "☠");
    break;
  }
  case ToolPlacement::Terrain: {
    TerrainElement preview = default_terrain_element(m_current_tool);
    preview.x = static_cast<float>(grid_pos.x());
    preview.z = static_cast<float>(grid_pos.y());
    draw_terrain_feature(painter, preview, widget_pos);
    break;
  }
  case ToolPlacement::Structure:
    draw_element(painter, type, widget_pos, m_current_player_id);
    break;
  case ToolPlacement::WorldProp:
    draw_element(painter, type, widget_pos);
    break;
  case ToolPlacement::None:
  case ToolPlacement::Erase:
  case ToolPlacement::Gate:
  case ToolPlacement::Forest:
  case ToolPlacement::Linear:
    break;
  }

  painter.setOpacity(1.0);
}

void MapCanvas::draw_element(QPainter& painter,
                             const QString& type,
                             const QPoint& pos,
                             int player_id,
                             int marker_radius_px) {

  int const size = marker_radius_px > 0 ? marker_radius_px : this->marker_radius_px();

  QColor fill_color;
  QString symbol;

  if (type == "barracks") {
    fill_color = player_color_for_editor(player_id);
    symbol = "B";
  } else if (type == "village") {
    fill_color = player_color_for_editor(player_id);
    symbol = "V";
  } else if (type == "defense_tower") {
    fill_color = player_color_for_editor(player_id);
    symbol = "T";
  } else if (type == "home") {
    fill_color = player_color_for_editor(player_id);
    symbol = "H";
  } else if (type == "marketplace") {
    fill_color = player_color_for_editor(player_id);
    symbol = "M";
  } else if (type == "temple") {
    fill_color = player_color_for_editor(player_id);
    symbol = "\u03A9";
  } else if (type == "farm") {
    fill_color = player_color_for_editor(player_id);
    symbol = "F";
  } else {
    fill_color = QColor(128, 128, 128);
    symbol = "?";
  }

  if (type == "firecamp" || type == "tent" || type == "supply_cart" ||
      type == "weapon_rack" || type == "ruins" || type == "magic_shrine" ||
      type == "dead_tree" || type == "boulder" || type == "pine_tree" ||
      type == "olive_tree" || type == "plant" || type == "iron_ore" ||
      type == "abandoned_home" || type == "statue" || type == "cursed_gold_vein") {
    draw_world_prop_icon(painter, type, pos, size);
  } else {
    painter.setBrush(fill_color);
    painter.setPen(QPen(fill_color.darker(140), 1));
    painter.drawEllipse(pos, size, size);
    QFont font = painter.font();
    const int symbol_point_size = std::clamp(size * 2 / 3, 8, 52);
    font.setPointSize(symbol_point_size);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(pos.x() - size, pos.y() - size, size * 2, size * 2),
                     Qt::AlignCenter,
                     symbol);
    if ((type == "barracks" || type == "village" || type == "defense_tower" ||
         type == "home" || type == "marketplace" || type == "temple" ||
         type == "farm") &&
        player_id >= 0 && labels_visible()) {
      QString const player_text = player_id == 0 ? "N" : QString::number(player_id);
      font.setPointSize(8);
      painter.setFont(font);
      painter.setPen(Qt::black);
      painter.drawText(pos.x() + size - 6, pos.y() - size + 10, player_text);
    }
  }
}

void MapCanvas::draw_world_prop_icon(QPainter& painter,
                                     const QString& type,
                                     const QPoint& pos,
                                     int size) {
  const auto s = static_cast<float>(size);
  painter.save();
  painter.translate(pos);

  if (type == QStringLiteral("firecamp")) {
    painter.setBrush(QColor(255, 140, 0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);

    const float fw = s * 0.38F;
    const float fh = s * 0.80F;
    const auto draw_flame = [&](float ox, float oy_base, float w, float h) {
      QPainterPath flame;
      flame.moveTo(ox, oy_base);
      flame.cubicTo(
          ox - w, oy_base - h * 0.5F, ox - w * 0.6F, oy_base - h, ox, oy_base - h);
      flame.cubicTo(
          ox + w * 0.6F, oy_base - h, ox + w, oy_base - h * 0.5F, ox, oy_base);
      painter.drawPath(flame);
    };
    painter.setBrush(QColor(255, 230, 80));
    draw_flame(-fw * 0.4F, s * 0.45F, fw * 0.7F, fh * 0.75F);
    draw_flame(fw * 0.4F, s * 0.45F, fw * 0.7F, fh * 0.75F);
    painter.setBrush(QColor(255, 255, 180));
    draw_flame(0.0F, s * 0.50F, fw, fh);

  } else if (type == QStringLiteral("tent")) {
    painter.setBrush(QColor(176, 126, 78));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float th = s * 0.75F;
    const float tw = s * 0.85F;
    QPolygonF tent;
    tent << QPointF(0, -th) << QPointF(tw, th * 0.55F) << QPointF(-tw, th * 0.55F);
    painter.setBrush(QColor(210, 165, 100));
    painter.drawPolygon(tent);

    painter.setPen(QPen(QColor(120, 85, 50), std::max(1.0F, s * 0.1F)));
    painter.drawLine(QPointF(0, 0), QPointF(0, th * 0.55F));

  } else if (type == QStringLiteral("supply_cart")) {
    painter.setBrush(QColor(140, 106, 64));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float bw = s * 0.75F;
    const float bh = s * 0.45F;
    painter.setBrush(QColor(180, 140, 90));
    painter.setPen(QPen(QColor(100, 72, 40), std::max(1.0F, s * 0.1F)));
    painter.drawRect(QRectF(-bw, -bh * 0.6F, bw * 2, bh));
    const float wr = s * 0.22F;
    painter.setBrush(QColor(80, 60, 35));
    painter.drawEllipse(QPointF(-bw * 0.6F, bh * 0.55F), wr, wr);
    painter.drawEllipse(QPointF(bw * 0.6F, bh * 0.55F), wr, wr);

  } else if (type == QStringLiteral("weapon_rack")) {
    painter.setBrush(QColor(120, 120, 132));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float arm = s * 0.70F;
    QPen const cross_pen(
        QColor(210, 210, 220), std::max(2.0F, s * 0.18F), Qt::SolidLine, Qt::RoundCap);
    painter.setPen(cross_pen);
    painter.drawLine(QPointF(-arm, -arm), QPointF(arm, arm));
    painter.drawLine(QPointF(arm, -arm), QPointF(-arm, arm));

  } else if (type == QStringLiteral("ruins")) {
    painter.setBrush(QColor(102, 98, 90));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float rs = s * 0.65F;
    QPen const rp(
        QColor(170, 165, 150), std::max(1.5F, s * 0.12F), Qt::SolidLine, Qt::SquareCap);
    painter.setPen(rp);

    const float gap = rs * 0.35F;
    painter.drawLine(QPointF(-rs, -rs), QPointF(-rs + gap, -rs));
    painter.drawLine(QPointF(-rs, -rs), QPointF(-rs, -rs + gap));
    painter.drawLine(QPointF(rs - gap, -rs), QPointF(rs, -rs));
    painter.drawLine(QPointF(rs, -rs), QPointF(rs, -rs + gap));
    painter.drawLine(QPointF(-rs, rs - gap), QPointF(-rs, rs));
    painter.drawLine(QPointF(-rs, rs), QPointF(-rs + gap, rs));
    painter.drawLine(QPointF(rs, rs - gap), QPointF(rs, rs));
    painter.drawLine(QPointF(rs, rs), QPointF(rs - gap, rs));

  } else if (type == QStringLiteral("magic_shrine")) {
    painter.setBrush(QColor(118, 96, 196));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float sr = s * 0.78F;
    const int points = 6;
    const float inner_r = sr * 0.45F;
    QPolygonF star;
    for (int k = 0; k < points * 2; ++k) {
      const float angle = static_cast<float>(k) * std::numbers::pi_v<float> /
                              static_cast<float>(points) -
                          std::numbers::pi_v<float> * 0.5F;
      const float r = (k % 2 == 0) ? sr : inner_r;
      star << QPointF(r * std::cos(angle), r * std::sin(angle));
    }
    painter.setBrush(QColor(210, 180, 255));
    painter.drawPolygon(star);
    painter.setBrush(QColor(255, 240, 255));
    painter.drawEllipse(QPointF(0, 0), sr * 0.18F, sr * 0.18F);

  } else if (type == QStringLiteral("dead_tree")) {
    painter.setBrush(QColor(111, 86, 67));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPen const tp(
        QColor(200, 170, 135), std::max(1.5F, s * 0.14F), Qt::SolidLine, Qt::RoundCap);
    painter.setPen(tp);

    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.15F));

    painter.drawLine(QPointF(0, -s * 0.15F), QPointF(-s * 0.55F, -s * 0.65F));
    painter.drawLine(QPointF(0, -s * 0.15F), QPointF(s * 0.55F, -s * 0.65F));
    painter.drawLine(QPointF(0, s * 0.20F), QPointF(-s * 0.42F, -s * 0.22F));
    painter.drawLine(QPointF(0, s * 0.20F), QPointF(s * 0.42F, -s * 0.22F));

  } else if (type == QStringLiteral("boulder")) {
    painter.setBrush(QColor(110, 110, 110));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);

    QPolygonF rock;
    rock << QPointF(0, -s * 0.72F) << QPointF(s * 0.58F, -s * 0.40F)
         << QPointF(s * 0.70F, s * 0.20F) << QPointF(s * 0.30F, s * 0.68F)
         << QPointF(-s * 0.38F, s * 0.68F) << QPointF(-s * 0.70F, s * 0.18F)
         << QPointF(-s * 0.55F, -s * 0.42F);
    painter.setBrush(QColor(175, 175, 175));
    painter.drawPolygon(rock);

    QPen const hp(QColor(215, 215, 215), std::max(1.0F, s * 0.10F));
    painter.setPen(hp);
    painter.drawLine(QPointF(-s * 0.30F, -s * 0.55F), QPointF(s * 0.40F, -s * 0.35F));
  } else if (type == QStringLiteral("pine_tree") ||
             type == QStringLiteral("olive_tree")) {
    const QColor crown = type == QStringLiteral("pine_tree") ? QColor(38, 110, 70)
                                                             : QColor(105, 126, 62);
    painter.setBrush(crown.darker(145));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    painter.setPen(QPen(QColor(105, 72, 42), std::max(1.5F, s * 0.16F)));
    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.15F));
    painter.setBrush(crown);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, -s * 0.25F), s * 0.62F, s * 0.62F);
  } else if (type == QStringLiteral("plant")) {
    painter.setBrush(QColor(54, 122, 58));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    painter.setPen(QPen(QColor(150, 205, 92), std::max(1.5F, s * 0.14F)));
    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.55F));
    painter.drawLine(QPointF(0, 0), QPointF(-s * 0.5F, -s * 0.25F));
    painter.drawLine(QPointF(0, s * 0.15F), QPointF(s * 0.5F, -s * 0.15F));
  } else if (type == QStringLiteral("iron_ore")) {
    painter.setBrush(QColor(68, 78, 88));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPolygonF ore;
    ore << QPointF(0, -s * 0.75F) << QPointF(s * 0.65F, -s * 0.15F)
        << QPointF(s * 0.35F, s * 0.7F) << QPointF(-s * 0.55F, s * 0.55F)
        << QPointF(-s * 0.7F, -s * 0.2F);
    painter.setBrush(QColor(150, 164, 176));
    painter.drawPolygon(ore);
  } else if (type == QStringLiteral("abandoned_home")) {
    painter.setBrush(QColor(86, 74, 60));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPolygonF gable;
    gable << QPointF(-s * 0.62F, s * 0.55F) << QPointF(-s * 0.62F, -s * 0.20F)
          << QPointF(0.0F, -s * 0.70F) << QPointF(s * 0.62F, -s * 0.20F)
          << QPointF(s * 0.62F, s * 0.55F);
    painter.setBrush(QColor(168, 152, 126));
    painter.drawPolygon(gable);
    painter.setPen(QPen(QColor(58, 48, 38), std::max(1.5F, s * 0.14F)));
    painter.drawLine(QPointF(s * 0.10F, -s * 0.45F), QPointF(s * 0.62F, -s * 0.05F));
    painter.drawLine(QPointF(-s * 0.30F, s * 0.55F), QPointF(-s * 0.30F, -s * 0.05F));
  } else if (type == QStringLiteral("statue")) {
    painter.setBrush(QColor(74, 82, 88));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    painter.setBrush(QColor(214, 210, 198));
    painter.drawRect(QRectF(-s * 0.46F, s * 0.34F, s * 0.92F, s * 0.34F));
    painter.drawRect(QRectF(-s * 0.24F, -s * 0.14F, s * 0.48F, s * 0.48F));
    painter.drawEllipse(QPointF(0, -s * 0.36F), s * 0.24F, s * 0.24F);
  } else if (type == QStringLiteral("cursed_gold_vein")) {
    painter.setBrush(QColor(52, 44, 38));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPolygonF crag;
    crag << QPointF(-s * 0.70F, s * 0.55F) << QPointF(-s * 0.30F, -s * 0.10F)
         << QPointF(0.0F, s * 0.15F) << QPointF(s * 0.34F, -s * 0.30F)
         << QPointF(s * 0.72F, s * 0.55F);
    painter.setBrush(QColor(96, 84, 72));
    painter.drawPolygon(crag);
    QPolygonF crystal;
    crystal << QPointF(-s * 0.12F, s * 0.30F) << QPointF(0.0F, -s * 0.62F)
            << QPointF(s * 0.16F, s * 0.30F);
    painter.setBrush(QColor(242, 196, 74));
    painter.drawPolygon(crystal);
    painter.setPen(QPen(QColor(214, 48, 32), std::max(1.2F, s * 0.12F)));
    painter.drawLine(QPointF(-s * 0.46F, s * 0.40F), QPointF(s * 0.26F, -s * 0.06F));
  }

  painter.restore();
}

Game::Map::FootprintCells
MapCanvas::terrain_footprint(const TerrainElement& elem) const {
  const float tile_size =
      m_map_data != nullptr ? std::max(m_map_data->grid().tile_size, 0.0001F) : 1.0F;
  const bool campaign_scale =
      m_map_data != nullptr && Game::Map::is_campaign_landform_scale(
                                   m_map_data->grid().width, m_map_data->grid().height);

  if (elem.type == QStringLiteral("mountain")) {
    return Game::Map::mountain_footprint_cells({.width = elem.width,
                                                .depth = elem.depth,
                                                .radius = elem.radius,
                                                .rotation_deg = elem.rotation,
                                                .tile_size = tile_size});
  }

  if (elem.type == QStringLiteral("lake")) {
    Game::Map::FootprintCells lake;
    lake.half_width = elem.width > 0.0F ? elem.width * 0.5F / tile_size
                                        : std::max(elem.radius / tile_size, 1.0F);
    lake.half_depth = elem.depth > 0.0F ? elem.depth * 0.5F / tile_size
                                        : std::max(elem.radius / tile_size, 1.0F);
    lake.width_cells = lake.half_width * 2.0F;
    lake.depth_cells = lake.half_depth * 2.0F;
    lake.rotation_deg = elem.rotation;
    return lake;
  }

  Game::Map::HillShape shape = Game::Map::HillShape::Blob;
  const bool shaped =
      Game::Map::parse_hill_shape(elem.shape.trimmed().toStdString(), shape) &&
      shape != Game::Map::HillShape::Blob;

  Game::Map::FootprintCells footprint =
      Game::Map::hill_footprint_cells({.width = elem.width,
                                       .depth = elem.depth,
                                       .radius = elem.radius,
                                       .rotation_deg = elem.rotation,
                                       .tile_size = tile_size,
                                       .grid_center_x = elem.x,
                                       .grid_center_z = elem.z,
                                       .campaign_scale = campaign_scale,
                                       .shaped = shaped});
  if (shape == Game::Map::HillShape::Mask) {
    footprint.rotation_deg = 0.0F;
  }
  return footprint;
}

Game::Map::HillShapeGeometry
MapCanvas::terrain_shape(const TerrainElement& elem) const {
  Game::Map::HillShapeGeometry geometry;
  if (elem.type != QStringLiteral("hill")) {
    return geometry;
  }

  Game::Map::HillShape shape = Game::Map::HillShape::Blob;
  if (!Game::Map::parse_hill_shape(elem.shape.trimmed().toStdString(), shape) ||
      shape == Game::Map::HillShape::Blob) {
    return geometry;
  }

  const Game::Map::FootprintCells footprint = terrain_footprint(elem);
  const double radians =
      static_cast<double>(footprint.rotation_deg) * std::numbers::pi / 180.0;
  const double cos_yaw = std::cos(radians);
  const double sin_yaw = std::sin(radians);

  Game::Map::HillShapeAuthoring authoring;
  authoring.shape = shape;
  authoring.thickness = elem.thickness;
  authoring.has_sweep = elem.has_arc;
  authoring.has_sweep_start = elem.has_arc_start;
  authoring.sweep_degrees = elem.arc;
  authoring.sweep_start_degrees = elem.arc_start;
  authoring.taper = elem.taper;
  for (const QJsonValue point_value : elem.points) {
    const QJsonObject point = point_value.toObject();
    const double dx = point.value(MapJsonKeys::x).toDouble(0.0) - elem.x;
    const double dz = point.value(MapJsonKeys::z).toDouble(0.0) - elem.z;
    authoring.local_points.push_back(
        {static_cast<float>(dx * cos_yaw + dz * sin_yaw),
         static_cast<float>(-dx * sin_yaw + dz * cos_yaw)});
  }

  if (shape == Game::Map::HillShape::Mask) {
    return geometry;
  }

  const auto params = Game::Map::hill_shape_params(
      footprint,
      authoring,
      m_map_data != nullptr ? std::max(m_map_data->grid().tile_size, 0.0001F) : 1.0F);
  return Game::Map::build_hill_shape(params);
}

QVector<QPoint> MapCanvas::terrain_mask_cells(const TerrainElement& elem) {
  QVector<QPoint> cells;
  for (const QJsonValue row_value : elem.cells) {
    const QJsonArray row = row_value.toArray();
    if (row.size() == 2) {
      cells.append(QPoint(static_cast<int>(std::lround(row.at(0).toDouble(0.0))),
                          static_cast<int>(std::lround(row.at(1).toDouble(0.0)))));
    } else if (row.size() == 3) {
      const int cell_z = static_cast<int>(std::lround(row.at(0).toDouble(0.0)));
      const int from = static_cast<int>(std::lround(row.at(1).toDouble(0.0)));
      const int to = static_cast<int>(std::lround(row.at(2).toDouble(0.0)));
      for (int cell_x = std::min(from, to); cell_x <= std::max(from, to); ++cell_x) {
        cells.append(QPoint(cell_x, cell_z));
      }
    }
  }
  return cells;
}

QSizeF MapCanvas::terrain_ellipse_px(const TerrainElement& elem) const {
  const Game::Map::FootprintCells footprint = terrain_footprint(elem);
  const Game::Map::HillShapeGeometry shape = terrain_shape(elem);
  const float half_width = shape.is_spine() ? shape.bound_half_x : footprint.half_width;
  const float half_depth = shape.is_spine() ? shape.bound_half_z : footprint.half_depth;
  const float rx = half_width * static_cast<float>(grid_cell_size) * m_zoom;
  const float ry = half_depth * static_cast<float>(grid_cell_size) * m_zoom;

  return {std::max(static_cast<float>(marker_radius_px()), rx), std::max(4.0F, ry)};
}

void MapCanvas::draw_terrain_outline(QPainter& painter,
                                     const TerrainElement& elem,
                                     const QPoint& center,
                                     const QPen& pen,
                                     double margin_px) {
  const Game::Map::FootprintCells footprint = terrain_footprint(elem);
  const Game::Map::HillShapeGeometry shape = terrain_shape(elem);
  const double cell_px = static_cast<double>(grid_cell_size) * m_zoom;

  painter.save();
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.translate(center);
  painter.scale(-1.0, -1.0);
  painter.rotate(static_cast<double>(footprint.rotation_deg));

  if (shape.is_spine()) {
    QPainterPath spine_path;
    spine_path.moveTo(shape.spine.front().x * cell_px, shape.spine.front().z * cell_px);
    for (std::size_t i = 1; i < shape.spine.size(); ++i) {
      spine_path.lineTo(shape.spine[i].x * cell_px, shape.spine[i].z * cell_px);
    }
    QPainterPathStroker stroker;
    stroker.setWidth(
        std::max(2.0, shape.half_thickness * 2.0 * cell_px + margin_px * 2.0));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    painter.drawPath(stroker.createStroke(spine_path));
  } else if (is_mask_hill(elem)) {
    const QVector<QPoint> cells = terrain_mask_cells(elem);
    double min_x = 0.0;
    double max_x = 0.0;
    double min_z = 0.0;
    double max_z = 0.0;
    bool first = true;
    for (const QPoint& cell : cells) {
      const double local_x = static_cast<double>(cell.x()) - elem.x;
      const double local_z = static_cast<double>(cell.y()) - elem.z;
      min_x = first ? local_x : std::min(min_x, local_x);
      max_x = first ? local_x : std::max(max_x, local_x);
      min_z = first ? local_z : std::min(min_z, local_z);
      max_z = first ? local_z : std::max(max_z, local_z);
      first = false;
    }
    painter.drawRect(QRectF((min_x - 0.5) * cell_px - margin_px,
                            (min_z - 0.5) * cell_px - margin_px,
                            (max_x - min_x + 1.0) * cell_px + margin_px * 2.0,
                            (max_z - min_z + 1.0) * cell_px + margin_px * 2.0));
  } else {
    const QSizeF ellipse = terrain_ellipse_px(elem);
    painter.drawEllipse(
        QPointF(0, 0), ellipse.width() + margin_px, ellipse.height() + margin_px);
  }

  painter.restore();
}

void MapCanvas::draw_terrain_feature(QPainter& painter,
                                     const TerrainElement& elem,
                                     const QPoint& center) {
  const Game::Map::FootprintCells footprint = terrain_footprint(elem);
  const QSizeF ellipse = terrain_ellipse_px(elem);
  const auto rx = ellipse.width();
  const auto ry = ellipse.height();

  painter.save();
  painter.translate(center);
  painter.scale(-1.0, -1.0);
  painter.rotate(static_cast<double>(footprint.rotation_deg));

  if (elem.type == QStringLiteral("hill")) {

    const QColor outer(168, 148, 102);
    const QColor mid(144, 122, 78);
    const QColor inner(120, 98, 58);
    const QColor peak(96, 74, 42);

    const Game::Map::HillShapeGeometry shape = terrain_shape(elem);
    painter.setPen(Qt::NoPen);

    if (shape.is_spine()) {
      const double cell_px = static_cast<double>(grid_cell_size) * m_zoom;
      QPainterPath spine_path;
      spine_path.moveTo(shape.spine.front().x * cell_px,
                        shape.spine.front().z * cell_px);
      for (std::size_t i = 1; i < shape.spine.size(); ++i) {
        spine_path.lineTo(shape.spine[i].x * cell_px, shape.spine[i].z * cell_px);
      }

      const auto stroke = [&](const QColor& color, double half_thickness_cells) {
        QPen pen(color,
                 std::max(2.0, half_thickness_cells * 2.0 * cell_px),
                 Qt::SolidLine,
                 Qt::RoundCap,
                 Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawPath(spine_path);
      };
      stroke(outer, shape.half_thickness);
      stroke(mid, shape.half_thickness * 0.72);
      stroke(inner, shape.half_thickness * 0.45);
      painter.setPen(Qt::NoPen);
    } else if (is_mask_hill(elem)) {
      const double cell_px = static_cast<double>(grid_cell_size) * m_zoom;
      painter.setBrush(outer);
      for (const QPoint& cell : terrain_mask_cells(elem)) {
        painter.drawRect(
            QRectF((static_cast<double>(cell.x()) - elem.x - 0.5) * cell_px,
                   (static_cast<double>(cell.y()) - elem.z - 0.5) * cell_px,
                   cell_px,
                   cell_px));
      }
    } else {
      painter.setBrush(outer);
      painter.drawEllipse(QPointF(0, 0), rx, ry);

      painter.setBrush(mid);
      painter.drawEllipse(QPointF(0, 0), rx * 0.72, ry * 0.72);

      painter.setBrush(inner);
      painter.drawEllipse(QPointF(0, 0), rx * 0.45, ry * 0.45);
    }

    painter.setBrush(peak);
    const double dot_r = std::max(2.5, std::min(rx, ry) * 0.18);
    painter.drawEllipse(QPointF(0, 0), dot_r, dot_r);

  } else if (elem.type == QStringLiteral("mountain")) {

    const QColor outer(148, 148, 162);
    const QColor mid(115, 115, 130);
    const QColor inner(85, 85, 100);
    const QColor peak(230, 235, 245);

    painter.setPen(Qt::NoPen);
    painter.setBrush(outer);
    painter.drawEllipse(QPointF(0, 0), rx, ry);

    painter.setBrush(mid);
    painter.drawEllipse(QPointF(0, 0), rx * 0.72, ry * 0.72);

    painter.setBrush(inner);
    painter.drawEllipse(QPointF(0, 0), rx * 0.45, ry * 0.45);

    painter.setBrush(peak);
    const double dot_r = std::max(2.5, std::min(rx, ry) * 0.18);
    painter.drawEllipse(QPointF(0, 0), dot_r, dot_r);
  } else if (elem.type == QStringLiteral("lake")) {
    painter.setPen(QPen(QColor(35, 80, 105), 2.0));
    painter.setBrush(QColor(55, 135, 165, 205));
    painter.drawEllipse(QPointF(0, 0), rx, ry);
    painter.setPen(QPen(QColor(145, 205, 215, 175), 1.0));
    painter.drawEllipse(QPointF(0, 0), rx * 0.82, ry * 0.82);
  }

  const bool shaped_hill = elem.type == QStringLiteral("hill") &&
                           (terrain_shape(elem).is_spine() || is_mask_hill(elem));
  if (footprint.organic_spread > 0.0F && !shaped_hill) {
    const double spread = 1.0 + static_cast<double>(footprint.organic_spread);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 226, 168, 130), 1.0, Qt::DotLine));
    painter.drawEllipse(QPointF(0, 0), rx * spread, ry * spread);
  }

  painter.restore();
}

int MapCanvas::terrain_marker_radius_px(const TerrainElement& elem) const {
  if (elem.type != QStringLiteral("hill") && elem.type != QStringLiteral("mountain") &&
      elem.type != QStringLiteral("lake")) {
    return marker_radius_px();
  }
  const QSizeF e = terrain_ellipse_px(elem);
  return std::max(marker_radius_px(),
                  static_cast<int>(std::round(std::max(e.width(), e.height()))));
}

float MapCanvas::terrain_hit_radius_px(const TerrainElement& elem) const {
  return static_cast<float>(terrain_marker_radius_px(elem)) + 4.0F;
}

void MapCanvas::draw_rubber_band(QPainter& painter) {
  if (!m_band_active) {
    return;
  }

  painter.save();
  painter.setPen(QPen(k_hover_select_color, 1, Qt::DashLine));
  painter.setBrush(QColor(100, 200, 255, 40));
  painter.drawRect(QRect(m_band_origin, m_band_current).normalized());
  painter.restore();
}

} // namespace MapEditor
