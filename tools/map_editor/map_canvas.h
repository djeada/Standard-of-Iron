#pragma once

#include <QPoint>
#include <QWidget>

#include "map_data.h"
#include "tool_panel.h"

namespace MapEditor {

class MapCanvas : public QWidget {
  Q_OBJECT

public:
  explicit MapCanvas(QWidget* parent = nullptr);

  void set_map_data(MapData* data);
  void set_current_tool(ToolType tool);
  void clear_tool();
  void clear_selection();
  void set_current_player_id(int id);

  [[nodiscard]] int selected_element_type() const { return m_selected_type; }
  [[nodiscard]] int selected_element_index() const { return m_selected_index; }

signals:
  void element_double_clicked(int element_type, int index);
  void grid_double_clicked();
  void tool_cleared();
  void status_hint_changed(const QString& hint);
  void zoom_changed(float zoom);
  void selection_changed(int element_type, int index);
  void cursor_moved(int grid_x, int grid_z);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

private:
  [[nodiscard]] QPointF map_to_grid(const QPoint& widget_pos) const;
  [[nodiscard]] QPoint grid_to_widget(float grid_x, float grid_z) const;

  void draw_grid(QPainter& painter);
  void draw_terrain_elements(QPainter& painter);
  void draw_world_props(QPainter& painter);
  void draw_structures(QPainter& painter);
  void draw_troop_spawns(QPainter& painter);
  void draw_linear_elements(QPainter& painter);
  void draw_current_placement(QPainter& painter);
  void draw_element(QPainter& painter,
                    const QString& type,
                    const QPoint& pos,
                    int player_id = 0);

  struct HitResult {
    int element_type = -1;
    int index = -1;
    int endpoint = -1;
  };
  [[nodiscard]] HitResult hit_test(const QPoint& pos) const;

  void begin_panning(const QPoint& pos);
  void finish_panning(const QPoint& pos);
  void update_canvas_cursor(const QPoint& pos);
  [[nodiscard]] bool is_forced_pan_gesture(const QMouseEvent* event) const;

  void place_element(const QPointF& grid_pos);
  void start_linear_element(const QPointF& grid_pos);
  void finish_linear_element(const QPointF& grid_pos);
  void erase_at_position(const QPointF& grid_pos);

  MapData* m_map_data = nullptr;
  ToolType m_current_tool = ToolType::Select;

  float m_zoom = 1.0F;
  QPointF m_pan_offset{0, 0};

  bool m_is_panning = false;
  bool m_is_pan_drag_pending = false;
  bool m_space_pan_active = false;
  QPoint m_last_mouse_pos;
  QPoint m_pan_press_pos;
  bool m_is_placing_linear = false;
  QPointF m_linear_start;

  int m_selected_type = -1;
  int m_selected_index = -1;
  int m_hovered_type = -1;
  int m_hovered_index = -1;
  bool m_is_dragging = false;
  int m_dragged_endpoint = -1;
  bool m_did_drag_move = false;
  TerrainElement m_drag_pre_terrain;
  WorldPropElement m_drag_pre_world_prop;
  LinearElement m_drag_pre_linear;
  StructureElement m_drag_pre_structure;
  TroopSpawnElement m_drag_pre_troop;

  int m_current_player_id = 0;

  static constexpr int grid_cell_size = 8;
  static constexpr int icon_size = 16;
  static constexpr int pan_drag_threshold = 4;
  static constexpr int min_player_id = 0;
  static constexpr int max_player_id = 4;
  static constexpr int default_max_population = 150;
  static constexpr int default_troop_max_population = -1;
  static inline const QString default_nation = "roman_republic";
};

} // namespace MapEditor
