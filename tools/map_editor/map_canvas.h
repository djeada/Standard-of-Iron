#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QPolygonF>
#include <QSet>
#include <QSizeF>
#include <QTimer>
#include <QWidget>

#include <array>
#include <functional>

#include "element_ops.h"
#include "game/map/terrain_footprint.h"
#include "map_data.h"
#include "mission_data.h"
#include "tool_type.h"
#include "world_backdrop.h"

namespace MapEditor {

class MapCanvas : public QWidget {
  Q_OBJECT

public:
  explicit MapCanvas(QWidget* parent = nullptr);

  enum Layer {
    LayerTerrain = static_cast<int>(ElementKind::Terrain),
    LayerWorldProp = static_cast<int>(ElementKind::WorldProp),
    LayerLinear = static_cast<int>(ElementKind::Linear),
    LayerStructure = static_cast<int>(ElementKind::Structure),
    LayerTroopSpawn = static_cast<int>(ElementKind::TroopSpawn),
    LayerUndeadZone = static_cast<int>(ElementKind::UndeadZone),
    LayerWildlifeArea = static_cast<int>(ElementKind::WildlifeArea),
    LayerForest = static_cast<int>(ElementKind::Forest),
    LayerFogZone = k_element_kind_count,
    LayerMissionOverlay,
    LayerCount,
  };

  void set_map_data(MapData* data);
  void set_read_only(bool read_only) { m_read_only = read_only; }
  [[nodiscard]] bool is_read_only() const { return m_read_only; }
  void center_on(const QPointF& grid_pos) { center_on_grid_pos(grid_pos); }
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

  void set_world_view(bool enabled);
  [[nodiscard]] bool world_view() const { return m_world_view; }
  void set_living_world(bool enabled);
  [[nodiscard]] bool living_world() const { return m_living_world; }
  [[nodiscard]] bool preview_pending() const;
  [[nodiscard]] bool edits_paused() const;
  [[nodiscard]] QString preview_error() const;
  [[nodiscard]] bool world_backdrop_ready() const { return painting_world(); }

signals:
  void element_double_clicked(int element_type, int index);
  void resize_requested();
  void tool_cleared();
  void status_hint_changed(const QString& hint);
  void zoom_changed(float zoom);
  void selection_changed(int element_type, int index);
  void cursor_moved(int grid_x, int grid_z);
  void layers_changed();
  void action_feedback(const QString& message, bool success = true);
  void preview_changed();

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
  void leaveEvent(QEvent* event) override;
  bool event(QEvent* event) override;

private:
  [[nodiscard]] QPointF map_to_grid(const QPoint& widget_pos) const;
  [[nodiscard]] QPoint grid_to_widget(float grid_x, float grid_z) const;
  [[nodiscard]] QPointF clamp_to_grid(const QPointF& grid_pos) const;

  void draw_grid(QPainter& painter);
  void draw_world(QPainter& painter);
  void draw_world_frame(QPainter& painter);
  [[nodiscard]] QRectF map_rect_px() const;
  [[nodiscard]] bool painting_world() const;
  void invalidate_world_backdrop(WorldPreviewChange change);
  void start_world_backdrop();
  void draw_fog_zones(QPainter& painter);
  void draw_category(QPainter& painter, int kind);
  void draw_one_element(QPainter& painter, const ElementRef& ref);
  void draw_terrain_element(QPainter& painter, int index, const TerrainElement& elem);
  void
  draw_world_prop_element(QPainter& painter, int index, const WorldPropElement& elem);
  void draw_gate_element(QPainter& painter,
                         const StructureElement& elem,
                         bool is_hovered,
                         const QColor& hover_ring_color);
  void
  draw_structure_element(QPainter& painter, int index, const StructureElement& elem);
  void
  draw_troop_spawn_element(QPainter& painter, int index, const TroopSpawnElement& elem);
  void draw_linear_element(QPainter& painter, int index, const LinearElement& elem);
  [[nodiscard]] float linear_width_px(const LinearElement& elem) const;
  [[nodiscard]] QPolygonF linear_polyline_px(const LinearElement& elem) const;
  [[nodiscard]] QRectF ford_marker_bounds(const LinearElement& elem) const;
  void draw_crossing_markers(QPainter& painter);
  void
  draw_undead_zone_element(QPainter& painter, int index, const UndeadZoneElement& elem);
  void draw_wildlife_area_element(QPainter& painter,
                                  int index,
                                  const WildlifeAreaElement& elem);
  void draw_forest_element(QPainter& painter, int index, const ForestElement& elem);
  void draw_linear_preview(QPainter& painter);
  void draw_mission_overlays(QPainter& painter);
  void draw_derived_commanders(QPainter& painter);
  void draw_current_placement(QPainter& painter);
  void draw_rubber_band(QPainter& painter);
  void draw_selection_overlay(QPainter& painter);
  void finish_rubber_band();

