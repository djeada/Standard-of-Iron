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
  [[nodiscard]] QPointF mapToGrid(const QPoint &widget_pos) const;
  [[nodiscard]] QPoint gridToWidget(float grid_x, float grid_z) const;

  void drawGrid(QPainter &painter);
  void drawTerrainElements(QPainter &painter);
  void drawFirecamps(QPainter &painter);
  void drawStructures(QPainter &painter);
  void drawLinearElements(QPainter &painter);
  void drawCurrentPlacement(QPainter &painter);
  void drawElement(QPainter &painter, const QString &type, const QPoint &pos,
                   int player_id = 0);

  struct HitResult {
    int element_type = -1;
    int index = -1;
    int endpoint = -1;
  };
  [[nodiscard]] HitResult hitTest(const QPoint &pos) const;

  void placeElement(const QPointF &grid_pos);
  void startLinearElement(const QPointF &grid_pos);
  void finishLinearElement(const QPointF &grid_pos);
  void eraseAtPosition(const QPointF &grid_pos);

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

  static constexpr int grid_cell_size = 8;
  static constexpr int icon_size = 16;
  static constexpr float hit_radius = 5.0F;
  static constexpr float endpoint_hit_radius = 3.0F;
  static constexpr int min_player_id = 0;
  static constexpr int max_player_id = 4;
  static constexpr int default_max_population = 150;
  static inline const QString default_nation = "roman_republic";
};

} // namespace MapEditor
