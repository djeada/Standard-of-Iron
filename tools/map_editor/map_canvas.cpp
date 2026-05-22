#include "map_canvas.h"

#include <QApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>
#include <limits>

#include "canvas_transform.h"
#include "spawn_icon_library.h"
#include "troop_tool_specs.h"

namespace MapEditor {

namespace {

const QColor k_canvas_background(7, 16, 24);
const QColor k_canvas_border(15, 43, 52);
const QColor k_grid_line_color(17, 48, 68);
const QColor k_grid_outline_color(79, 106, 117);
const QColor k_grid_text_color(183, 210, 223);
const QColor k_empty_state_text(159, 217, 255);
const QColor k_hover_select_color(100, 200, 255);
const QColor k_hover_erase_color(255, 80, 80);

QPointF snap_pos(const QPointF& gp) {
  return {std::round(gp.x()), std::round(gp.y())};
}

auto world_prop_type_for_tool(ToolType tool) -> QString {
  switch (tool) {
  case ToolType::PropFirecamp:
    return QStringLiteral("firecamp");
  case ToolType::PropTent:
    return QStringLiteral("tent");
  case ToolType::PropSupplyCart:
    return QStringLiteral("supply_cart");
  case ToolType::PropWeaponRack:
    return QStringLiteral("weapon_rack");
  case ToolType::PropRuins:
    return QStringLiteral("ruins");
  case ToolType::PropMagicShrine:
    return QStringLiteral("magic_shrine");
  case ToolType::PropDeadTree:
    return QStringLiteral("dead_tree");
  case ToolType::PropBoulder:
    return QStringLiteral("boulder");
  default:
    return {};
  }
}

void draw_troop_marker(QPainter& painter,
                       const QPoint& pos,
                       const QString& type,
                       int player_id) {
  constexpr float k_troop_badge_size = 22.0F;
  const QRectF bounds(pos.x() - k_troop_badge_size * 0.5F,
                      pos.y() - k_troop_badge_size * 0.5F,
                      k_troop_badge_size,
                      k_troop_badge_size);
  paint_troop_badge(painter, bounds, type, player_color_for_editor(player_id));
}

} // namespace

MapCanvas::MapCanvas(QWidget* parent)
    : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(400, 400);

  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, k_canvas_background);
  setPalette(pal);
}

void MapCanvas::set_map_data(MapData* data) {
  m_map_data = data;
  if (m_map_data != nullptr) {
    connect(m_map_data, &MapData::data_changed, this, qOverload<>(&QWidget::update));
  }
  update();
}

void MapCanvas::set_current_tool(ToolType tool) {
  m_current_tool = tool;
  m_is_placing_linear = false;
  update_canvas_cursor(m_last_mouse_pos);
  emit status_hint_changed("");
  update();
}

void MapCanvas::clear_tool() {
  m_current_tool = ToolType::Select;
  m_is_placing_linear = false;
  update_canvas_cursor(m_last_mouse_pos);
  emit status_hint_changed("");
  emit tool_cleared();
  update();
}

void MapCanvas::begin_panning(const QPoint& pos) {
  m_is_panning = true;
  m_is_pan_drag_pending = false;
  m_last_mouse_pos = pos;
  setCursor(Qt::ClosedHandCursor);
}

void MapCanvas::finish_panning(const QPoint& pos) {
  m_is_panning = false;
  m_is_pan_drag_pending = false;
  update_canvas_cursor(pos);
}

void MapCanvas::update_canvas_cursor(const QPoint& pos) {
  if (m_is_panning || m_is_dragging) {
    setCursor(Qt::ClosedHandCursor);
    return;
  }

  if (m_space_pan_active) {
    setCursor(Qt::OpenHandCursor);
    return;
  }

  if (m_current_tool == ToolType::Select) {
    const HitResult hover = hit_test(pos);
    setCursor(hover.element_type >= 0 ? Qt::SizeAllCursor : Qt::OpenHandCursor);
    return;
  }

  setCursor(Qt::ArrowCursor);
}

bool MapCanvas::is_forced_pan_gesture(const QMouseEvent* event) const {
  return event->button() == Qt::MiddleButton ||
         (event->button() == Qt::LeftButton &&
          (((event->modifiers() & Qt::ControlModifier) != 0U) || m_space_pan_active));
}

void MapCanvas::clear_selection() {
  m_selected_type = -1;
  m_selected_index = -1;
  emit selection_changed(-1, -1);
  update();
}

void MapCanvas::set_current_player_id(int id) {
  m_current_player_id = id;
}

QPointF MapCanvas::map_to_grid(const QPoint& widget_pos) const {
  if (m_map_data == nullptr) {
    float const x = (widget_pos.x() - m_pan_offset.x()) / (grid_cell_size * m_zoom);
    float const z = (widget_pos.y() - m_pan_offset.y()) / (grid_cell_size * m_zoom);
    return {x, z};
  }

  return CanvasTransform::widget_to_grid(widget_pos,
                                         m_map_data->grid(),
                                         m_zoom,
                                         m_pan_offset,
                                         static_cast<float>(grid_cell_size));
}

QPoint MapCanvas::grid_to_widget(float grid_x, float grid_z) const {
  if (m_map_data == nullptr) {
    float const x =
        grid_x * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.x());
    float const y =
        grid_z * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.y());
    return {static_cast<int>(x), static_cast<int>(y)};
  }

  return CanvasTransform::grid_to_widget(grid_x,
                                         grid_z,
                                         m_map_data->grid(),
                                         m_zoom,
                                         m_pan_offset,
                                         static_cast<float>(grid_cell_size));
}

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
  draw_linear_elements(painter);
  draw_terrain_elements(painter);
  draw_world_props(painter);
  draw_structures(painter);
  draw_troop_spawns(painter);
  draw_undead_zones(painter);
  draw_current_placement(painter);

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

