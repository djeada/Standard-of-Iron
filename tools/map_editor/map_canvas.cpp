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

  // Set background
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(40, 50, 60));
  setPalette(pal);
}

void MapCanvas::setMapData(MapData *data) {
  m_mapData = data;
  if (m_mapData) {
    connect(m_mapData, &MapData::dataChanged, this,
            qOverload<>(&QWidget::update));
  }
  update();
}

void MapCanvas::setCurrentTool(ToolType tool) {
  m_currentTool = tool;
  m_isPlacingLinear = false;
  update();
}

void MapCanvas::clearTool() {
  m_currentTool = ToolType::Select;
  m_isPlacingLinear = false;
  emit toolCleared();
  update();
}

QPointF MapCanvas::mapToGrid(const QPoint &widgetPos) const {
  float x =
      (widgetPos.x() - m_panOffset.x()) / (GRID_CELL_SIZE * m_zoom);
  float z =
      (widgetPos.y() - m_panOffset.y()) / (GRID_CELL_SIZE * m_zoom);
  return QPointF(x, z);
}

QPoint MapCanvas::gridToWidget(float gridX, float gridZ) const {
  float x = gridX * GRID_CELL_SIZE * m_zoom + static_cast<float>(m_panOffset.x());
  float y = gridZ * GRID_CELL_SIZE * m_zoom + static_cast<float>(m_panOffset.y());
  return QPoint(static_cast<int>(x), static_cast<int>(y));
}

void MapCanvas::paintEvent(QPaintEvent * /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw background
  painter.fillRect(rect(), QColor(40, 50, 60));

  if (!m_mapData) {
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
  if (!m_mapData) {
    return;
  }

  const GridSettings &grid = m_mapData->grid();
  float cellSize = GRID_CELL_SIZE * m_zoom;

  // Only draw grid if zoom is sufficient
  if (cellSize < 2) {
    return;
  }

  painter.setPen(QPen(QColor(60, 70, 80), 1));

  float startX = static_cast<float>(m_panOffset.x());
  float startY = static_cast<float>(m_panOffset.y());
  float endX = startX + grid.width * cellSize;
  float endY = startY + grid.height * cellSize;

  // Draw vertical lines every 10 units
  for (int i = 0; i <= grid.width; i += 10) {
    float x = startX + i * cellSize;
    if (x >= 0 && x <= width()) {
      painter.drawLine(QPointF(x, std::max(0.0F, startY)),
                       QPointF(x, std::min(static_cast<float>(height()), endY)));
    }
  }

  // Draw horizontal lines every 10 units
  for (int i = 0; i <= grid.height; i += 10) {
    float y = startY + i * cellSize;
    if (y >= 0 && y <= height()) {
      painter.drawLine(QPointF(std::max(0.0F, startX), y),
                       QPointF(std::min(static_cast<float>(width()), endX), y));
    }
  }

  // Draw map boundary
  painter.setPen(QPen(QColor(100, 120, 140), 2));
  painter.drawRect(QRectF(startX, startY, grid.width * cellSize,
                          grid.height * cellSize));

  // Draw coordinate labels at corners for orientation
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);
  painter.setPen(QColor(180, 180, 180));

  // Origin label (0, 0)
  painter.drawText(QPointF(startX + 2, startY + 12), "0,0");

  // Top-right corner (width, 0)
  QString topRight = QString("%1,0").arg(grid.width);
  painter.drawText(QPointF(endX - 30, startY + 12), topRight);

  // Bottom-left corner (0, height)
  QString bottomLeft = QString("0,%1").arg(grid.height);
  painter.drawText(QPointF(startX + 2, endY - 4), bottomLeft);

  // Bottom-right corner (width, height)
  QString bottomRight = QString("%1,%2").arg(grid.width).arg(grid.height);
  painter.drawText(QPointF(endX - 40, endY - 4), bottomRight);
}

void MapCanvas::drawTerrainElements(QPainter &painter) {
  if (!m_mapData) {
    return;
  }

  const auto &terrain = m_mapData->terrainElements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto &elem = terrain[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool isSelected = (m_selectedType == 0 && m_selectedIndex == i);
    if (isSelected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, elem.type, pos);
  }
}

