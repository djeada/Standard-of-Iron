#pragma once

#include <QPoint>
#include <QSizeF>
#include <QWidget>

#include <array>
#include <functional>

#include "element_ops.h"
#include "map_data.h"
#include "mission_data.h"
#include "tool_panel.h"

namespace MapEditor {

class MapCanvas : public QWidget {
  Q_OBJECT

public:
  explicit MapCanvas(QWidget* parent = nullptr);

  enum Layer {
    LayerTerrain = 0,
    LayerWorldProp = 1,
    LayerLinear = 2,
    LayerStructure = 3,
    LayerTroopSpawn = 4,
    LayerUndeadZone = 5,
    LayerFogZone = 6,
    LayerMissionOverlay = 7,
    LayerCount = 8,
  };

  void set_map_data(MapData* data);
  void set_mission_data(MissionData* data);
  void set_current_tool(ToolType tool);
  void clear_tool();
  void clear_selection();
  void set_current_player_id(int id);
  void set_current_nation(const QString& nation);

  [[nodiscard]] int selected_element_type() const { return primary_selection().kind; }
  [[nodiscard]] int selected_element_index() const { return primary_selection().index; }
  [[nodiscard]] bool has_selection() const { return !m_selection.isEmpty(); }
  [[nodiscard]] int selection_count() const {
    return static_cast<int>(m_selection.size());
  }
  void select_all();

  [[nodiscard]] float zoom() const { return m_zoom; }
  void set_zoom(float zoom);
  void zoom_in();
  void zoom_out();
  void zoom_to_fit();
  void frame_selection();

  void delete_selection();
  void duplicate_selection();
  void copy_selection();
  void paste_from_clipboard(const QPointF& grid_pos);
  void paste_at_cursor();
  [[nodiscard]] bool has_clipboard() const { return !m_clipboard.isEmpty(); }
  void snap_selection_to_grid();
  void set_selection_player_id(int player_id);
  void nudge_selection(const QPointF& delta_cells);

  void bring_selection_to_front();
  void send_selection_to_back();
  void reset_draw_order();
  [[nodiscard]] bool has_draw_order_overrides() const {
    return !m_front_order.isEmpty() || !m_back_order.isEmpty();
  }

  void set_layer_visible(int layer, bool visible);
  [[nodiscard]] bool layer_visible(int layer) const;

  [[nodiscard]] static QString layer_label(int layer);

signals:
  void element_double_clicked(int element_type, int index);
  void grid_double_clicked();
  void tool_cleared();
  void status_hint_changed(const QString& hint);
  void zoom_changed(float zoom);
  void selection_changed(int element_type, int index);
  void cursor_moved(int grid_x, int grid_z);
  void layers_changed();
  void action_feedback(const QString& message);

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
  bool event(QEvent* event) override;

private:
  [[nodiscard]] QPointF map_to_grid(const QPoint& widget_pos) const;
  [[nodiscard]] QPoint grid_to_widget(float grid_x, float grid_z) const;
  [[nodiscard]] QPointF clamp_to_grid(const QPointF& grid_pos) const;

  void draw_grid(QPainter& painter);
  void draw_fog_zones(QPainter& painter);
  void draw_category(QPainter& painter, int kind);
  void draw_one_element(QPainter& painter, const ElementRef& ref);
  void draw_terrain_element(QPainter& painter, int index);
  void draw_world_prop_element(QPainter& painter, int index);
  void draw_structure_element(QPainter& painter, int index);
  void draw_troop_spawn_element(QPainter& painter, int index);
  void draw_linear_element(QPainter& painter, int index);
  void draw_undead_zone_element(QPainter& painter, int index);
  void draw_linear_preview(QPainter& painter);
  void draw_mission_overlays(QPainter& painter);
  void draw_current_placement(QPainter& painter);
  void draw_rubber_band(QPainter& painter);
  void finish_rubber_band();

  [[nodiscard]] QVector<int> category_draw_order(int kind) const;
  void draw_terrain_feature(QPainter& painter,
                            const TerrainElement& elem,
                            const QPoint& center);
  void draw_world_prop_icon(QPainter& painter,
                            const QString& type,
                            const QPoint& pos,
                            int size);
  void draw_element(QPainter& painter,
                    const QString& type,
                    const QPoint& pos,
                    int player_id = 0,
                    int marker_radius_px = -1);
  [[nodiscard]] QSizeF terrain_ellipse_px(const TerrainElement& elem) const;
  [[nodiscard]] int terrain_marker_radius_px(const TerrainElement& elem) const;
  [[nodiscard]] float terrain_hit_radius_px(const TerrainElement& elem) const;

  [[nodiscard]] int marker_radius_px() const;
  [[nodiscard]] bool labels_visible() const;

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

  void show_context_menu(const QPoint& pos);
  void set_selection(int element_type, int index);
  void set_selection(const QVector<ElementRef>& refs);
  void toggle_selection(const ElementRef& ref);
  [[nodiscard]] ElementRef primary_selection() const {
    return m_selection.isEmpty() ? ElementRef{} : m_selection.back();
  }
  [[nodiscard]] bool is_selected_element(int kind, int index) const;
  void notify_selection_changed();
  void drop_stale_view_state();
  void apply_zoom(float zoom, const QPointF& anchor_widget_pos);
  void center_on_grid_pos(const QPointF& grid_pos);
  [[nodiscard]] ElementSnapshot selected_snapshot() const;
  [[nodiscard]] QVector<ElementSnapshot> selected_snapshots() const;
  [[nodiscard]] QVector<ElementRef> elements_in_rect(const QRect& rect) const;
  void move_selection_to(const QPointF& primary_target);
  void select_appended(const QVector<ElementSnapshot>& added);

  using SelectionTransform =
      std::function<std::optional<ElementSnapshot>(const ElementSnapshot&)>;
  void apply_to_selection(const SelectionTransform& transform,
                          const QString& description);
  [[nodiscard]] QPointF clamp_group_delta(const QVector<ElementSnapshot>& snaps,
                                          const QPointF& delta) const;

  MapData* m_map_data = nullptr;
  MissionData* m_mission_data = nullptr;
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

  QVector<ElementRef> m_selection;
  int m_hovered_type = -1;
  int m_hovered_index = -1;
  bool m_is_dragging = false;
  int m_dragged_endpoint = -1;
  bool m_did_drag_move = false;
  QPointF m_linear_drag_center_offset;
  QVector<ElementRef> m_drag_refs;
  QVector<ElementSnapshot> m_drag_pre_elements;
  QVector<ElementSnapshot> m_clipboard;

  bool m_band_active = false;
  QPoint m_band_origin;
  QPoint m_band_current;

  QVector<ElementRef> m_front_order;
  QVector<ElementRef> m_back_order;
  std::array<int, k_element_kind_count> m_element_counts{};

  std::array<bool, LayerCount> m_layer_visible{};

  int m_current_player_id = 0;
  QString m_current_nation;

  static constexpr int grid_cell_size = 8;
  static constexpr int icon_size = 16;
  static constexpr int pan_drag_threshold = 4;
  static constexpr int min_player_id = 0;
  static constexpr int max_player_id = 4;
  static constexpr int default_max_population = 150;
  static constexpr int default_troop_max_population = -1;
  static constexpr float min_zoom = 0.05F;
  static constexpr float max_zoom = 5.0F;
};

} // namespace MapEditor
