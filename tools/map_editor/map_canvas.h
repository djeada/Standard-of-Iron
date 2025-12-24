#pragma once

#include "map_data.h"
#include "tool_panel.h"
#include <QPoint>
#include <QWidget>

namespace MapEditor {

/**
 * @brief Canvas widget for rendering and editing the map
 */
class MapCanvas : public QWidget {
  Q_OBJECT

public:
  explicit MapCanvas(QWidget *parent = nullptr);

  void setMapData(MapData *data);
  void setCurrentTool(ToolType tool);
  void clearTool();

signals:
  void elementDoubleClicked(int elementType, int index);
  void gridDoubleClicked();
  void toolCleared();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  // Coordinate conversion
  [[nodiscard]] QPointF mapToGrid(const QPoint &widgetPos) const;
  [[nodiscard]] QPoint gridToWidget(float gridX, float gridZ) const;

  // Drawing helpers
  void drawGrid(QPainter &painter);
  void drawAxes(QPainter &painter);
  void drawTerrainElements(QPainter &painter);
  void drawFirecamps(QPainter &painter);
  void drawStructures(QPainter &painter);
  void drawLinearElements(QPainter &painter);
  void drawCurrentPlacement(QPainter &painter);
  void drawElement(QPainter &painter, const QString &type, const QPoint &pos,
                   int playerId = 0);

  // Hit testing
  struct HitResult {
    int elementType = -1; // 0=terrain, 1=firecamp, 2=linear, 3=structure
    int index = -1;
  };
  [[nodiscard]] HitResult hitTest(const QPoint &pos) const;

  // Element placement
  void placeElement(const QPointF &gridPos);
  void startLinearElement(const QPointF &gridPos);
  void finishLinearElement(const QPointF &gridPos);
  void eraseAtPosition(const QPointF &gridPos);

  MapData *m_mapData = nullptr;
  ToolType m_currentTool = ToolType::Select;

  // View state
  float m_zoom = 1.0F;
  QPointF m_panOffset{0, 0};

  // Interaction state
  bool m_isPanning = false;
  QPoint m_lastMousePos;
  bool m_isPlacingLinear = false;
  QPointF m_linearStart;

  // Selection state
  int m_selectedType = -1;
  int m_selectedIndex = -1;
  bool m_isDragging = false;

  // Current structure placement player
  int m_currentPlayerId = 0;

  static constexpr int GRID_CELL_SIZE = 8;   // Pixels per grid unit at zoom 1.0
  static constexpr int ICON_SIZE = 16;       // Fixed icon size for all elements
};

} // namespace MapEditor
