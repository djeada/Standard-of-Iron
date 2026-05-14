#include "map_canvas.h"

#include <QApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

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
  return QPointF(std::round(gp.x()), std::round(gp.y()));
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
  case ToolType::PropDeadTree:
    return QStringLiteral("dead_tree");
  case ToolType::PropBoulder:
    return QStringLiteral("boulder");
  default:
    return {};
  }
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
  if (m_map_data) {
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
          ((event->modifiers() & Qt::ControlModifier) || m_space_pan_active));
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
  float x = (widget_pos.x() - m_pan_offset.x()) / (grid_cell_size * m_zoom);
  float z = (widget_pos.y() - m_pan_offset.y()) / (grid_cell_size * m_zoom);
  return QPointF(x, z);
}

QPoint MapCanvas::grid_to_widget(float grid_x, float grid_z) const {
  float x = grid_x * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.x());
  float y = grid_z * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.y());
  return QPoint(static_cast<int>(x), static_cast<int>(y));
}

void MapCanvas::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.fillRect(rect(), k_canvas_background);
  painter.setPen(QPen(k_canvas_border, 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  if (!m_map_data) {
    painter.setPen(k_empty_state_text);
    painter.drawText(rect(), Qt::AlignCenter, "No map loaded");
    return;
  }

  draw_grid(painter);
  draw_linear_elements(painter);
  draw_terrain_elements(painter);
  draw_world_props(painter);
  draw_structures(painter);
  draw_current_placement(painter);

  const bool is_empty =
      m_map_data->terrain_elements().isEmpty() && m_map_data->world_props().isEmpty() &&
      m_map_data->linear_elements().isEmpty() && m_map_data->structures().isEmpty();
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
  if (!m_map_data) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  float cell_size = grid_cell_size * m_zoom;

  if (cell_size < 2) {
    return;
  }

  painter.setPen(QPen(k_grid_line_color, 1));

  float start_x = static_cast<float>(m_pan_offset.x());
  float start_y = static_cast<float>(m_pan_offset.y());
  float end_x = start_x + grid.width * cell_size;
  float end_y = start_y + grid.height * cell_size;

  for (int i = 0; i <= grid.width; i += 10) {
    float x = start_x + i * cell_size;
    if (x >= 0 && x <= width()) {
      painter.drawLine(QPointF(x, std::max(0.0F, start_y)),
                       QPointF(x, std::min(static_cast<float>(height()), end_y)));
    }
  }

  for (int i = 0; i <= grid.height; i += 10) {
    float y = start_y + i * cell_size;
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

  painter.drawText(QPointF(start_x + 2, start_y + 12), "0,0");

  QString top_right = QString("%1,0").arg(grid.width);
  painter.drawText(QPointF(end_x - 30, start_y + 12), top_right);

  QString bottom_left = QString("0,%1").arg(grid.height);
  painter.drawText(QPointF(start_x + 2, end_y - 4), bottom_left);

  QString bottom_right = QString("%1,%2").arg(grid.width).arg(grid.height);
  painter.drawText(QPointF(end_x - 40, end_y - 4), bottom_right);
}

void MapCanvas::draw_terrain_elements(QPainter& painter) {
  if (!m_map_data) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& terrain = m_map_data->terrain_elements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto& elem = terrain[i];
    QPoint pos = grid_to_widget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 0 && m_selected_index == i);
    bool is_hovered = (m_hovered_type == 0 && m_hovered_index == i);
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
  if (!m_map_data) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& world_props = m_map_data->world_props();
  for (int i = 0; i < world_props.size(); ++i) {
    const auto& elem = world_props[i];
    QPoint pos = grid_to_widget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 1 && m_selected_index == i);
    bool is_hovered = (m_hovered_type == 1 && m_hovered_index == i);
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
  if (!m_map_data) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& structures = m_map_data->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto& elem = structures[i];
    QPoint pos = grid_to_widget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 3 && m_selected_index == i);
    bool is_hovered = (m_hovered_type == 3 && m_hovered_index == i);
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

void MapCanvas::draw_linear_elements(QPainter& painter) {
  if (!m_map_data) {
    return;
  }

  const bool is_eraser = (m_current_tool == ToolType::Eraser);
  const QColor& hover_ring_color =
      is_eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& linear = m_map_data->linear_elements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto& elem = linear[i];
    QPoint start_pos = grid_to_widget(elem.start.x(), elem.start.y());
    QPoint end_pos = grid_to_widget(elem.end.x(), elem.end.y());

    bool is_selected = (m_selected_type == 2 && m_selected_index == i);
    bool is_hovered = (m_hovered_type == 2 && m_hovered_index == i);

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

    int endpoint_size = 6;
    painter.setBrush(color.lighter());
    painter.setPen(Qt::white);
    painter.drawEllipse(start_pos, endpoint_size, endpoint_size);
    painter.drawEllipse(end_pos, endpoint_size, endpoint_size);
  }

  if (m_is_placing_linear) {
    QPoint start_pos = grid_to_widget(static_cast<float>(m_linear_start.x()),
                                      static_cast<float>(m_linear_start.y()));
    QPointF current_grid = map_to_grid(m_last_mouse_pos);
    QPoint end_pos = grid_to_widget(static_cast<float>(current_grid.x()),
                                    static_cast<float>(current_grid.y()));

    QPen pen(Qt::white, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(start_pos, end_pos);

    painter.save();
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(100, 200, 255, 180));
    painter.drawEllipse(start_pos, 7, 7);
    painter.restore();
  }
}