void MapCanvas::draw_grid(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  float const cell_size = grid_cell_size * m_zoom;

  if (cell_size < 2) {
    return;
  }

  painter.setPen(QPen(k_grid_line_color, 1));

  auto const start_x = static_cast<float>(m_pan_offset.x());
  auto const start_y = static_cast<float>(m_pan_offset.y());
  float const end_x = start_x + grid.width * cell_size;
  float const end_y = start_y + grid.height * cell_size;

  for (int i = 0; i <= grid.width; i += 10) {
    float const x = start_x + i * cell_size;
    if (x >= 0 && x <= width()) {
      painter.drawLine(QPointF(x, std::max(0.0F, start_y)),
                       QPointF(x, std::min(static_cast<float>(height()), end_y)));
    }
  }

  for (int i = 0; i <= grid.height; i += 10) {
    float const y = start_y + i * cell_size;
    if (y >= 0 && y <= height()) {
      painter.drawLine(QPointF(std::max(0.0F, start_x), y),
                       QPointF(std::min(static_cast<float>(width()), end_x), y));
    }
  }

  painter.setPen(QPen(k_grid_outline_color, 2));
  painter.drawRect(
      QRectF(start_x, start_y, grid.width * cell_size, grid.height * cell_size));

  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);
  painter.setPen(k_grid_text_color);

  const QPoint top_left =
      grid_to_widget(static_cast<float>(grid.width), static_cast<float>(grid.height));
  const QPoint top_right = grid_to_widget(0.0F, static_cast<float>(grid.height));
  const QPoint bottom_left = grid_to_widget(static_cast<float>(grid.width), 0.0F);
  const QPoint bottom_right = grid_to_widget(0.0F, 0.0F);

  painter.drawText(QPointF(top_left.x() + 2, top_left.y() + 12),
                   QString("%1,%2").arg(grid.width).arg(grid.height));
  painter.drawText(QPointF(top_right.x() - 30, top_right.y() + 12),
                   QString("0,%1").arg(grid.height));
  painter.drawText(QPointF(bottom_left.x() + 2, bottom_left.y() - 4),
                   QString("%1,0").arg(grid.width));
  painter.drawText(QPointF(bottom_right.x() - 20, bottom_right.y() - 4), "0,0");
}

