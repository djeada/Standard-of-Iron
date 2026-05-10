#include "map_canvas.h"
#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

namespace MapEditor {

MapCanvas::MapCanvas(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(400, 400);

  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(40, 50, 60));
  setPalette(pal);
}

void MapCanvas::setMapData(MapData *data) {
  m_map_data = data;
  if (m_map_data) {
    connect(m_map_data, &MapData::dataChanged, this,
            qOverload<>(&QWidget::update));
  }
  update();
}

void MapCanvas::setCurrentTool(ToolType tool) {
  m_current_tool = tool;
  m_is_placing_linear = false;
  update();
}

void MapCanvas::clearTool() {
  m_current_tool = ToolType::Select;
  m_is_placing_linear = false;
  emit toolCleared();
  update();
}

QPointF MapCanvas::mapToGrid(const QPoint &widget_pos) const {
  float x = (widget_pos.x() - m_pan_offset.x()) / (grid_cell_size * m_zoom);
  float z = (widget_pos.y() - m_pan_offset.y()) / (grid_cell_size * m_zoom);
  return QPointF(x, z);
}

QPoint MapCanvas::gridToWidget(float grid_x, float grid_z) const {
  float x =
      grid_x * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.x());
  float y =
      grid_z * grid_cell_size * m_zoom + static_cast<float>(m_pan_offset.y());
  return QPoint(static_cast<int>(x), static_cast<int>(y));
}

void MapCanvas::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.fillRect(rect(), QColor(40, 50, 60));

  if (!m_map_data) {
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, "No map loaded");
    return;
  }

  drawGrid(painter);
  drawLinearElements(painter);
  drawTerrainElements(painter);
  drawFirecamps(painter);
  drawStructures(painter);
  drawCurrentPlacement(painter);
}

void MapCanvas::drawGrid(QPainter &painter) {
  if (!m_map_data) {
    return;
  }

  const GridSettings &grid = m_map_data->grid();
  float cell_size = grid_cell_size * m_zoom;

  if (cell_size < 2) {
    return;
  }

  painter.setPen(QPen(QColor(60, 70, 80), 1));

  float start_x = static_cast<float>(m_pan_offset.x());
  float start_y = static_cast<float>(m_pan_offset.y());
  float end_x = start_x + grid.width * cell_size;
  float end_y = start_y + grid.height * cell_size;

  for (int i = 0; i <= grid.width; i += 10) {
    float x = start_x + i * cell_size;
    if (x >= 0 && x <= width()) {
      painter.drawLine(
          QPointF(x, std::max(0.0F, start_y)),
          QPointF(x, std::min(static_cast<float>(height()), end_y)));
    }
  }

  for (int i = 0; i <= grid.height; i += 10) {
    float y = start_y + i * cell_size;
    if (y >= 0 && y <= height()) {
      painter.drawLine(
          QPointF(std::max(0.0F, start_x), y),
          QPointF(std::min(static_cast<float>(width()), end_x), y));
    }
  }

  painter.setPen(QPen(QColor(100, 120, 140), 2));
  painter.drawRect(QRectF(start_x, start_y, grid.width * cell_size,
                          grid.height * cell_size));

  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);
  painter.setPen(QColor(180, 180, 180));

  painter.drawText(QPointF(start_x + 2, start_y + 12), "0,0");

  QString top_right = QString("%1,0").arg(grid.width);
  painter.drawText(QPointF(end_x - 30, start_y + 12), top_right);

  QString bottom_left = QString("0,%1").arg(grid.height);
  painter.drawText(QPointF(start_x + 2, end_y - 4), bottom_left);

  QString bottom_right = QString("%1,%2").arg(grid.width).arg(grid.height);
  painter.drawText(QPointF(end_x - 40, end_y - 4), bottom_right);
}

void MapCanvas::drawTerrainElements(QPainter &painter) {
  if (!m_map_data) {
    return;
  }

  const auto &terrain = m_map_data->terrainElements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto &elem = terrain[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 0 && m_selected_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, elem.type, pos);
  }
}

void MapCanvas::drawFirecamps(QPainter &painter) {
  if (!m_map_data) {
    return;
  }

  const auto &firecamps = m_map_data->firecamps();
  for (int i = 0; i < firecamps.size(); ++i) {
    const auto &elem = firecamps[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 1 && m_selected_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, "firecamp", pos);
  }
}

void MapCanvas::drawStructures(QPainter &painter) {
  if (!m_map_data) {
    return;
  }

  const auto &structures = m_map_data->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto &elem = structures[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool is_selected = (m_selected_type == 3 && m_selected_index == i);
    if (is_selected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, elem.type, pos, elem.player_id);
  }
}