  [[nodiscard]] QVector<int> category_draw_order(int kind) const;
  [[nodiscard]] QVector<ElementSnapshot>
  without_duplicate_commanders(const QVector<ElementSnapshot>& snaps,
                               int* rejected_count) const;
  void draw_terrain_outline(QPainter& painter,
                            const TerrainElement& elem,
                            const QPoint& center,
                            const QPen& pen,
                            double margin_px);
  void draw_terrain_feature(QPainter& painter,
                            const TerrainElement& elem,
                            const QPoint& center);
  void draw_terrain_entrances(QPainter& painter, const TerrainElement& elem);
  void draw_world_prop_icon(QPainter& painter,
                            const QString& type,
                            const QPoint& pos,
                            int size);
  void draw_element(QPainter& painter,
                    const QString& type,
                    const QPoint& pos,
                    int player_id = 0,
                    int marker_radius_px = -1);
  [[nodiscard]] Game::Map::FootprintCells
  terrain_footprint(const TerrainElement& elem) const;
  [[nodiscard]] Game::Map::HillShapeGeometry
  terrain_shape(const TerrainElement& elem) const;
  [[nodiscard]] static QVector<QPoint> terrain_mask_cells(const TerrainElement& elem);
  [[nodiscard]] QSizeF terrain_ellipse_px(const TerrainElement& elem) const;
  [[nodiscard]] int terrain_marker_radius_px(const TerrainElement& elem) const;
  [[nodiscard]] float terrain_hit_radius_px(const TerrainElement& elem) const;

  [[nodiscard]] int marker_radius_px() const;
  [[nodiscard]] bool labels_visible() const;

  [[nodiscard]] ElementRef hit_test(const QPoint& pos) const;
  [[nodiscard]] QRectF move_handle_for(const ElementSnapshot& snap) const;
  [[nodiscard]] QRectF endpoint_handle(const LinearElement& line, int endpoint) const;
  enum class MoveHandle {
    None,
    Whole,
    Start,
    End
  };
  [[nodiscard]] MoveHandle move_handle_at(const QPoint& pos) const;
  [[nodiscard]] const ElementSnapshot* drag_preview(const ElementRef& ref) const;
  void begin_move(MoveHandle handle, const QPoint& pos);
  void update_move(const QPoint& pos, bool snap);
  void finish_move();
  void cancel_pointer_gesture();
  void update_canvas_cursor(const QPoint& pos);
  [[nodiscard]] bool is_forced_pan_gesture(const QMouseEvent* event) const;
  [[nodiscard]] bool edit_available() const { return !m_read_only && !edits_paused(); }
  [[nodiscard]] QRectF map_coordinate_bounds() const;

  void place_gate(const QPointF& grid_pos);
  void place_element(const QPointF& grid_pos);
  void place_at_pointer(const QPoint& pos, bool snap);
  void start_linear_element(const QPointF& grid_pos);
  void finish_linear_element(const QPointF& grid_pos);
  void erase_at_position(const QPoint& pos);

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
  void pan_by(const QPointF& delta);
  void center_on_grid_pos(const QPointF& grid_pos);
  void report_converted(int converted);
  [[nodiscard]] ElementSnapshot selected_snapshot() const;
  [[nodiscard]] QVector<ElementSnapshot> selected_snapshots() const;
  [[nodiscard]] QVector<ElementRef> elements_in_rect(const QRect& rect) const;
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
  bool m_read_only = false;

  float m_zoom = 1.0F;
  bool m_fit_view = true;
  QPointF m_pan_offset{0, 0};

  enum class PointerGesture {
    None,
    Click,
    ExtendSelection,
    Pan,
    Move
  };
  PointerGesture m_gesture = PointerGesture::None;
  Qt::MouseButton m_gesture_button = Qt::NoButton;
  QPoint m_press_pos;
  bool m_space_pan_active = false;
  bool m_pointer_inside = false;
  QPoint m_last_mouse_pos;
  QPointF m_placement_pos;
  bool m_is_placing_linear = false;
  QPointF m_linear_start;

  QVector<ElementRef> m_selection;
  QSet<ElementRef> m_selected_refs;
  ElementRef m_hovered;
  MoveHandle m_active_handle = MoveHandle::None;
  QPointF m_drag_press_grid;
  bool m_drag_dist_reached = false;
  bool m_drag_changes_terrain = false;
  QHash<ElementRef, qsizetype> m_drag_indices;
  QVector<ElementSnapshot> m_drag_before;
  QVector<ElementSnapshot> m_drag_after;
  QVector<ElementSnapshot> m_clipboard;

  bool m_band_active = false;
  QPoint m_band_origin;
  QPoint m_band_current;

  QVector<ElementRef> m_front_order;
  QVector<ElementRef> m_back_order;
  std::array<int, k_element_kind_count> m_element_counts{};

  std::array<bool, LayerCount> m_layer_visible{};

  bool m_world_view = true;
  bool m_living_world = true;
  WorldBackdrop m_backdrop;
  QTimer m_backdrop_debounce;
  QTimer m_ambient_timer;
  QElapsedTimer m_world_clock;
  quint64 m_backdrop_requested = 0;
  quint64 m_backdrop_completed = 0;
  quint64 m_backdrop_required = 0;
  bool m_backdrop_running = false;
  qint64 m_last_paint_ms = 0;

  int m_current_player_id = 0;
  QString m_current_nation;

  static constexpr int grid_cell_size = 8;
  static constexpr int icon_size = 16;
  static constexpr int min_player_id = 0;
  static constexpr int max_player_id = 4;
  static constexpr int default_max_population = 150;
  static constexpr int default_troop_max_population = -1;
  static constexpr float min_zoom = 0.05F;
  static constexpr float max_zoom = 5.0F;
};

} // namespace MapEditor