void MapCanvas::drawFirecamps(QPainter &painter) {
  if (!m_mapData) {
    return;
  }

  const auto &firecamps = m_mapData->firecamps();
  for (int i = 0; i < firecamps.size(); ++i) {
    const auto &elem = firecamps[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool isSelected = (m_selectedType == 1 && m_selectedIndex == i);
    if (isSelected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, "firecamp", pos);
  }
}

void MapCanvas::drawStructures(QPainter &painter) {
  if (!m_mapData) {
    return;
  }

  const auto &structures = m_mapData->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto &elem = structures[i];
    QPoint pos = gridToWidget(elem.x, elem.z);

    bool isSelected = (m_selectedType == 3 && m_selectedIndex == i);
    if (isSelected) {
      painter.setPen(QPen(Qt::yellow, 2));
    } else {
      painter.setPen(QPen(Qt::white, 1));
    }

    drawElement(painter, elem.type, pos, elem.playerId);
  }
}

void MapCanvas::drawLinearElements(QPainter &painter) {
  if (!m_mapData) {
    return;
  }

  const auto &linear = m_mapData->linearElements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto &elem = linear[i];
    QPoint startPos = gridToWidget(elem.start.x(), elem.start.y());
    QPoint endPos = gridToWidget(elem.end.x(), elem.end.y());

    bool isSelected = (m_selectedType == 2 && m_selectedIndex == i);

    QColor color;
    int lineWidth = static_cast<int>(elem.width * m_zoom);
    lineWidth = std::max(2, std::min(lineWidth, 20));

    if (elem.type == "river") {
      color = QColor(70, 130, 200);
    } else if (elem.type == "road") {
      color = QColor(139, 119, 101);
    } else if (elem.type == "bridge") {
      color = QColor(160, 140, 100);
    }

    if (isSelected) {
      painter.setPen(QPen(Qt::yellow, lineWidth + 2));
      painter.drawLine(startPos, endPos);
    }

    painter.setPen(QPen(color, lineWidth));
    painter.drawLine(startPos, endPos);

    // Draw endpoints
    int endpointSize = 6;
    painter.setBrush(color.lighter());
    painter.setPen(Qt::white);
    painter.drawEllipse(startPos, endpointSize, endpointSize);
    painter.drawEllipse(endPos, endpointSize, endpointSize);
  }

  // Draw in-progress linear element
  if (m_isPlacingLinear) {
    QPoint startPos =
        gridToWidget(static_cast<float>(m_linearStart.x()),
                     static_cast<float>(m_linearStart.y()));
    QPointF currentGrid = mapToGrid(m_lastMousePos);
    QPoint endPos = gridToWidget(static_cast<float>(currentGrid.x()),
                                 static_cast<float>(currentGrid.y()));

    QPen pen(Qt::white, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(startPos, endPos);
  }
}

void MapCanvas::drawCurrentPlacement(QPainter &painter) {
  if (m_currentTool == ToolType::Select || m_currentTool == ToolType::Eraser) {
    return;
  }

  // Draw ghost of element at cursor position
  QPointF gridPos = mapToGrid(m_lastMousePos);
  QPoint widgetPos = gridToWidget(static_cast<float>(gridPos.x()),
                                  static_cast<float>(gridPos.y()));

  painter.setOpacity(0.5);
  painter.setPen(QPen(Qt::white, 1, Qt::DashLine));

  QString type;
  switch (m_currentTool) {
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
    drawElement(painter, type, widgetPos);
  }

  painter.setOpacity(1.0);
}