void MapCanvas::drawLinearElements(QPainter &painter) {
  if (!m_map_data) {
    return;
  }

  const auto &linear = m_map_data->linearElements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto &elem = linear[i];
    QPoint start_pos = gridToWidget(elem.start.x(), elem.start.y());
    QPoint end_pos = gridToWidget(elem.end.x(), elem.end.y());

    bool is_selected = (m_selected_type == 2 && m_selected_index == i);

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
      painter.setPen(QPen(Qt::yellow, line_width + 2));
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
    QPoint start_pos = gridToWidget(static_cast<float>(m_linear_start.x()),
                                    static_cast<float>(m_linear_start.y()));
    QPointF current_grid = mapToGrid(m_last_mouse_pos);
    QPoint end_pos = gridToWidget(static_cast<float>(current_grid.x()),
                                  static_cast<float>(current_grid.y()));

    QPen pen(Qt::white, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(start_pos, end_pos);
  }
}

void MapCanvas::drawCurrentPlacement(QPainter &painter) {
  if (m_current_tool == ToolType::Select ||
      m_current_tool == ToolType::Eraser) {
    return;
  }

  QPointF grid_pos = mapToGrid(m_last_mouse_pos);
  QPoint widget_pos = gridToWidget(static_cast<float>(grid_pos.x()),
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
  case ToolType::Firecamp:
    type = "firecamp";
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
    drawElement(painter, type, widget_pos);
  }

  painter.setOpacity(1.0);
}

void MapCanvas::drawElement(QPainter &painter, const QString &type,
                            const QPoint &pos, int player_id) {

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
                   Qt::AlignCenter, symbol);

  if ((type == "barracks" || type == "village") && player_id >= 0) {
    QString player_text = player_id == 0 ? "N" : QString::number(player_id);
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pos.x() + size - 6, pos.y() - size + 10, player_text);
  }
}

void MapCanvas::mousePressEvent(QMouseEvent *event) {
  m_last_mouse_pos = event->pos();

  if (event->button() == Qt::RightButton) {
    clearTool();
    return;
  }

  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton &&
       event->modifiers() & Qt::ControlModifier)) {
    m_is_panning = true;
    setCursor(Qt::ClosedHandCursor);
    return;
  }

  if (event->button() == Qt::LeftButton && m_map_data) {
    QPointF grid_pos = mapToGrid(event->pos());

    switch (m_current_tool) {
    case ToolType::Select: {
      HitResult hit = hitTest(event->pos());
      m_selected_type = hit.element_type;
      m_selected_index = hit.index;
      m_dragged_endpoint = hit.endpoint;
      if (hit.element_type >= 0) {
        m_is_dragging = true;
      }
      update();
      break;
    }
    case ToolType::Hill:
    case ToolType::Mountain:
    case ToolType::Firecamp:
    case ToolType::Barracks:
    case ToolType::Village:
      placeElement(grid_pos);
      break;
    case ToolType::River:
    case ToolType::Road:
    case ToolType::Bridge:
      if (!m_is_placing_linear) {
        startLinearElement(grid_pos);
      } else {
        finishLinearElement(grid_pos);
      }
      break;
    case ToolType::Eraser:
      eraseAtPosition(grid_pos);
      break;
    }
  }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton && m_is_panning)) {
    m_is_panning = false;
    setCursor(Qt::ArrowCursor);
  }

  m_is_dragging = false;
  m_dragged_endpoint = -1;
}

