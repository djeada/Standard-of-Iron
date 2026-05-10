#pragma once

#include "map_data.h"
#include "tool_panel.h"
#include <QPoint>
#include <QWidget>

namespace MapEditor {

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
  [[nodiscard]] QPointF mapToGrid(const QPoint &widgetPos) const;
  [[nodiscard]] QPoint gridToWidget(float gridX, float gridZ) const;

  void drawGrid(QPainter &painter);
  void drawTerrainElements(QPainter &painter);
  void drawFirecamps(QPainter &painter);
  void drawStructures(QPainter &painter);
  void drawLinearElements(QPainter &painter);
  void drawCurrentPlacement(QPainter &painter);
  void drawElement(QPainter &painter, const QString &type, const QPoint &pos,
                   int player_id = 0);

  struct HitResult {
    int elementType = -1;
    int index = -1;
    int endpoint = -1;
  };
  [[nodiscard]] HitResult hitTest(const QPoint &pos) const;

  void placeElement(const QPointF &gridPos);
  void startLinearElement(const QPointF &gridPos);
  void finishLinearElement(const QPointF &gridPos);
  void eraseAtPosition(const QPointF &gridPos);

  MapData *m_map_data = nullptr;
  ToolType m_current_tool = ToolType::Select;

  float m_zoom = 1.0F;
  QPointF m_pan_offset{0, 0};

  bool m_is_panning = false;
  QPoint m_last_mouse_pos;
  bool m_is_placing_linear = false;
  QPointF m_linear_start;

  int m_selected_type = -1;
  int m_selected_index = -1;
  bool m_is_dragging = false;
  int m_dragged_endpoint = -1;

  int m_current_player_id = 0;

  static constexpr int GRID_CELL_SIZE = 8;
  static constexpr int ICON_SIZE = 16;
  static constexpr float HIT_RADIUS = 5.0F;
  static constexpr float ENDPOINT_HIT_RADIUS = 3.0F;
  static constexpr int MIN_PLAYER_ID = 0;
  static constexpr int MAX_PLAYER_ID = 4;
  static constexpr int DEFAULT_MAX_POPULATION = 150;
  static inline const QString DEFAULT_NATION = "roman_republic";
};

} // namespace MapEditor