void MapCanvas::drawElement(QPainter &painter, const QString &type,
                            const QPoint &pos, int playerId) {
  // Use fixed icon size for all elements
  int size = ICON_SIZE;

  QColor fillColor;
  QString symbol;

  if (type == "hill") {
    fillColor = QColor(139, 137, 112);
    symbol = "⛰";
  } else if (type == "mountain") {
    fillColor = QColor(105, 105, 105);
    symbol = "🏔";
  } else if (type == "firecamp") {
    fillColor = QColor(255, 140, 0);
    symbol = "🔥";
  } else if (type == "barracks") {
    // Color based on player
    if (playerId == 0) {
      fillColor = QColor(180, 180, 180); // Neutral - gray
    } else if (playerId == 1) {
      fillColor = QColor(100, 150, 255); // Player 1 - blue
    } else if (playerId == 2) {
      fillColor = QColor(255, 100, 100); // Player 2 - red
    } else {
      fillColor = QColor(100, 255, 100); // Other - green
    }
    symbol = "🏛";
  } else if (type == "village") {
    if (playerId == 0) {
      fillColor = QColor(180, 180, 180);
    } else if (playerId == 1) {
      fillColor = QColor(100, 150, 255);
    } else if (playerId == 2) {
      fillColor = QColor(255, 100, 100);
    } else {
      fillColor = QColor(100, 255, 100);
    }
    symbol = "🏘";
  } else {
    fillColor = QColor(128, 128, 128);
    symbol = "?";
  }

  painter.setBrush(fillColor);
  painter.drawEllipse(pos, size, size);

  // Draw symbol
  QFont font = painter.font();
  font.setPointSize(12);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(QRect(pos.x() - size, pos.y() - size, size * 2, size * 2),
                   Qt::AlignCenter, symbol);

  // Draw player indicator for structures
  if ((type == "barracks" || type == "village") && playerId >= 0) {
    QString playerText = playerId == 0 ? "N" : QString::number(playerId);
    font.setPointSize(8);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pos.x() + size - 6, pos.y() - size + 10, playerText);
  }
}

void MapCanvas::mousePressEvent(QMouseEvent *event) {
  m_lastMousePos = event->pos();

  // Right-click clears tool selection
  if (event->button() == Qt::RightButton) {
    clearTool();
    return;
  }

  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton &&
       event->modifiers() & Qt::ControlModifier)) {
    m_isPanning = true;
    setCursor(Qt::ClosedHandCursor);
    return;
  }

  if (event->button() == Qt::LeftButton && m_mapData) {
    QPointF gridPos = mapToGrid(event->pos());

    switch (m_currentTool) {
    case ToolType::Select: {
      HitResult hit = hitTest(event->pos());
      m_selectedType = hit.elementType;
      m_selectedIndex = hit.index;
      m_draggedEndpoint = hit.endpoint; // Track which endpoint if linear element
      if (hit.elementType >= 0) {
        m_isDragging = true;
      }
      update();
      break;
    }
    case ToolType::Hill:
    case ToolType::Mountain:
    case ToolType::Firecamp:
    case ToolType::Barracks:
    case ToolType::Village:
      placeElement(gridPos);
      break;
    case ToolType::River:
    case ToolType::Road:
    case ToolType::Bridge:
      if (!m_isPlacingLinear) {
        startLinearElement(gridPos);
      } else {
        finishLinearElement(gridPos);
      }
      break;
    case ToolType::Eraser:
      eraseAtPosition(gridPos);
      break;
    }
  }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton && m_isPanning)) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
  }

  m_isDragging = false;
  m_draggedEndpoint = -1;
}

void MapCanvas::mouseMoveEvent(QMouseEvent *event) {
  QPoint delta = event->pos() - m_lastMousePos;
  m_lastMousePos = event->pos();

  if (m_isPanning) {
    m_panOffset += QPointF(delta.x(), delta.y());
    update();
    return;
  }

  if (m_isDragging && m_mapData && m_selectedType >= 0 &&
      m_selectedIndex >= 0) {
    QPointF gridPos = mapToGrid(event->pos());

    if (m_selectedType == 0) {
      auto terrain = m_mapData->terrainElements();
      if (m_selectedIndex < terrain.size()) {
        TerrainElement elem = terrain[m_selectedIndex];
        elem.x = static_cast<float>(gridPos.x());
        elem.z = static_cast<float>(gridPos.y());
        m_mapData->updateTerrainElement(m_selectedIndex, elem);
      }
    } else if (m_selectedType == 1) {
      auto firecamps = m_mapData->firecamps();
      if (m_selectedIndex < firecamps.size()) {
        FirecampElement elem = firecamps[m_selectedIndex];
        elem.x = static_cast<float>(gridPos.x());
        elem.z = static_cast<float>(gridPos.y());
        m_mapData->updateFirecamp(m_selectedIndex, elem);
      }
    } else if (m_selectedType == 2) {
      // Linear element - drag endpoint or whole element
      auto linear = m_mapData->linearElements();
      if (m_selectedIndex < linear.size()) {
        LinearElement elem = linear[m_selectedIndex];
        QVector2D newPos(static_cast<float>(gridPos.x()),
                         static_cast<float>(gridPos.y()));

        if (m_draggedEndpoint == 0) {
          // Drag start endpoint
          elem.start = newPos;
        } else if (m_draggedEndpoint == 1) {
          // Drag end endpoint
          elem.end = newPos;
        }
        // Note: if m_draggedEndpoint == -1 (clicked on line), we don't move

        m_mapData->updateLinearElement(m_selectedIndex, elem);
      }
    } else if (m_selectedType == 3) {
      auto structures = m_mapData->structures();
      if (m_selectedIndex < structures.size()) {
        StructureElement elem = structures[m_selectedIndex];
        elem.x = static_cast<float>(gridPos.x());
        elem.z = static_cast<float>(gridPos.y());
        m_mapData->updateStructure(m_selectedIndex, elem);
      }
    }
  }

  // Update for ghost preview
  if (m_currentTool != ToolType::Select) {
    update();
  }
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    HitResult hit = hitTest(event->pos());
    if (hit.elementType >= 0 && hit.index >= 0) {
      emit elementDoubleClicked(hit.elementType, hit.index);
    } else {
      // Double-click on empty space to edit grid dimensions
      emit gridDoubleClicked();
    }
  }
}