void MapCanvas::draw_current_placement(QPainter& painter) {
  if (m_current_tool == ToolType::Select || m_current_tool == ToolType::Eraser) {
    return;
  }

  QPointF grid_pos = map_to_grid(m_last_mouse_pos);
  QPoint widget_pos = grid_to_widget(static_cast<float>(grid_pos.x()),
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

  if (!type.isEmpty()) {
    draw_element(painter, type, widget_pos);
  }

  painter.setOpacity(1.0);
}

void MapCanvas::draw_element(QPainter& painter,
                             const QString& type,
                             const QPoint& pos,
                             int player_id) {

  int size = icon_size;

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
  } else if (type == "dead_tree") {
    fill_color = QColor(111, 86, 67);
    symbol = "🌲";
  } else if (type == "boulder") {
    fill_color = QColor(110, 110, 110);
    symbol = "🪨";
  } else if (type == "barracks") {

    if (player_id == 0) {
      fill_color = QColor(180, 180, 180);
    } else if (player_id == 1) {
      fill_color = QColor(100, 150, 255);
    } else if (player_id == 2) {
      fill_color = QColor(255, 100, 100);
    } else {
      fill_color = QColor(100, 255, 100);
    }
    symbol = "🏛";
  } else if (type == "village") {
    if (player_id == 0) {
      fill_color = QColor(180, 180, 180);
    } else if (player_id == 1) {
      fill_color = QColor(100, 150, 255);
    } else if (player_id == 2) {
      fill_color = QColor(255, 100, 100);
    } else {
      fill_color = QColor(100, 255, 100);
    }
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
    QString player_text = player_id == 0 ? "N" : QString::number(player_id);
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

  if (event->button() == Qt::LeftButton && m_map_data) {
    QPointF raw_pos = map_to_grid(event->pos());
    const bool shift_held = event->modifiers() & Qt::ShiftModifier;
    QPointF grid_pos = shift_held ? raw_pos : snap_pos(raw_pos);

    switch (m_current_tool) {
    case ToolType::Select: {
      HitResult hit = hit_test(event->pos());
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
    case ToolType::PropDeadTree:
    case ToolType::PropBoulder:
    case ToolType::Barracks:
    case ToolType::Village:
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
      m_map_data && m_selected_type >= 0 && m_selected_index >= 0) {
    if (m_selected_type == 0) {
      const auto& terrain = m_map_data->terrain_elements();
      if (m_selected_index < terrain.size()) {
        m_map_data->record_command(std::make_unique<UpdateTerrainCmd>(
            m_map_data,
            m_selected_index,
            m_drag_pre_terrain,
            terrain[m_selected_index],
            "Move " + terrain[m_selected_index].type));
      }
    } else if (m_selected_type == 1) {
      const auto& world_props = m_map_data->world_props();
      if (m_selected_index < world_props.size()) {
        m_map_data->record_command(std::make_unique<UpdateWorldPropCmd>(
            m_map_data,
            m_selected_index,
            m_drag_pre_world_prop,
            world_props[m_selected_index],
            "Move " + world_props[m_selected_index].type));
      }
    } else if (m_selected_type == 2) {
      const auto& linear = m_map_data->linear_elements();
      if (m_selected_index < linear.size()) {
        m_map_data->record_command(
            std::make_unique<UpdateLinearCmd>(m_map_data,
                                              m_selected_index,
                                              m_drag_pre_linear,
                                              linear[m_selected_index],
                                              "Move " + linear[m_selected_index].type));
      }
    } else if (m_selected_type == 3) {
      const auto& structures = m_map_data->structures();
      if (m_selected_index < structures.size()) {
        m_map_data->record_command(std::make_unique<UpdateStructureCmd>(
            m_map_data,
            m_selected_index,
            m_drag_pre_structure,
            structures[m_selected_index],
            "Move " + structures[m_selected_index].type));
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
  QPoint delta = event->pos() - m_last_mouse_pos;
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

  if (m_is_dragging && m_map_data && m_selected_type >= 0 && m_selected_index >= 0) {
    m_did_drag_move = true;
    QPointF raw_pos = map_to_grid(event->pos());
    const bool shift_held = event->modifiers() & Qt::ShiftModifier;
    QPointF grid_pos = shift_held ? raw_pos : snap_pos(raw_pos);

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
        QVector2D new_pos(static_cast<float>(grid_pos.x()),
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
    }
  }

  if (!m_is_dragging && !m_is_panning && m_map_data &&
      (m_current_tool == ToolType::Select || m_current_tool == ToolType::Eraser)) {
    HitResult hover_hit = hit_test(event->pos());
    int new_hovered_type = hover_hit.element_type;
    int new_hovered_index = hover_hit.index;
    if (new_hovered_type != m_hovered_type || new_hovered_index != m_hovered_index) {
      m_hovered_type = new_hovered_type;
      m_hovered_index = new_hovered_index;
      update();
    }
  }

  QPointF cursor_grid = map_to_grid(event->pos());
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
    HitResult hit = hit_test(event->pos());
    if (hit.element_type >= 0 && hit.index >= 0) {
      emit element_double_clicked(hit.element_type, hit.index);
    } else {

      emit grid_double_clicked();
    }
  }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
  float old_zoom = m_zoom;

  if (event->angleDelta().y() > 0) {
    m_zoom *= 1.1F;
  } else {
    m_zoom /= 1.1F;
  }

  m_zoom = std::clamp(m_zoom, 0.1F, 5.0F);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QPointF cursor_pos = event->position();
#else
  QPointF cursor_pos = event->posF();
#endif
  m_pan_offset = cursor_pos - (cursor_pos - m_pan_offset) * (m_zoom / old_zoom);

  emit zoom_changed(m_zoom);
  update();
}

void MapCanvas::resizeEvent(QResizeEvent*) {

  if (m_pan_offset.isNull() && m_map_data) {
    m_pan_offset = QPointF(50, 50);
  }
}

MapCanvas::HitResult MapCanvas::hit_test(const QPoint& pos) const {
  HitResult result;
  if (!m_map_data) {
    return result;
  }

  QPointF grid_pos = map_to_grid(pos);

  const auto& structures = m_map_data->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto& elem = structures[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 3;
      result.index = i;
      return result;
    }
  }

  const auto& terrain = m_map_data->terrain_elements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto& elem = terrain[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 0;
      result.index = i;
      return result;
    }
  }

  const auto& world_props = m_map_data->world_props();
  for (int i = 0; i < world_props.size(); ++i) {
    const auto& elem = world_props[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 1;
      result.index = i;
      return result;
    }
  }

  const auto& linear = m_map_data->linear_elements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto& elem = linear[i];
    QVector2D p(static_cast<float>(grid_pos.x()), static_cast<float>(grid_pos.y()));

    float start_dist = (p - elem.start).length();
    if (start_dist <= endpoint_hit_radius) {
      result.element_type = 2;
      result.index = i;
      result.endpoint = 0;
      return result;
    }

    float end_dist = (p - elem.end).length();
    if (end_dist <= endpoint_hit_radius) {
      result.element_type = 2;
      result.index = i;
      result.endpoint = 1;
      return result;
    }
  }

  for (int i = 0; i < linear.size(); ++i) {
    const auto& elem = linear[i];
    QVector2D p(static_cast<float>(grid_pos.x()), static_cast<float>(grid_pos.y()));
    QVector2D a = elem.start;
    QVector2D b = elem.end;
    QVector2D ab = b - a;
    float ab_length_sq = QVector2D::dotProduct(ab, ab);

    float dist;
    if (ab_length_sq < 0.0001F) {
      dist = (p - a).length();
    } else {
      float t = std::clamp(QVector2D::dotProduct(p - a, ab) / ab_length_sq, 0.0F, 1.0F);
      QVector2D closest = a + t * ab;
      dist = (p - closest).length();
    }

    if (dist <= elem.width + 2.0F) {
      result.element_type = 2;
      result.index = i;
      result.endpoint = -1;
      return result;
    }
  }

  return result;
}

void MapCanvas::place_element(const QPointF& grid_pos) {
  if (!m_map_data) {
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
  if (!m_map_data) {
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
  if (!m_map_data) {
    return;
  }

  HitResult hit = hit_test(grid_to_widget(static_cast<float>(grid_pos.x()),
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
    if (m_map_data && m_selected_type >= 0 && m_selected_index >= 0) {
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