void MapCanvas::draw_terrain_elements(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& terrain = m_map_data->terrain_elements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto& elem = terrain[i];
    QPoint const pos = grid_to_widget(elem.x, elem.z);

    bool const is_selected = (m_selected_type == 0 && m_selected_index == i);
    bool const is_hovered = (m_hovered_type == 0 && m_hovered_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else if (is_hovered) {
      painter.setPen(QPen(hover_ring_color, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    draw_element(painter, elem.type, pos);

    if (is_hovered && !is_selected) {
      painter.save();
      painter.setPen(QPen(hover_ring_color, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(pos, icon_size / 2 + 4, icon_size / 2 + 4);
      painter.restore();
    }
  }
}

void MapCanvas::draw_world_props(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& world_props = m_map_data->world_props();
  for (int i = 0; i < world_props.size(); ++i) {
    const auto& elem = world_props[i];
    QPoint const pos = grid_to_widget(elem.x, elem.z);

    bool const is_selected = (m_selected_type == 1 && m_selected_index == i);
    bool const is_hovered = (m_hovered_type == 1 && m_hovered_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else if (is_hovered) {
      painter.setPen(QPen(hover_ring_color, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    draw_element(painter, elem.type, pos);

    if (is_hovered && !is_selected) {
      painter.save();
      painter.setPen(QPen(hover_ring_color, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(pos, icon_size / 2 + 4, icon_size / 2 + 4);
      painter.restore();
    }
  }
}

void MapCanvas::draw_structures(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& structures = m_map_data->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto& elem = structures[i];
    QPoint const pos = grid_to_widget(elem.x, elem.z);

    bool const is_selected = (m_selected_type == 3 && m_selected_index == i);
    bool const is_hovered = (m_hovered_type == 3 && m_hovered_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else if (is_hovered) {
      painter.setPen(QPen(hover_ring_color, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    draw_element(painter, elem.type, pos, elem.player_id);

    if (is_hovered && !is_selected) {
      painter.save();
      painter.setPen(QPen(hover_ring_color, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(pos, icon_size / 2 + 4, icon_size / 2 + 4);
      painter.restore();
    }
  }
}

void MapCanvas::draw_troop_spawns(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& troop_spawns = m_map_data->troop_spawns();
  for (int i = 0; i < troop_spawns.size(); ++i) {
    const auto& elem = troop_spawns[i];
    const QPoint pos = grid_to_widget(elem.x, elem.z);

    const bool is_selected = (m_selected_type == 4 && m_selected_index == i);
    const bool is_hovered = (m_hovered_type == 4 && m_hovered_index == i);
    if (is_selected) {
      painter.save();
      painter.setPen(QPen(Qt::yellow, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(pos, icon_size / 2 + 6, icon_size / 2 + 6);
      painter.restore();
    } else if (is_hovered) {
      painter.save();
      painter.setPen(QPen(hover_ring_color, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(pos, icon_size / 2 + 6, icon_size / 2 + 6);
      painter.restore();
    }

    draw_troop_marker(painter, pos, elem.type, elem.player_id);
  }
}

void MapCanvas::draw_linear_elements(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& linear = m_map_data->linear_elements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto& elem = linear[i];
    QPoint const start_pos = grid_to_widget(elem.start.x(), elem.start.y());
    QPoint const end_pos = grid_to_widget(elem.end.x(), elem.end.y());

    bool const is_selected = (m_selected_type == 2 && m_selected_index == i);
    bool const is_hovered = (m_hovered_type == 2 && m_hovered_index == i);

    QColor color;
    int line_width = static_cast<int>(elem.width * m_zoom);
    line_width = std::max(2, std::min(line_width, 20));

    if (elem.type == "river") {
      color = QColor(70, 130, 200);
    } else if (elem.type == "road") {
      color = QColor(139, 119, 101);
    } else if (elem.type == "bridge") {
      color = QColor(160, 140, 100);
    }

    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, line_width + 4));
      painter.drawLine(start_pos, end_pos);
    } else if (is_hovered) {
      painter.setPen(QPen(hover_ring_color, line_width + 4));
      painter.drawLine(start_pos, end_pos);
    }

    painter.setPen(QPen(color, line_width));
    painter.drawLine(start_pos, end_pos);

    int const endpoint_size = 6;
    painter.setBrush(color.lighter());
    painter.setPen(Qt::white);
    painter.drawEllipse(start_pos, endpoint_size, endpoint_size);
    painter.drawEllipse(end_pos, endpoint_size, endpoint_size);
  }

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

void MapCanvas::draw_undead_zones(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;

  static const QColor k_zone_fill(100, 40, 140, 55);
  static const QColor k_zone_border(180, 80, 220, 200);
  static const QColor k_leash_ring(140, 100, 180, 80);

  const auto& zones = m_map_data->undead_zones();
  for (int i = 0; i < zones.size(); ++i) {
    const auto& elem = zones[i];
    QPoint const center = grid_to_widget(elem.x, elem.z);

    bool const is_selected = (m_selected_type == 5 && m_selected_index == i);
    bool const is_hovered = (m_hovered_type == 5 && m_hovered_index == i);

    const int radius_px = static_cast<int>(elem.radius * m_zoom * grid_cell_size);
    const int leash_px = static_cast<int>(elem.leash_radius * m_zoom * grid_cell_size);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Leash radius ring (dotted)
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(k_leash_ring, 1, Qt::DotLine));
    painter.drawEllipse(center, leash_px, leash_px);

    // Fill zone circle
    painter.setBrush(k_zone_fill);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else if (is_hovered) {
      painter.setPen(QPen(hover_ring_color, 2));
    } else {
      painter.setPen(QPen(k_zone_border, 1));
    }
    painter.drawEllipse(center, radius_px, radius_px);

    // Anchor icon in the center
    const QString icon = (elem.anchor_type == QStringLiteral("ruins")) ? "🏚" : "✦";
    painter.setPen(QColor(230, 180, 255));
    QFont f = painter.font();
    f.setPointSize(9);
    painter.setFont(f);
    painter.drawText(
        QRect(center.x() - 10, center.y() - 10, 20, 20), Qt::AlignCenter, icon);

    // Zone id label below
    if (!elem.id.isEmpty()) {
      f.setPointSize(7);
      painter.setFont(f);
      painter.setPen(QColor(210, 170, 240, 200));
      painter.drawText(QRect(center.x() - 40, center.y() + radius_px + 2, 80, 14),
                       Qt::AlignCenter,
                       elem.id);
    }

    painter.restore();
  }
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

  QString type;
  switch (m_current_tool) {
  case ToolType::Hill:
    type = "hill";
    break;
  case ToolType::Mountain:
    type = "mountain";
    break;
  case ToolType::PropFirecamp:
  case ToolType::PropTent:
  case ToolType::PropSupplyCart:
  case ToolType::PropWeaponRack:
  case ToolType::PropRuins:
  case ToolType::PropMagicShrine:
  case ToolType::PropDeadTree:
  case ToolType::PropBoulder:
    type = world_prop_type_for_tool(m_current_tool);
    break;
  case ToolType::Barracks:
    type = "barracks";
    break;
  case ToolType::Village:
    type = "village";
    break;
  default:
    break;
  }

  if (is_troop_tool(m_current_tool)) {
    draw_troop_marker(
        painter, widget_pos, troop_type_for_tool(m_current_tool), m_current_player_id);
  } else if (m_current_tool == ToolType::UndeadZone) {
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
  } else if (!type.isEmpty()) {
    draw_element(painter, type, widget_pos);
  }

  painter.setOpacity(1.0);
}

void MapCanvas::draw_element(QPainter& painter,
                             const QString& type,
                             const QPoint& pos,
                             int player_id) {

  int const size = icon_size;

  QColor fill_color;
  QString symbol;

  if (type == "hill") {
    fill_color = QColor(139, 137, 112);
    symbol = "⛰";
  } else if (type == "mountain") {
    fill_color = QColor(105, 105, 105);
    symbol = "🏔";
  } else if (type == "firecamp") {
    fill_color = QColor(255, 140, 0);
    symbol = "🔥";
  } else if (type == "tent") {
    fill_color = QColor(176, 126, 78);
    symbol = "⛺";
  } else if (type == "supply_cart") {
    fill_color = QColor(140, 106, 64);
    symbol = "🛒";
  } else if (type == "weapon_rack") {
    fill_color = QColor(120, 120, 132);
    symbol = "⚔";
  } else if (type == "ruins") {
    fill_color = QColor(102, 98, 90);
    symbol = "🏚";
  } else if (type == "magic_shrine") {
    fill_color = QColor(118, 96, 196);
    symbol = "✦";
  } else if (type == "dead_tree") {
    fill_color = QColor(111, 86, 67);
    symbol = "🌲";
  } else if (type == "boulder") {
    fill_color = QColor(110, 110, 110);
    symbol = "🪨";
  } else if (type == "barracks") {
    fill_color = player_color_for_editor(player_id);
    symbol = "🏛";
  } else if (type == "village") {
    fill_color = player_color_for_editor(player_id);
    symbol = "🏘";
  } else {
    fill_color = QColor(128, 128, 128);
    symbol = "?";
  }

  painter.setBrush(fill_color);
  painter.drawEllipse(pos, size, size);

  QFont font = painter.font();
  font.setPointSize(12);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(QRect(pos.x() - size, pos.y() - size, size * 2, size * 2),
                   Qt::AlignCenter,
                   symbol);

  if ((type == "barracks" || type == "village") && player_id >= 0) {
    QString const player_text = player_id == 0 ? "N" : QString::number(player_id);
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pos.x() + size - 6, pos.y() - size + 10, player_text);
  }
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
  m_last_mouse_pos = event->pos();

  if (event->button() == Qt::RightButton) {
    clear_tool();
    return;
  }

  if (is_forced_pan_gesture(event)) {
    begin_panning(event->pos());
    return;
  }

  if (event->button() == Qt::LeftButton && (m_map_data != nullptr)) {
    QPointF const raw_pos = map_to_grid(event->pos());
    const bool shift_held =
        event->modifiers() &
        static_cast<int>(
            static_cast<int>(static_cast<unsigned int>(
                                 static_cast<int>(static_cast<unsigned int>(
                                                      Qt::ShiftModifier != 0U) != 0U) !=
                                 0U) != 0U) != 0U != 0u);
    QPointF const grid_pos = shift_held ? raw_pos : snap_pos(raw_pos);

    switch (m_current_tool) {
    case ToolType::Select: {
      HitResult const hit = hit_test(event->pos());
      m_selected_type = hit.element_type;
      m_selected_index = hit.index;
      m_dragged_endpoint = hit.endpoint;
      emit selection_changed(m_selected_type, m_selected_index);
      if (hit.element_type >= 0) {
        m_is_dragging = true;
        m_did_drag_move = false;
        update_canvas_cursor(event->pos());
        if (hit.element_type == 0 &&
            hit.index < m_map_data->terrain_elements().size()) {
          m_drag_pre_terrain = m_map_data->terrain_elements()[hit.index];
        } else if (hit.element_type == 1 &&
                   hit.index < m_map_data->world_props().size()) {
          m_drag_pre_world_prop = m_map_data->world_props()[hit.index];
        } else if (hit.element_type == 2 &&
                   hit.index < m_map_data->linear_elements().size()) {
          m_drag_pre_linear = m_map_data->linear_elements()[hit.index];
        } else if (hit.element_type == 3 &&
                   hit.index < m_map_data->structures().size()) {
          m_drag_pre_structure = m_map_data->structures()[hit.index];
        } else if (hit.element_type == 4 &&
                   hit.index < m_map_data->troop_spawns().size()) {
          m_drag_pre_troop = m_map_data->troop_spawns()[hit.index];
        } else if (hit.element_type == 5 &&
                   hit.index < m_map_data->undead_zones().size()) {
          m_drag_pre_undead_zone = m_map_data->undead_zones()[hit.index];
        }
      } else {
        m_is_pan_drag_pending = true;
        m_pan_press_pos = event->pos();
        update_canvas_cursor(event->pos());
      }
      update();
      break;
    }
    case ToolType::Hill:
    case ToolType::Mountain:
    case ToolType::PropFirecamp:
    case ToolType::PropTent:
    case ToolType::PropSupplyCart:
    case ToolType::PropWeaponRack:
    case ToolType::PropRuins:
    case ToolType::PropMagicShrine:
    case ToolType::PropDeadTree:
    case ToolType::PropBoulder:
    case ToolType::Barracks:
    case ToolType::Village:
    case ToolType::TroopArcher:
    case ToolType::TroopSwordsman:
    case ToolType::TroopSpearman:
    case ToolType::TroopHorseSwordsman:
    case ToolType::TroopHorseArcher:
    case ToolType::TroopHorseSpearman:
    case ToolType::TroopHealer:
    case ToolType::TroopCatapult:
    case ToolType::TroopBallista:
    case ToolType::TroopElephant:
    case ToolType::TroopRomanLegionOrganizer:
    case ToolType::TroopRomanVeteranConsul:
    case ToolType::TroopRomanFieldCommander:
    case ToolType::TroopCarthageMercenaryBroker:
    case ToolType::TroopCarthageCavalryPatron:
    case ToolType::TroopCarthageElephantMaster:
    case ToolType::TroopSkeletonSwordsman:
    case ToolType::TroopSkeletonArcher:
    case ToolType::TroopGravePriest:
    case ToolType::TroopCivilian:
    case ToolType::TroopBuilder:
    case ToolType::UndeadZone:
      place_element(grid_pos);
      break;
    case ToolType::River:
    case ToolType::Road:
    case ToolType::Bridge:
      if (!m_is_placing_linear) {
        start_linear_element(grid_pos);
      } else {
        finish_linear_element(grid_pos);
      }
      break;
    case ToolType::Eraser:
      erase_at_position(grid_pos);
      break;
    }
  }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton && m_is_panning)) {
    finish_panning(event->pos());
  }

  if (event->button() == Qt::LeftButton && m_is_dragging && m_did_drag_move &&
      (m_map_data != nullptr) && m_selected_type >= 0 && m_selected_index >= 0) {
    if (m_selected_type == 0) {
      const auto& terrain = m_map_data->terrain_elements();
      if (m_selected_index < terrain.size()) {
        const TerrainElement& current = terrain[m_selected_index];
        const bool moved =
            (m_drag_pre_terrain.x != current.x) || (m_drag_pre_terrain.z != current.z);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateTerrainCmd>(m_map_data,
                                                 m_selected_index,
                                                 m_drag_pre_terrain,
                                                 current,
                                                 "Move " + current.type));
        }
      }
    } else if (m_selected_type == 1) {
      const auto& world_props = m_map_data->world_props();
      if (m_selected_index < world_props.size()) {
        const WorldPropElement& current = world_props[m_selected_index];
        const bool moved = (m_drag_pre_world_prop.x != current.x) ||
                           (m_drag_pre_world_prop.z != current.z);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateWorldPropCmd>(m_map_data,
                                                   m_selected_index,
                                                   m_drag_pre_world_prop,
                                                   current,
                                                   "Move " + current.type));
        }
      }
    } else if (m_selected_type == 2) {
      const auto& linear = m_map_data->linear_elements();
      if (m_selected_index < linear.size()) {
        const LinearElement& current = linear[m_selected_index];
        const bool moved = (m_drag_pre_linear.start != current.start) ||
                           (m_drag_pre_linear.end != current.end);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateLinearCmd>(m_map_data,
                                                m_selected_index,
                                                m_drag_pre_linear,
                                                current,
                                                "Move " + current.type));
        }
      }
    } else if (m_selected_type == 3) {
      const auto& structures = m_map_data->structures();
      if (m_selected_index < structures.size()) {
        const StructureElement& current = structures[m_selected_index];
        const bool moved = (m_drag_pre_structure.x != current.x) ||
                           (m_drag_pre_structure.z != current.z);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateStructureCmd>(m_map_data,
                                                   m_selected_index,
                                                   m_drag_pre_structure,
                                                   current,
                                                   "Move " + current.type));
        }
      }
    } else if (m_selected_type == 4) {
      const auto& troop_spawns = m_map_data->troop_spawns();
      if (m_selected_index < troop_spawns.size()) {
        const TroopSpawnElement& current = troop_spawns[m_selected_index];
        const bool moved =
            (m_drag_pre_troop.x != current.x) || (m_drag_pre_troop.z != current.z);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateTroopSpawnCmd>(m_map_data,
                                                    m_selected_index,
                                                    m_drag_pre_troop,
                                                    current,
                                                    "Move " + current.type));
        }
      }
    } else if (m_selected_type == 5) {
      const auto& undead_zones = m_map_data->undead_zones();
      if (m_selected_index < undead_zones.size()) {
        const UndeadZoneElement& current = undead_zones[m_selected_index];
        const bool moved = (m_drag_pre_undead_zone.x != current.x) ||
                           (m_drag_pre_undead_zone.z != current.z);
        if (moved) {
          m_map_data->record_command(
              std::make_unique<UpdateUndeadZoneCmd>(m_map_data,
                                                    m_selected_index,
                                                    m_drag_pre_undead_zone,
                                                    current,
                                                    "Move undead zone"));
        }
      }
    }
  }

  if (event->button() == Qt::LeftButton) {
    m_is_pan_drag_pending = false;
  }

  m_is_dragging = false;
  m_dragged_endpoint = -1;
  m_did_drag_move = false;
  m_hovered_type = -1;
  m_hovered_index = -1;
  update_canvas_cursor(event->pos());
  update();
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event) {
  QPoint const delta = event->pos() - m_last_mouse_pos;
  m_last_mouse_pos = event->pos();

  if (m_is_pan_drag_pending) {
    const int drag_distance = (event->pos() - m_pan_press_pos).manhattanLength();
    if (drag_distance >= pan_drag_threshold) {
      begin_panning(event->pos());
      m_pan_offset += QPointF(delta.x(), delta.y());
      update();
      return;
    }
  }

  if (m_is_panning) {
    m_pan_offset += QPointF(delta.x(), delta.y());
    update();
    return;
  }

  if (m_is_dragging && (m_map_data != nullptr) && m_selected_type >= 0 &&
      m_selected_index >= 0) {
    m_did_drag_move = true;
    QPointF const raw_pos = map_to_grid(event->pos());
    const bool shift_held =
        event->modifiers() &
        static_cast<int>(
            static_cast<int>(static_cast<unsigned int>(
                                 static_cast<int>(static_cast<unsigned int>(
                                                      Qt::ShiftModifier != 0U) != 0U) !=
                                 0U) != 0U) != 0U != 0u);
    QPointF const grid_pos = shift_held ? raw_pos : snap_pos(raw_pos);

    if (m_selected_type == 0) {
      auto terrain = m_map_data->terrain_elements();
      if (m_selected_index < terrain.size()) {
        TerrainElement elem = terrain[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->update_terrain_element(m_selected_index, elem);
      }
    } else if (m_selected_type == 1) {
      auto world_props = m_map_data->world_props();
      if (m_selected_index < world_props.size()) {
        WorldPropElement elem = world_props[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->update_world_prop(m_selected_index, elem);
      }
    } else if (m_selected_type == 2) {

      auto linear = m_map_data->linear_elements();
      if (m_selected_index < linear.size() && m_dragged_endpoint >= 0) {
        LinearElement elem = linear[m_selected_index];
        QVector2D const new_pos(static_cast<float>(grid_pos.x()),
                                static_cast<float>(grid_pos.y()));

        if (m_dragged_endpoint == 0) {
          elem.start = new_pos;
        } else if (m_dragged_endpoint == 1) {
          elem.end = new_pos;
        }

        m_map_data->update_linear_element(m_selected_index, elem);
      }
    } else if (m_selected_type == 3) {
      auto structures = m_map_data->structures();
      if (m_selected_index < structures.size()) {
        StructureElement elem = structures[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->update_structure(m_selected_index, elem);
      }
    } else if (m_selected_type == 4) {
      auto troop_spawns = m_map_data->troop_spawns();
      if (m_selected_index < troop_spawns.size()) {
        TroopSpawnElement elem = troop_spawns[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->update_troop_spawn(m_selected_index, elem);
      }
    } else if (m_selected_type == 5) {
      auto undead_zones = m_map_data->undead_zones();
      if (m_selected_index < undead_zones.size()) {
        UndeadZoneElement elem = undead_zones[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->update_undead_zone(m_selected_index, elem);
      }
    }
  }

  if (!m_is_dragging && !m_is_panning && (m_map_data != nullptr) &&
      (m_current_tool == ToolType::Select || m_current_tool == ToolType::Eraser)) {
    HitResult const hover_hit = hit_test(event->pos());
    int const new_hovered_type = hover_hit.element_type;
    int const new_hovered_index = hover_hit.index;
    if (new_hovered_type != m_hovered_type || new_hovered_index != m_hovered_index) {
      m_hovered_type = new_hovered_type;
      m_hovered_index = new_hovered_index;
      update();
    }
  }

  QPointF const cursor_grid = map_to_grid(event->pos());
  emit cursor_moved(static_cast<int>(std::floor(cursor_grid.x())),
                    static_cast<int>(std::floor(cursor_grid.y())));

  if (m_current_tool == ToolType::Select && !m_is_dragging && !m_is_panning) {
    update_canvas_cursor(event->pos());
  } else if (m_current_tool != ToolType::Select) {
    update();
  }
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    HitResult const hit = hit_test(event->pos());
    if (hit.element_type >= 0 && hit.index >= 0) {
      emit element_double_clicked(hit.element_type, hit.index);
    } else {

      emit grid_double_clicked();
    }
  }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
  float const old_zoom = m_zoom;

  if (event->angleDelta().y() > 0) {
    m_zoom *= 1.1F;
  } else {
    m_zoom /= 1.1F;
  }

  m_zoom = std::clamp(m_zoom, 0.1F, 5.0F);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QPointF const cursor_pos = event->position();
#else
  QPointF cursor_pos = event->posF();
#endif
  m_pan_offset = cursor_pos - (cursor_pos - m_pan_offset) * (m_zoom / old_zoom);

  emit zoom_changed(m_zoom);
  update();
}

void MapCanvas::resizeEvent(QResizeEvent*) {

  if (m_pan_offset.isNull() && (m_map_data != nullptr)) {
    m_pan_offset = QPointF(50, 50);
  }
}

MapCanvas::HitResult MapCanvas::hit_test(const QPoint& pos) const {
  HitResult result;
  if (m_map_data == nullptr) {
    return result;
  }

  const QVector2D cursor(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
  const float point_hit_radius_px = static_cast<float>(icon_size) + 4.0F;
  float best_dist = std::numeric_limits<float>::infinity();
  int best_priority = std::numeric_limits<int>::max();

  auto consider_hit = [&](int element_type,
                          int index,
                          int endpoint,
                          float distance,
                          float max_distance,
                          int priority) {
    if (distance > max_distance) {
      return;
    }
    if (distance < best_dist ||
        ((std::abs(distance - best_dist) < 0.01F) && priority < best_priority)) {
      best_dist = distance;
      best_priority = priority;
      result.element_type = element_type;
      result.index = index;
      result.endpoint = endpoint;
    }
  };

  const auto& troop_spawns = m_map_data->troop_spawns();
  for (int i = troop_spawns.size() - 1; i >= 0; --i) {
    const auto& elem = troop_spawns[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(4, i, -1, (cursor - center_vec).length(), point_hit_radius_px, 0);
  }

  // Undead zones: hit if cursor is within radius circle
  const auto& undead_zones = m_map_data->undead_zones();
  for (int i = undead_zones.size() - 1; i >= 0; --i) {
    const auto& elem = undead_zones[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    const float zone_radius_px = elem.radius * m_zoom * grid_cell_size;
    consider_hit(5, i, -1, (cursor - center_vec).length(), zone_radius_px + 4.0F, 6);
  }

  const auto& structures = m_map_data->structures();
  for (int i = structures.size() - 1; i >= 0; --i) {
    const auto& elem = structures[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(3, i, -1, (cursor - center_vec).length(), point_hit_radius_px, 1);
  }

  const auto& world_props = m_map_data->world_props();
  for (int i = world_props.size() - 1; i >= 0; --i) {
    const auto& elem = world_props[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(1, i, -1, (cursor - center_vec).length(), point_hit_radius_px, 2);
  }

  const auto& terrain = m_map_data->terrain_elements();
  for (int i = terrain.size() - 1; i >= 0; --i) {
    const auto& elem = terrain[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(0, i, -1, (cursor - center_vec).length(), point_hit_radius_px, 3);
  }

  const auto& linear = m_map_data->linear_elements();
  for (int i = linear.size() - 1; i >= 0; --i) {
    const auto& elem = linear[i];
    const QPoint start_pos = grid_to_widget(elem.start.x(), elem.start.y());
    const QPoint end_pos = grid_to_widget(elem.end.x(), elem.end.y());
    const QVector2D start_vec(static_cast<float>(start_pos.x()),
                              static_cast<float>(start_pos.y()));
    const QVector2D end_vec(static_cast<float>(end_pos.x()),
                            static_cast<float>(end_pos.y()));

    consider_hit(2, i, 0, (cursor - start_vec).length(), point_hit_radius_px, 4);
    consider_hit(2, i, 1, (cursor - end_vec).length(), point_hit_radius_px, 4);
  }

  for (int i = linear.size() - 1; i >= 0; --i) {
    const auto& elem = linear[i];
    const QPoint start_pos = grid_to_widget(elem.start.x(), elem.start.y());
    const QPoint end_pos = grid_to_widget(elem.end.x(), elem.end.y());

    const QVector2D a(static_cast<float>(start_pos.x()),
                      static_cast<float>(start_pos.y()));
    const QVector2D b(static_cast<float>(end_pos.x()), static_cast<float>(end_pos.y()));
    const QVector2D ab = b - a;
    const float ab_length_sq = QVector2D::dotProduct(ab, ab);

    float dist = std::numeric_limits<float>::infinity();
    if (ab_length_sq < 0.0001F) {
      dist = (cursor - a).length();
    } else {
      const float t =
          std::clamp(QVector2D::dotProduct(cursor - a, ab) / ab_length_sq, 0.0F, 1.0F);
      const QVector2D closest = a + t * ab;
      dist = (cursor - closest).length();
    }

    int line_width_px = static_cast<int>(elem.width * m_zoom);
    line_width_px = std::max(2, std::min(line_width_px, 20));
    const float line_hit_radius_px = static_cast<float>(line_width_px) * 0.5F + 4.0F;
    consider_hit(2, i, -1, dist, line_hit_radius_px, 5);
  }

  return result;
}

void MapCanvas::place_element(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  if (m_current_tool == ToolType::Hill || m_current_tool == ToolType::Mountain) {
    TerrainElement elem;
    elem.type = (m_current_tool == ToolType::Hill) ? "hill" : "mountain";
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.radius = 10.0F;
    elem.height = (m_current_tool == ToolType::Hill) ? 3.0F : 8.0F;
    m_map_data->execute_command(std::make_unique<AddTerrainCmd>(m_map_data, elem));
  } else if (!world_prop_type_for_tool(m_current_tool).isEmpty()) {
    WorldPropElement elem;
    elem.type = world_prop_type_for_tool(m_current_tool);
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    if (elem.type == QStringLiteral("firecamp")) {
      elem.intensity = 1.0F;
      elem.radius = 3.0F;
    } else {
      elem.scale = 1.0F;
      elem.rotation = 0.0F;
    }
    m_map_data->execute_command(std::make_unique<AddWorldPropCmd>(m_map_data, elem));
  } else if (m_current_tool == ToolType::Barracks ||
             m_current_tool == ToolType::Village) {
    StructureElement elem;
    elem.type = (m_current_tool == ToolType::Barracks) ? "barracks" : "village";
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.player_id = m_current_player_id;
    elem.max_population = default_max_population;
    elem.nation = default_nation;
    m_map_data->execute_command(std::make_unique<AddStructureCmd>(m_map_data, elem));
  } else if (is_troop_tool(m_current_tool)) {
    TroopSpawnElement elem;
    elem.type = troop_type_for_tool(m_current_tool);
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.player_id = m_current_player_id;
    elem.max_population = default_troop_max_population;
    m_map_data->execute_command(std::make_unique<AddTroopSpawnCmd>(m_map_data, elem));
  } else if (m_current_tool == ToolType::UndeadZone) {
    UndeadZoneElement elem;
    static int zone_counter = 0;
    elem.id = QStringLiteral("zone_%1").arg(++zone_counter);
    elem.anchor_type = QStringLiteral("magic_shrine");
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.radius = 8.0F;
    elem.leash_radius = 14.0F;
    elem.owner_id = 99;
    elem.team_id = 99;
    elem.awaken_on = QJsonArray{QStringLiteral("unit_enters_radius")};
    // Default wave: 2 skeleton swordsmen + 1 grave priest
    QJsonObject units_obj;
    units_obj[QStringLiteral("skeleton_swordsman")] = 2;
    units_obj[QStringLiteral("grave_priest")] = 1;
    QJsonObject wave;
    wave[QStringLiteral("trigger")] = QStringLiteral("initial");
    wave[QStringLiteral("units")] = units_obj;
    elem.waves = QJsonArray{wave};
    m_map_data->execute_command(std::make_unique<AddUndeadZoneCmd>(m_map_data, elem));
  }
}

void MapCanvas::start_linear_element(const QPointF& grid_pos) {
  m_is_placing_linear = true;
  m_linear_start = grid_pos;

  QString type_name;
  if (m_current_tool == ToolType::River) {
    type_name = "river";
  } else if (m_current_tool == ToolType::Road) {
    type_name = "road";
  } else if (m_current_tool == ToolType::Bridge) {
    type_name = "bridge";
  }
  emit status_hint_changed("Drawing " + type_name +
                           " \u2014 click to place end point"
                           " (right-click to cancel)");
}

void MapCanvas::finish_linear_element(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  LinearElement elem;
  elem.start = QVector2D(static_cast<float>(m_linear_start.x()),
                         static_cast<float>(m_linear_start.y()));
  elem.end =
      QVector2D(static_cast<float>(grid_pos.x()), static_cast<float>(grid_pos.y()));

  switch (m_current_tool) {
  case ToolType::River:
    elem.type = "river";
    elem.width = 3.0F;
    break;
  case ToolType::Road:
    elem.type = "road";
    elem.width = 3.0F;
    elem.style = "default";
    break;
  case ToolType::Bridge:
    elem.type = "bridge";
    elem.width = 4.0F;
    elem.height = 0.5F;
    break;
  default:
    break;
  }

  m_map_data->execute_command(std::make_unique<AddLinearCmd>(m_map_data, elem));
  m_is_placing_linear = false;
  emit status_hint_changed("");
}

void MapCanvas::erase_at_position(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  HitResult const hit = hit_test(grid_to_widget(static_cast<float>(grid_pos.x()),
                                                static_cast<float>(grid_pos.y())));

  if (hit.element_type == 0 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveTerrainCmd>(
        m_map_data, hit.index, m_map_data->terrain_elements()[hit.index]));
  } else if (hit.element_type == 1 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveWorldPropCmd>(
        m_map_data, hit.index, m_map_data->world_props()[hit.index]));
  } else if (hit.element_type == 2 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveLinearCmd>(
        m_map_data, hit.index, m_map_data->linear_elements()[hit.index]));
  } else if (hit.element_type == 3 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveStructureCmd>(
        m_map_data, hit.index, m_map_data->structures()[hit.index]));
  } else if (hit.element_type == 4 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveTroopSpawnCmd>(
        m_map_data, hit.index, m_map_data->troop_spawns()[hit.index]));
  } else if (hit.element_type == 5 && hit.index >= 0) {
    m_map_data->execute_command(std::make_unique<RemoveUndeadZoneCmd>(
        m_map_data, hit.index, m_map_data->undead_zones()[hit.index]));
  }
}

void MapCanvas::keyPressEvent(QKeyEvent* event) {
  switch (event->key()) {
  case Qt::Key_Space:
    if (!event->isAutoRepeat()) {
      m_space_pan_active = true;
      update_canvas_cursor(m_last_mouse_pos);
    }
    event->accept();
    break;
  case Qt::Key_Delete:
  case Qt::Key_Backspace:
    if ((m_map_data != nullptr) && m_selected_type >= 0 && m_selected_index >= 0) {
      if (m_selected_type == 0) {
        const auto& terrain = m_map_data->terrain_elements();
        if (m_selected_index < terrain.size()) {
          m_map_data->execute_command(std::make_unique<RemoveTerrainCmd>(
              m_map_data, m_selected_index, terrain[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      } else if (m_selected_type == 1) {
        const auto& world_props = m_map_data->world_props();
        if (m_selected_index < world_props.size()) {
          m_map_data->execute_command(std::make_unique<RemoveWorldPropCmd>(
              m_map_data, m_selected_index, world_props[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      } else if (m_selected_type == 2) {
        const auto& linear = m_map_data->linear_elements();
        if (m_selected_index < linear.size()) {
          m_map_data->execute_command(std::make_unique<RemoveLinearCmd>(
              m_map_data, m_selected_index, linear[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      } else if (m_selected_type == 3) {
        const auto& structures = m_map_data->structures();
        if (m_selected_index < structures.size()) {
          m_map_data->execute_command(std::make_unique<RemoveStructureCmd>(
              m_map_data, m_selected_index, structures[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      } else if (m_selected_type == 4) {
        const auto& troop_spawns = m_map_data->troop_spawns();
        if (m_selected_index < troop_spawns.size()) {
          m_map_data->execute_command(std::make_unique<RemoveTroopSpawnCmd>(
              m_map_data, m_selected_index, troop_spawns[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      } else if (m_selected_type == 5) {
        const auto& undead_zones = m_map_data->undead_zones();
        if (m_selected_index < undead_zones.size()) {
          m_map_data->execute_command(std::make_unique<RemoveUndeadZoneCmd>(
              m_map_data, m_selected_index, undead_zones[m_selected_index]));
          m_selected_type = -1;
          m_selected_index = -1;
          emit selection_changed(-1, -1);
        }
      }
    }
    break;
  case Qt::Key_Escape:
    if (m_is_placing_linear) {
      m_is_placing_linear = false;
      emit status_hint_changed("");
      update();
    } else if (m_is_panning || m_is_pan_drag_pending) {
      finish_panning(m_last_mouse_pos);
      emit status_hint_changed("");
      update();
    } else if (m_current_tool != ToolType::Select) {
      clear_tool();
    } else {
      m_selected_type = -1;
      m_selected_index = -1;
      emit selection_changed(-1, -1);
      update();
    }
    break;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

void MapCanvas::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
    m_space_pan_active = false;
    if (!m_is_panning) {
      update_canvas_cursor(m_last_mouse_pos);
    }
    event->accept();
    return;
  }

  QWidget::keyReleaseEvent(event);
}

} // namespace MapEditor