void MapCanvas::wheelEvent(QWheelEvent *event) {
  float oldZoom = m_zoom;

  if (event->angleDelta().y() > 0) {
    m_zoom *= 1.1F;
  } else {
    m_zoom /= 1.1F;
  }

  m_zoom = std::clamp(m_zoom, 0.1F, 5.0F);

  // Zoom towards cursor position
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QPointF cursorPos = event->position();
#else
  QPointF cursorPos = event->posF();
#endif
  m_panOffset = cursorPos - (cursorPos - m_panOffset) * (m_zoom / oldZoom);

  update();
}

void MapCanvas::resizeEvent(QResizeEvent * /*event*/) {
  // Center the map on first resize if needed
  if (m_panOffset.isNull() && m_mapData) {
    m_panOffset = QPointF(50, 50);
  }
}

MapCanvas::HitResult MapCanvas::hitTest(const QPoint &pos) const {
  HitResult result;
  if (!m_mapData) {
    return result;
  }

  QPointF gridPos = mapToGrid(pos);

  // Check structures first (they appear on top)
  const auto &structures = m_mapData->structures();
  for (int i = 0; i < structures.size(); ++i) {
    const auto &elem = structures[i];
    float dx = static_cast<float>(gridPos.x()) - elem.x;
    float dz = static_cast<float>(gridPos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= HIT_RADIUS) {
      result.elementType = 3;
      result.index = i;
      return result;
    }
  }

  // Check terrain elements
  const auto &terrain = m_mapData->terrainElements();
  for (int i = 0; i < terrain.size(); ++i) {
    const auto &elem = terrain[i];
    float dx = static_cast<float>(gridPos.x()) - elem.x;
    float dz = static_cast<float>(gridPos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= HIT_RADIUS) {
      result.elementType = 0;
      result.index = i;
      return result;
    }
  }

  // Check firecamps
  const auto &firecamps = m_mapData->firecamps();
  for (int i = 0; i < firecamps.size(); ++i) {
    const auto &elem = firecamps[i];
    float dx = static_cast<float>(gridPos.x()) - elem.x;
    float dz = static_cast<float>(gridPos.y()) - elem.z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= HIT_RADIUS) {
      result.elementType = 1;
      result.index = i;
      return result;
    }
  }

  // Check linear elements - first check endpoints (for dragging), then line
  const auto &linear = m_mapData->linearElements();
  for (int i = 0; i < linear.size(); ++i) {
    const auto &elem = linear[i];
    QVector2D p(static_cast<float>(gridPos.x()),
                static_cast<float>(gridPos.y()));

    // Check start endpoint
    float startDist = (p - elem.start).length();
    if (startDist <= ENDPOINT_HIT_RADIUS) {
      result.elementType = 2;
      result.index = i;
      result.endpoint = 0; // Start endpoint
      return result;
    }

    // Check end endpoint
    float endDist = (p - elem.end).length();
    if (endDist <= ENDPOINT_HIT_RADIUS) {
      result.elementType = 2;
      result.index = i;
      result.endpoint = 1; // End endpoint
      return result;
    }
  }

  // Check linear elements - now check line body
  for (int i = 0; i < linear.size(); ++i) {
    const auto &elem = linear[i];

    // Point-to-line-segment distance
    QVector2D p(static_cast<float>(gridPos.x()),
                static_cast<float>(gridPos.y()));
    QVector2D a = elem.start;
    QVector2D b = elem.end;

    QVector2D ab = b - a;
    float abLengthSq = QVector2D::dotProduct(ab, ab);

    float dist;
    if (abLengthSq < 0.0001F) {
      // Start and end points are identical, treat as point
      dist = (p - a).length();
    } else {
      float t = std::clamp(QVector2D::dotProduct(p - a, ab) / abLengthSq, 0.0F,
                           1.0F);
      QVector2D closest = a + t * ab;
      dist = (p - closest).length();
    }

    if (dist <= elem.width + 2.0F) {
      result.elementType = 2;
      result.index = i;
      result.endpoint = -1; // Line body, not an endpoint
      return result;
    }
  }

  return result;
}