void MapCanvas::mouseMoveEvent(QMouseEvent *event) {
  QPoint delta = event->pos() - m_last_mouse_pos;
  m_last_mouse_pos = event->pos();

  if (m_is_panning) {
    m_pan_offset += QPointF(delta.x(), delta.y());
    update();
    return;
  }

  if (m_is_dragging && m_map_data && m_selected_type >= 0 &&
      m_selected_index >= 0) {
    QPointF grid_pos = mapToGrid(event->pos());

    if (m_selected_type == 0) {
      auto terrain = m_map_data->terrainElements();
      if (m_selected_index < terrain.size()) {
        TerrainElement elem = terrain[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->updateTerrainElement(m_selected_index, elem);
      }
    } else if (m_selected_type == 1) {
      auto firecamps = m_map_data->firecamps();
      if (m_selected_index < firecamps.size()) {
        FirecampElement elem = firecamps[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->updateFirecamp(m_selected_index, elem);
      }
    } else if (m_selected_type == 2) {

      auto linear = m_map_data->linearElements();
      if (m_selected_index < linear.size() && m_dragged_endpoint >= 0) {
        LinearElement elem = linear[m_selected_index];
        QVector2D new_pos(static_cast<float>(grid_pos.x()),
                          static_cast<float>(grid_pos.y()));

        if (m_dragged_endpoint == 0) {
          elem.start = new_pos;
        } else if (m_dragged_endpoint == 1) {
          elem.end = new_pos;
        }

        m_map_data->updateLinearElement(m_selected_index, elem);
      }
    } else if (m_selected_type == 3) {
      auto structures = m_map_data->structures();
      if (m_selected_index < structures.size()) {
        StructureElement elem = structures[m_selected_index];
        elem.x = static_cast<float>(grid_pos.x());
        elem.z = static_cast<float>(grid_pos.y());
        m_map_data->updateStructure(m_selected_index, elem);
      }
    }
  }

  if (m_current_tool != ToolType::Select) {
    update();
  }
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    HitResult hit = hitTest(event->pos());
    if (hit.element_type >= 0 && hit.index >= 0) {
      emit elementDoubleClicked(hit.element_type, hit.index);
    } else {

      emit gridDoubleClicked();
    }
  }
}

void MapCanvas::wheelEvent(QWheelEvent *event) {
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

  update();
}

void MapCanvas::resizeEvent(QResizeEvent *) {

  if (m_pan_offset.isNull() && m_map_data) {
    m_pan_offset = QPointF(50, 50);
  }
}

MapCanvas::HitResult MapCanvas::hitTest(const QPoint &pos) const {
  HitResult result;
  if (!m_map_data) {
    return result;
  }

  QPointF grid_pos = mapToGrid(pos);

  const auto &structures = m_map_data->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto &elem = structures[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 3;
      result.index = i;
      return result;
    }
  }

  const auto &terrain = m_map_data->terrainElements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto &elem = terrain[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 0;
      result.index = i;
      return result;
    }
  }

  const auto &firecamps = m_map_data->firecamps();
  for (int i = 0; i < firecamps.size(); ++i) {
    const auto &elem = firecamps[i];
    float dx = static_cast<float>(grid_pos.x()) - elem.x;
    float dz = static_cast<float>(grid_pos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= hit_radius) {
      result.element_type = 1;
      result.index = i;
      return result;
    }
  }

  const auto &linear = m_map_data->linearElements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto &elem = linear[i];
    QVector2D p(static_cast<float>(grid_pos.x()),
                static_cast<float>(grid_pos.y()));

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
    const auto &elem = linear[i];
    QVector2D p(static_cast<float>(grid_pos.x()),
                static_cast<float>(grid_pos.y()));
    QVector2D a = elem.start;
    QVector2D b = elem.end;
    QVector2D ab = b - a;
    float ab_length_sq = QVector2D::dotProduct(ab, ab);

    float dist;
    if (ab_length_sq < 0.0001F) {
      dist = (p - a).length();
    } else {
      float t = std::clamp(QVector2D::dotProduct(p - a, ab) / ab_length_sq,
                           0.0F, 1.0F);
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

void MapCanvas::placeElement(const QPointF &grid_pos) {
  if (!m_map_data) {
    return;
  }

  if (m_current_tool == ToolType::Hill ||
      m_current_tool == ToolType::Mountain) {
    TerrainElement elem;
    elem.type = (m_current_tool == ToolType::Hill) ? "hill" : "mountain";
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.radius = 10.0F;
    elem.height = (m_current_tool == ToolType::Hill) ? 3.0F : 8.0F;
    m_map_data->addTerrainElement(elem);
  } else if (m_current_tool == ToolType::Firecamp) {
    FirecampElement elem;
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.intensity = 1.0F;
    elem.radius = 3.0F;
    m_map_data->addFirecamp(elem);
  } else if (m_current_tool == ToolType::Barracks ||
             m_current_tool == ToolType::Village) {

    bool ok;
    int player_id = QInputDialog::getInt(
        this, "Assign Team",
        QString("Enter player ID (%1 = neutral, %2-%3 = players):")
            .arg(min_player_id)
            .arg(min_player_id + 1)
            .arg(max_player_id),
        m_current_player_id, min_player_id, max_player_id, 1, &ok);

    if (ok) {
      m_current_player_id = player_id;

      StructureElement elem;
      elem.type =
          (m_current_tool == ToolType::Barracks) ? "barracks" : "village";
      elem.x = static_cast<float>(grid_pos.x());
      elem.z = static_cast<float>(grid_pos.y());
      elem.player_id = player_id;
      elem.max_population = default_max_population;
      elem.nation = default_nation;
      m_map_data->addStructure(elem);
    }
  }
}

void MapCanvas::startLinearElement(const QPointF &grid_pos) {
  m_is_placing_linear = true;
  m_linear_start = grid_pos;
}

void MapCanvas::finishLinearElement(const QPointF &grid_pos) {
  if (!m_map_data) {
    return;
  }

  LinearElement elem;
  elem.start = QVector2D(static_cast<float>(m_linear_start.x()),
                         static_cast<float>(m_linear_start.y()));
  elem.end = QVector2D(static_cast<float>(grid_pos.x()),
                       static_cast<float>(grid_pos.y()));

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

  m_map_data->addLinearElement(elem);
  m_is_placing_linear = false;
}

void MapCanvas::eraseAtPosition(const QPointF &grid_pos) {
  if (!m_map_data) {
    return;
  }

  HitResult hit = hitTest(gridToWidget(static_cast<float>(grid_pos.x()),
                                       static_cast<float>(grid_pos.y())));

  if (hit.element_type == 0) {
    m_map_data->removeTerrainElement(hit.index);
  } else if (hit.element_type == 1) {
    m_map_data->removeFirecamp(hit.index);
  } else if (hit.element_type == 2) {
    m_map_data->removeLinearElement(hit.index);
  } else if (hit.element_type == 3) {
    m_map_data->removeStructure(hit.index);
  }
}

} // namespace MapEditor