void MapCanvas::placeElement(const QPointF &gridPos) {
  if (!m_mapData) {
    return;
  }

  if (m_currentTool == ToolType::Hill || m_currentTool == ToolType::Mountain) {
    TerrainElement elem;
    elem.type = (m_currentTool == ToolType::Hill) ? "hill" : "mountain";
    elem.x = static_cast<float>(gridPos.x());
    elem.z = static_cast<float>(gridPos.y());
    elem.radius = 10.0F;
    elem.height = (m_currentTool == ToolType::Hill) ? 3.0F : 8.0F;
    m_mapData->addTerrainElement(elem);
  } else if (m_currentTool == ToolType::Firecamp) {
    FirecampElement elem;
    elem.x = static_cast<float>(gridPos.x());
    elem.z = static_cast<float>(gridPos.y());
    elem.intensity = 1.0F;
    elem.radius = 3.0F;
    m_mapData->addFirecamp(elem);
  } else if (m_currentTool == ToolType::Barracks ||
             m_currentTool == ToolType::Village) {
    // Ask for player assignment
    bool ok;
    int playerId = QInputDialog::getInt(
        this, "Assign Team",
        QString("Enter player ID (%1 = neutral, %2-%3 = players):")
            .arg(MIN_PLAYER_ID)
            .arg(MIN_PLAYER_ID + 1)
            .arg(MAX_PLAYER_ID),
        m_currentPlayerId, MIN_PLAYER_ID, MAX_PLAYER_ID, 1, &ok);

    if (ok) {
      m_currentPlayerId = playerId; // Remember for next placement

      StructureElement elem;
      elem.type =
          (m_currentTool == ToolType::Barracks) ? "barracks" : "village";
      elem.x = static_cast<float>(gridPos.x());
      elem.z = static_cast<float>(gridPos.y());
      elem.playerId = playerId;
      elem.maxPopulation = DEFAULT_MAX_POPULATION;
      elem.nation = DEFAULT_NATION;
      m_mapData->addStructure(elem);
    }
  }
}

void MapCanvas::startLinearElement(const QPointF &gridPos) {
  m_isPlacingLinear = true;
  m_linearStart = gridPos;
}

void MapCanvas::finishLinearElement(const QPointF &gridPos) {
  if (!m_mapData) {
    return;
  }

  LinearElement elem;
  elem.start = QVector2D(static_cast<float>(m_linearStart.x()),
                         static_cast<float>(m_linearStart.y()));
  elem.end = QVector2D(static_cast<float>(gridPos.x()),
                       static_cast<float>(gridPos.y()));

  switch (m_currentTool) {
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

  m_mapData->addLinearElement(elem);
  m_isPlacingLinear = false;
}

void MapCanvas::eraseAtPosition(const QPointF &gridPos) {
  if (!m_mapData) {
    return;
  }

  HitResult hit = hitTest(gridToWidget(static_cast<float>(gridPos.x()),
                                       static_cast<float>(gridPos.y())));

  if (hit.elementType == 0) {
    m_mapData->removeTerrainElement(hit.index);
  } else if (hit.elementType == 1) {
    m_mapData->removeFirecamp(hit.index);
  } else if (hit.elementType == 2) {
    m_mapData->removeLinearElement(hit.index);
  } else if (hit.elementType == 3) {
    m_mapData->removeStructure(hit.index);
  }
}

} // namespace MapEditor
