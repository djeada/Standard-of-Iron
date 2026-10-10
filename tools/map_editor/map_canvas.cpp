#include "map_canvas.h"

#include <QApplication>
#include <QClipboard>
#include <QHelpEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QSet>
#include <QSizeF>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "canvas_input.h"
#include "canvas_transform.h"
#include "commander_preview.h"
#include "element_ops.h"
#include "map_canvas_internal.h"
#include "map_json_keys.h"
#include "tool_catalog.h"
#include "ui/theme.h"
#include "wall_geometry.h"

namespace MapEditor {

using CanvasDetail::default_terrain_element;
using CanvasDetail::default_wildlife_radius;

namespace {

QPointF snap_pos(const QPointF& gp) {
  return {std::round(gp.x()), std::round(gp.y())};
}

auto axis_aligned_endpoint(const QVector2D& anchor, QVector2D candidate) -> QVector2D {
  const QVector2D delta = candidate - anchor;
  if (std::abs(delta.x()) >= std::abs(delta.y())) {
    candidate.setY(anchor.y());
  } else {
    candidate.setX(anchor.x());
  }
  return candidate;
}

} // namespace

MapCanvas::MapCanvas(QWidget* parent)
    : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(400, 400);
  m_layer_visible.fill(true);

  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Theme::backgroundDeep());
  setPalette(pal);
}

void MapCanvas::set_map_data(MapData* data) {
  m_map_data = data;
  m_selection.clear();
  m_front_order.clear();
  m_back_order.clear();
  m_element_counts.fill(0);
  if (m_map_data != nullptr) {
    connect(m_map_data, &MapData::data_changed, this, qOverload<>(&QWidget::update));
    connect(
        m_map_data, &MapData::data_changed, this, &MapCanvas::drop_stale_view_state);
    drop_stale_view_state();
  }
  update();
}

void MapCanvas::drop_stale_view_state() {
  if (m_map_data == nullptr) {
    return;
  }

  std::array<int, k_element_kind_count> counts{};
  bool structure_changed = false;
  for (int kind = 0; kind < k_element_kind_count; ++kind) {
    counts[kind] = ElementOps::count(*m_map_data, kind);
    structure_changed = structure_changed || counts[kind] != m_element_counts[kind];
  }
  m_element_counts = counts;

  if (!structure_changed) {
    return;
  }

  m_front_order.clear();
  m_back_order.clear();

  const int before = static_cast<int>(m_selection.size());
  m_selection.removeIf([this](const ElementRef& ref) {
    return !ElementOps::index_valid(*m_map_data, ref.kind, ref.index);
  });
  if (static_cast<int>(m_selection.size()) != before) {
    notify_selection_changed();
  }
}

void MapCanvas::set_mission_data(MissionData* data) {
  if (m_mission_data != nullptr) {
    disconnect(m_mission_data, nullptr, this, nullptr);
  }
  m_mission_data = data;
  if (m_mission_data != nullptr) {
    connect(m_mission_data,
            &MissionData::data_changed,
            this,
            qOverload<>(&QWidget::update));
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
  set_selection(QVector<ElementRef>{});
}

void MapCanvas::set_selection(int element_type, int index) {
  if (element_type < 0 || index < 0) {
    set_selection(QVector<ElementRef>{});
    return;
  }
  set_selection(QVector<ElementRef>{ElementRef{element_type, index}});
}

void MapCanvas::set_selection(const QVector<ElementRef>& refs) {
  m_selection.clear();
  for (const ElementRef& ref : refs) {
    if (m_map_data != nullptr && layer_visible(ref.kind) &&
        ElementOps::index_valid(*m_map_data, ref.kind, ref.index) &&
        !m_selection.contains(ref)) {
      m_selection.append(ref);
    }
  }
  notify_selection_changed();
}

void MapCanvas::toggle_selection(const ElementRef& ref) {
  if (m_map_data == nullptr ||
      !ElementOps::index_valid(*m_map_data, ref.kind, ref.index)) {
    return;
  }

  const qsizetype at = m_selection.indexOf(ref);
  if (at >= 0) {
    m_selection.remove(at);
  } else {
    m_selection.append(ref);
  }
  notify_selection_changed();
}

void MapCanvas::notify_selection_changed() {
  const ElementRef primary = primary_selection();
  emit selection_changed(primary.kind, primary.index);
  update();
}

bool MapCanvas::is_selected_element(int kind, int index) const {
  return m_selection.contains(ElementRef{kind, index});
}

void MapCanvas::select_all() {
  if (m_map_data == nullptr) {
    return;
  }

  QVector<ElementRef> refs;
  for (int kind = 0; kind < k_element_kind_count; ++kind) {
    if (!layer_visible(kind)) {
      continue;
    }
    const int count = ElementOps::count(*m_map_data, kind);
    for (int index = 0; index < count; ++index) {
      refs.append(ElementRef{kind, index});
    }
  }

  set_selection(refs);
  emit action_feedback(
      QStringLiteral("Selected %1 element(s).").arg(m_selection.size()));
}

ElementSnapshot MapCanvas::selected_snapshot() const {
  if (m_map_data == nullptr) {
    return {};
  }
  return ElementOps::snapshot(*m_map_data, primary_selection());
}

QVector<ElementSnapshot> MapCanvas::selected_snapshots() const {
  if (m_map_data == nullptr) {
    return {};
  }
  return ElementOps::snapshots(*m_map_data, m_selection);
}

QString MapCanvas::layer_label(int layer) {
  switch (layer) {
  case LayerFogZone:
    return QStringLiteral("Fog zones");
  case LayerMissionOverlay:
    return QStringLiteral("Mission overlays");
  default:
    break;
  }
  return ElementOps::is_valid_kind(layer) ? ElementOps::category_label(layer)
                                          : QString{};
}

void MapCanvas::set_layer_visible(int layer, bool visible) {
  if (layer < 0 || layer >= LayerCount || m_layer_visible[layer] == visible) {
    return;
  }
  m_layer_visible[layer] = visible;

  if (!visible) {
    const qsizetype before = m_selection.size();
    m_selection.removeIf([layer](const ElementRef& ref) { return ref.kind == layer; });
    if (m_selection.size() != before) {
      notify_selection_changed();
    }
    if (m_hovered_type == layer) {
      m_hovered_type = -1;
      m_hovered_index = -1;
    }
  }

  emit layers_changed();
  update();
}

bool MapCanvas::layer_visible(int layer) const {
  return layer >= 0 && layer < LayerCount && m_layer_visible[layer];
}

int MapCanvas::marker_radius_px() const {
  const float scale = std::clamp(m_zoom, 0.35F, 2.25F);
  return std::max(4, static_cast<int>(std::lround(icon_size * scale)));
}

bool MapCanvas::labels_visible() const {
  return m_zoom >= 0.6F;
}

void MapCanvas::apply_zoom(float zoom, const QPointF& anchor_widget_pos) {
  const float old_zoom = m_zoom;
  const float new_zoom = std::clamp(zoom, min_zoom, max_zoom);
  if (std::abs(new_zoom - old_zoom) < 1e-6F) {
    return;
  }

  m_zoom = new_zoom;
  m_pan_offset =
      anchor_widget_pos - (anchor_widget_pos - m_pan_offset) * (m_zoom / old_zoom);
  emit zoom_changed(m_zoom);
  update();
}

void MapCanvas::set_zoom(float zoom) {
  apply_zoom(zoom, QPointF(width() * 0.5, height() * 0.5));
}

void MapCanvas::zoom_in() {
  set_zoom(m_zoom * 1.25F);
}

void MapCanvas::zoom_out() {
  set_zoom(m_zoom / 1.25F);
}

void MapCanvas::zoom_to_fit() {
  if (m_map_data == nullptr) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  if (grid.width <= 0 || grid.height <= 0) {
    return;
  }

  constexpr float k_fit_margin = 0.94F;
  const float fit_x = static_cast<float>(width()) * k_fit_margin /
                      (static_cast<float>(grid.width) * grid_cell_size);
  const float fit_y = static_cast<float>(height()) * k_fit_margin /
                      (static_cast<float>(grid.height) * grid_cell_size);

  m_zoom = std::clamp(std::min(fit_x, fit_y), min_zoom, max_zoom);

  const float span_x = static_cast<float>(grid.width) * grid_cell_size * m_zoom;
  const float span_y = static_cast<float>(grid.height) * grid_cell_size * m_zoom;
  m_pan_offset = QPointF((width() - span_x) * 0.5, (height() - span_y) * 0.5);

  emit zoom_changed(m_zoom);
  update();
}

void MapCanvas::center_on_grid_pos(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  const float scaled_cell = grid_cell_size * m_zoom;
  const float view_x =
      static_cast<float>(grid.width) - static_cast<float>(grid_pos.x());
  const float view_z =
      static_cast<float>(grid.height) - static_cast<float>(grid_pos.y());

  m_pan_offset = QPointF(width() * 0.5 - view_x * scaled_cell,
                         height() * 0.5 - view_z * scaled_cell);
  update();
}

void MapCanvas::frame_selection() {
  const std::optional<QPointF> pos = ElementOps::position(selected_snapshot());
  if (!pos.has_value()) {
    emit action_feedback(QStringLiteral("Select an element first to frame it."));
    return;
  }

  if (m_zoom < 1.0F) {
    m_zoom = 1.0F;
    emit zoom_changed(m_zoom);
  }
  center_on_grid_pos(*pos);
}

void MapCanvas::delete_selection() {
  if (m_map_data == nullptr || m_selection.isEmpty()) {
    return;
  }

  std::unique_ptr<Command> cmd = ElementOps::make_remove_many(*m_map_data, m_selection);
  if (!cmd) {
    return;
  }

  const qsizetype removed = m_selection.size();
  m_selection.clear();
  m_map_data->execute_command(std::move(cmd));
  notify_selection_changed();
  if (removed > 1) {
    emit action_feedback(QStringLiteral("Deleted %1 elements.").arg(removed));
  }
}

void MapCanvas::copy_selection() {
  const QVector<ElementSnapshot> snaps = selected_snapshots();
  if (snaps.isEmpty()) {
    return;
  }

  m_clipboard = snaps;
  emit action_feedback(
      snaps.size() == 1
          ? QStringLiteral("Copied %1.").arg(ElementOps::display_name(snaps.front()))
          : QStringLiteral("Copied %1 elements.").arg(snaps.size()));
}

void MapCanvas::paste_from_clipboard(const QPointF& grid_pos) {
  if (m_map_data == nullptr || !has_clipboard()) {
    return;
  }

  const std::optional<QPointF> anchor = ElementOps::group_anchor(m_clipboard);
  if (!anchor.has_value()) {
    return;
  }
  const QPointF delta = clamp_to_grid(grid_pos) - *anchor;

  QVector<ElementSnapshot> translated;
  translated.reserve(m_clipboard.size());
  for (const ElementSnapshot& snap : m_clipboard) {
    translated.append(ElementOps::translated(snap, delta));
  }

  int rejected_commanders = 0;
  const QVector<ElementSnapshot> pasted =
      without_duplicate_commanders(translated, &rejected_commanders);
  if (rejected_commanders > 0) {
    emit action_feedback(
        QStringLiteral("Skipped %1 commander spawn(s): one commander per owner.")
            .arg(rejected_commanders));
  }

  std::unique_ptr<Command> cmd = ElementOps::make_add_many(*m_map_data, pasted);
  if (!cmd) {
    return;
  }

  m_map_data->execute_command(std::move(cmd));
  select_appended(pasted);
  emit action_feedback(
      pasted.size() == 1
          ? QStringLiteral("Pasted %1.").arg(ElementOps::display_name(pasted.front()))
          : QStringLiteral("Pasted %1 elements.").arg(pasted.size()));
}

void MapCanvas::paste_at_cursor() {
  paste_from_clipboard(snap_pos(map_to_grid(m_last_mouse_pos)));
}

void MapCanvas::duplicate_selection() {
  if (m_map_data == nullptr) {
    return;
  }

  const QVector<ElementSnapshot> snaps = selected_snapshots();
  if (snaps.isEmpty()) {
    return;
  }

  QVector<ElementSnapshot> offset;
  offset.reserve(snaps.size());
  for (const ElementSnapshot& snap : snaps) {
    offset.append(ElementOps::translated(snap, QPointF(1.0, 1.0)));
  }

  int rejected_commanders = 0;
  const QVector<ElementSnapshot> copies =
      without_duplicate_commanders(offset, &rejected_commanders);
  if (rejected_commanders > 0) {
    emit action_feedback(
        QStringLiteral("Skipped %1 commander spawn(s): one commander per owner.")
            .arg(rejected_commanders));
  }

  std::unique_ptr<Command> cmd = ElementOps::make_add_many(*m_map_data, copies);
  if (!cmd) {
    return;
  }

  m_map_data->execute_command(std::move(cmd));
  select_appended(copies);
  emit action_feedback(
      copies.size() == 1
          ? QStringLiteral("Duplicated %1.")
                .arg(ElementOps::display_name(copies.front()))
          : QStringLiteral("Duplicated %1 elements.").arg(copies.size()));
}

void MapCanvas::select_appended(const QVector<ElementSnapshot>& added) {
  if (m_map_data == nullptr) {
    return;
  }

  std::array<int, k_element_kind_count> remaining{};
  for (const ElementSnapshot& snap : added) {
    const int kind = ElementOps::kind_of(snap);
    if (ElementOps::is_valid_kind(kind)) {
      ++remaining[kind];
    }
  }

  QVector<ElementRef> refs;
  for (int kind = 0; kind < k_element_kind_count; ++kind) {
    const int count = ElementOps::count(*m_map_data, kind);
    for (int index = count - remaining[kind]; index < count; ++index) {
      refs.append(ElementRef{kind, index});
    }
  }
  set_selection(refs);
}

void MapCanvas::apply_to_selection(const SelectionTransform& transform,
                                   const QString& description) {
  if (m_map_data == nullptr || m_selection.isEmpty()) {
    return;
  }

  const QVector<ElementSnapshot> current = selected_snapshots();
  QVector<ElementRef> refs;
  QVector<ElementSnapshot> before;
  QVector<ElementSnapshot> after;

  for (int i = 0; i < current.size(); ++i) {
    std::optional<ElementSnapshot> changed = transform(current[i]);
    if (!changed.has_value()) {
      continue;
    }
    refs.append(m_selection[i]);
    before.append(current[i]);
    after.append(*changed);
  }

  if (refs.isEmpty()) {
    return;
  }

  const int converted = ElementOps::count_generated(before);
  std::unique_ptr<Command> cmd =
      ElementOps::make_update_many(*m_map_data, refs, before, after, description);
  if (cmd) {
    m_map_data->execute_command(std::move(cmd));
    report_converted(converted);
  }
}

void MapCanvas::report_converted(int converted) {
  if (converted > 0) {
    emit action_feedback(
        QStringLiteral("Converted %1 generated element(s) to authored; a reroll "
                       "will keep them.")
            .arg(converted));
  }
}

void MapCanvas::snap_selection_to_grid() {
  apply_to_selection(
      [](const ElementSnapshot& snap) -> std::optional<ElementSnapshot> {
        const ElementSnapshot snapped = ElementOps::snapped_to_grid(snap);
        if (!ElementOps::has_moved(snap, snapped)) {
          return std::nullopt;
        }
        return snapped;
      },
      QStringLiteral("Snap to grid"));
}

void MapCanvas::set_selection_player_id(int player_id) {
  const bool owner_has_commander =
      m_map_data != nullptr &&
      m_map_data->commander_spawn_index_for_player(player_id) >= 0;

  QVector<QString> blocked_commanders;
  if (owner_has_commander) {
    for (const ElementSnapshot& snap : selected_snapshots()) {
      if (ElementOps::supports_player_id(snap) &&
          ElementOps::player_id(snap) != player_id &&
          MapData::is_commander_troop_type(ElementOps::type_name(snap))) {
        blocked_commanders.append(ElementOps::type_name(snap));
      }
    }
  }

  apply_to_selection(
      [player_id, owner_has_commander](
          const ElementSnapshot& snap) -> std::optional<ElementSnapshot> {
        if (!ElementOps::supports_player_id(snap) ||
            ElementOps::player_id(snap) == player_id) {
          return std::nullopt;
        }
        if (owner_has_commander &&
            MapData::is_commander_troop_type(ElementOps::type_name(snap))) {
          return std::nullopt;
        }
        return ElementOps::with_player_id(snap, player_id);
      },
      QStringLiteral("Set player %1").arg(player_id));

  if (owner_has_commander && !blocked_commanders.isEmpty()) {
    emit action_feedback(
        QStringLiteral("Player %1 already has a commander — %2 commander spawn(s) "
                       "kept their owner.")
            .arg(player_id)
            .arg(blocked_commanders.size()));
  }
}

QVector<ElementSnapshot>
MapCanvas::without_duplicate_commanders(const QVector<ElementSnapshot>& snaps,
                                        int* rejected_count) const {
  QVector<ElementSnapshot> accepted;
  accepted.reserve(snaps.size());
  QSet<int> owners_with_commander;

  for (const ElementSnapshot& snap : snaps) {
    if (!MapData::is_commander_troop_type(ElementOps::type_name(snap)) ||
        !ElementOps::supports_player_id(snap)) {
      accepted.append(snap);
      continue;
    }

    const int owner = ElementOps::player_id(snap);
    const bool taken = owners_with_commander.contains(owner) ||
                       (m_map_data != nullptr &&
                        m_map_data->commander_spawn_index_for_player(owner) >= 0);
    if (taken) {
      if (rejected_count != nullptr) {
        ++(*rejected_count);
      }
      continue;
    }
    owners_with_commander.insert(owner);
    accepted.append(snap);
  }

  return accepted;
}

void MapCanvas::nudge_selection(const QPointF& delta_cells) {
  if (m_map_data == nullptr || m_selection.isEmpty()) {
    return;
  }

  const QVector<ElementSnapshot> before = selected_snapshots();
  const QPointF delta = clamp_group_delta(before, delta_cells);
  if (delta.isNull()) {
    return;
  }

  apply_to_selection(
      [&delta](const ElementSnapshot& snap) -> std::optional<ElementSnapshot> {
        return ElementOps::translated(snap, delta);
      },
      m_selection.size() == 1
          ? "Move " + ElementOps::display_name(before.front())
          : QStringLiteral("Move %1 elements").arg(m_selection.size()));
}

QPointF MapCanvas::clamp_group_delta(const QVector<ElementSnapshot>& snaps,
                                     const QPointF& delta) const {
  if (m_map_data == nullptr) {
    return delta;
  }

  const GridSettings& grid = m_map_data->grid();
  double min_dx = std::numeric_limits<double>::lowest();
  double max_dx = std::numeric_limits<double>::max();
  double min_dy = std::numeric_limits<double>::lowest();
  double max_dy = std::numeric_limits<double>::max();

  for (const ElementSnapshot& snap : snaps) {
    const std::optional<QPointF> pos = ElementOps::position(snap);
    if (!pos.has_value()) {
      continue;
    }
    min_dx = std::max(min_dx, -pos->x());
    max_dx = std::min(max_dx, static_cast<double>(grid.width) - pos->x());
    min_dy = std::max(min_dy, -pos->y());
    max_dy = std::min(max_dy, static_cast<double>(grid.height) - pos->y());
  }

  if (min_dx > max_dx || min_dy > max_dy) {
    return {};
  }
  return {std::clamp(delta.x(), min_dx, max_dx), std::clamp(delta.y(), min_dy, max_dy)};
}

void MapCanvas::bring_selection_to_front() {
  if (m_selection.isEmpty()) {
    return;
  }
  for (const ElementRef& ref : m_selection) {
    m_back_order.removeAll(ref);
    m_front_order.removeAll(ref);
    m_front_order.append(ref);
  }
  emit action_feedback(QStringLiteral("Brought %1 element(s) to front (view only).")
                           .arg(m_selection.size()));
  update();
}

void MapCanvas::send_selection_to_back() {
  if (m_selection.isEmpty()) {
    return;
  }
  for (const ElementRef& ref : m_selection) {
    m_front_order.removeAll(ref);
    m_back_order.removeAll(ref);
    m_back_order.prepend(ref);
  }
  emit action_feedback(QStringLiteral("Sent %1 element(s) to back (view only).")
                           .arg(m_selection.size()));
  update();
}

void MapCanvas::reset_draw_order() {
  if (!has_draw_order_overrides()) {
    return;
  }
  m_front_order.clear();
  m_back_order.clear();
  emit action_feedback(QStringLiteral("Restored the default draw order."));
  update();
}

void MapCanvas::set_current_player_id(int id) {
  m_current_player_id = id;
}

void MapCanvas::set_current_nation(const QString& nation) {
  m_current_nation = nation;
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

QPointF MapCanvas::clamp_to_grid(const QPointF& grid_pos) const {
  if (m_map_data == nullptr) {
    return grid_pos;
  }

  const GridSettings& grid = m_map_data->grid();
  return {std::clamp(grid_pos.x(), 0.0, static_cast<double>(grid.width)),
          std::clamp(grid_pos.y(), 0.0, static_cast<double>(grid.height))};
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

void MapCanvas::mousePressEvent(QMouseEvent* event) {
  m_last_mouse_pos = event->pos();

  if (m_read_only) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
      begin_panning(event->pos());
    }
    return;
  }

  if (event->button() == Qt::RightButton) {
    switch (right_click_action(m_is_placing_linear, m_current_tool)) {
    case RightClickAction::CancelLinearDraw:
      m_is_placing_linear = false;
      emit status_hint_changed("");
      update();
      break;
    case RightClickAction::ClearTool:
      clear_tool();
      break;
    case RightClickAction::ShowContextMenu:
      show_context_menu(event->pos());
      break;
    }
    return;
  }

  if (is_forced_pan_gesture(event)) {
    begin_panning(event->pos());
    return;
  }

  if (event->button() == Qt::LeftButton && (m_map_data != nullptr)) {
    QPointF const raw_pos = map_to_grid(event->pos());
    const bool shift_held = (event->modifiers() & Qt::ShiftModifier) != 0U;
    QPointF const grid_pos = clamp_to_grid(shift_held ? raw_pos : snap_pos(raw_pos));

    switch (m_current_tool) {
    case ToolType::Select: {
      HitResult const hit = hit_test(event->pos());
      const ElementRef hit_ref{hit.element_type, hit.index};
      m_dragged_endpoint = hit.endpoint;

      if (!hit_ref.is_valid()) {
        if (shift_held) {

          m_band_active = true;
          m_band_origin = event->pos();
          m_band_current = event->pos();
        } else {
          m_drag_refs.clear();
          m_drag_pre_elements.clear();
          set_selection(QVector<ElementRef>{});
          m_is_pan_drag_pending = true;
          m_pan_press_pos = event->pos();
        }
        update_canvas_cursor(event->pos());
        update();
        break;
      }

      if (shift_held) {
        toggle_selection(hit_ref);
      } else if (!is_selected_element(hit_ref.kind, hit_ref.index)) {

        set_selection(hit_ref.kind, hit_ref.index);
      } else {

        m_selection.removeAll(hit_ref);
        m_selection.append(hit_ref);
        notify_selection_changed();
      }

      if (is_selected_element(hit_ref.kind, hit_ref.index)) {
        m_is_dragging = true;
        m_did_drag_move = false;
        m_linear_drag_center_offset = QPointF();
        m_drag_refs = m_selection;
        m_drag_pre_elements = selected_snapshots();
        if (const auto* linear =
                std::get_if<LinearElement>(&m_drag_pre_elements.back());
            linear != nullptr && hit.endpoint < 0) {
          const QPointF center((linear->start.x() + linear->end.x()) * 0.5F,
                               (linear->start.y() + linear->end.y()) * 0.5F);
          m_linear_drag_center_offset = grid_pos - center;
        }
      }
      update_canvas_cursor(event->pos());
      update();
      break;
    }
    case ToolType::Eraser:
      erase_at_position(grid_pos);
      break;
    default:
      if (!is_linear_tool(m_current_tool)) {
        place_element(grid_pos);
      } else if (!m_is_placing_linear) {
        start_linear_element(grid_pos);
      } else {
        finish_linear_element(grid_pos);
      }
      break;
    }
  }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton && m_is_panning)) {
    finish_panning(event->pos());
  }

  if (event->button() == Qt::LeftButton && m_band_active) {
    finish_rubber_band();
  }

  if (event->button() == Qt::LeftButton && m_is_dragging && m_did_drag_move &&
      (m_map_data != nullptr) && !m_drag_refs.isEmpty()) {

    QVector<ElementRef> refs;
    QVector<ElementSnapshot> before;
    QVector<ElementSnapshot> after;
    for (int i = 0; i < m_drag_refs.size(); ++i) {
      const ElementSnapshot current = ElementOps::snapshot(*m_map_data, m_drag_refs[i]);
      if (ElementOps::has_moved(m_drag_pre_elements[i], current)) {
        refs.append(m_drag_refs[i]);
        before.append(m_drag_pre_elements[i]);
        after.append(current);
      }
    }

    if (!refs.isEmpty()) {
      const QString description =
          refs.size() == 1 ? "Move " + ElementOps::display_name(after.front())
                           : QStringLiteral("Move %1 elements").arg(refs.size());
      const int converted = ElementOps::count_generated(before);
      if (std::unique_ptr<Command> cmd = ElementOps::make_update_many(
              *m_map_data, refs, before, after, description)) {
        if (converted > 0) {
          cmd->execute();
        }
        m_map_data->record_command(std::move(cmd));
        report_converted(converted);
      }
    }
  }

  if (event->button() == Qt::LeftButton) {
    m_is_pan_drag_pending = false;
  }

  m_drag_refs.clear();
  m_drag_pre_elements.clear();
  m_is_dragging = false;
  m_dragged_endpoint = -1;
  m_did_drag_move = false;
  m_linear_drag_center_offset = QPointF();
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

  if (m_band_active) {
    m_band_current = event->pos();
    update();
    return;
  }

  if (m_is_dragging && (m_map_data != nullptr) && !m_drag_refs.isEmpty()) {
    m_did_drag_move = true;
    QPointF const raw_pos = map_to_grid(event->pos());
    const bool shift_held = (event->modifiers() & Qt::ShiftModifier) != 0U;
    QPointF const grid_pos = clamp_to_grid(shift_held ? raw_pos : snap_pos(raw_pos));

    const ElementSnapshot& primary_pre = m_drag_pre_elements.back();
    const auto* dragged_linear = std::get_if<LinearElement>(&primary_pre);

    if (dragged_linear != nullptr && m_dragged_endpoint >= 0) {

      LinearElement elem = std::get<LinearElement>(
          ElementOps::snapshot(*m_map_data, m_drag_refs.back()));
      const QVector2D new_pos(static_cast<float>(grid_pos.x()),
                              static_cast<float>(grid_pos.y()));
      if (m_dragged_endpoint == 0) {
        elem.start = elem.type == QStringLiteral("wall")
                         ? axis_aligned_endpoint(elem.end, new_pos)
                         : new_pos;
      } else if (m_dragged_endpoint == 1) {
        elem.end = elem.type == QStringLiteral("wall")
                       ? axis_aligned_endpoint(elem.start, new_pos)
                       : new_pos;
      }
      m_map_data->update_linear_element(m_drag_refs.back().index, elem);
    } else {
      move_selection_to(dragged_linear != nullptr
                            ? grid_pos - m_linear_drag_center_offset
                            : grid_pos);
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

void MapCanvas::move_selection_to(const QPointF& primary_target) {
  if (m_map_data == nullptr || m_drag_refs.isEmpty()) {
    return;
  }

  const std::optional<QPointF> primary_origin =
      ElementOps::position(m_drag_pre_elements.back());
  if (!primary_origin.has_value()) {
    return;
  }

  const QPointF delta =
      clamp_group_delta(m_drag_pre_elements, primary_target - *primary_origin);
  for (int i = 0; i < m_drag_refs.size(); ++i) {
    ElementOps::apply(*m_map_data,
                      m_drag_refs[i].index,
                      ElementOps::translated(m_drag_pre_elements[i], delta));
  }
}

QVector<ElementRef> MapCanvas::elements_in_rect(const QRect& rect) const {
  QVector<ElementRef> refs;
  if (m_map_data == nullptr) {
    return refs;
  }

  for (int kind = 0; kind < k_element_kind_count; ++kind) {
    if (!layer_visible(kind)) {
      continue;
    }
    const int count = ElementOps::count(*m_map_data, kind);
    for (int index = 0; index < count; ++index) {
      const ElementSnapshot snap = ElementOps::snapshot(*m_map_data, kind, index);

      bool inside = false;
      if (const auto* linear = std::get_if<LinearElement>(&snap)) {

        inside = rect.contains(grid_to_widget(linear->start.x(), linear->start.y())) ||
                 rect.contains(grid_to_widget(linear->end.x(), linear->end.y()));
      }
      if (!inside) {
        const std::optional<QPointF> pos = ElementOps::position(snap);
        inside = pos.has_value() &&
                 rect.contains(grid_to_widget(static_cast<float>(pos->x()),
                                              static_cast<float>(pos->y())));
      }

      if (inside) {
        refs.append(ElementRef{kind, index});
      }
    }
  }

  return refs;
}

void MapCanvas::finish_rubber_band() {
  m_band_active = false;

  const QRect band = QRect(m_band_origin, m_band_current).normalized();
  if (band.width() < 3 && band.height() < 3) {
    update();
    return;
  }

  QVector<ElementRef> refs = m_selection;
  for (const ElementRef& ref : elements_in_rect(band)) {
    if (!refs.contains(ref)) {
      refs.append(ref);
    }
  }

  const qsizetype added = refs.size() - m_selection.size();
  set_selection(refs);
  emit action_feedback(
      added > 0 ? QStringLiteral("Added %1 element(s) to the selection.").arg(added)
                : QStringLiteral("No elements inside the selection box."));
}

void MapCanvas::show_context_menu(const QPoint& pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const HitResult hit = hit_test(pos);

  const QPointF grid_pos = clamp_to_grid(snap_pos(map_to_grid(pos)));
  const QPoint global_pos = mapToGlobal(pos);

  QMenu menu(this);

  if (hit.element_type >= 0 && hit.index >= 0) {

    if (!is_selected_element(hit.element_type, hit.index)) {
      set_selection(hit.element_type, hit.index);
    }
    const ElementSnapshot snap =
        ElementOps::snapshot(*m_map_data, hit.element_type, hit.index);
    const bool multi = m_selection.size() > 1;

    auto* header = menu.addAction(
        multi
            ? QStringLiteral("%1 elements selected").arg(m_selection.size())
            : QStringLiteral("%1: %2").arg(ElementOps::category_label(hit.element_type),
                                           ElementOps::display_name(snap)));
    header->setEnabled(false);
    menu.addSeparator();

    const int element_type = hit.element_type;
    const int index = hit.index;
    QAction* edit = menu.addAction(QStringLiteral("Edit JSON…"));
    edit->setEnabled(!multi);
    connect(edit, &QAction::triggered, this, [this, element_type, index]() {
      emit element_double_clicked(element_type, index);
    });

    QAction* duplicate = menu.addAction(QStringLiteral("Duplicate"));
    duplicate->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(duplicate, &QAction::triggered, this, &MapCanvas::duplicate_selection);

    QAction* copy = menu.addAction(QStringLiteral("Copy"));
    copy->setShortcut(QKeySequence::Copy);
    connect(copy, &QAction::triggered, this, &MapCanvas::copy_selection);

    QAction* remove = menu.addAction(QStringLiteral("Delete"));
    remove->setShortcut(QKeySequence::Delete);
    connect(remove, &QAction::triggered, this, &MapCanvas::delete_selection);

    menu.addSeparator();

    QAction* snap_action = menu.addAction(QStringLiteral("Snap to grid"));
    connect(snap_action, &QAction::triggered, this, &MapCanvas::snap_selection_to_grid);

    QAction* frame = menu.addAction(QStringLiteral("Frame selection\tF"));
    connect(frame, &QAction::triggered, this, &MapCanvas::frame_selection);

    menu.addSeparator();

    connect(menu.addAction(QStringLiteral("Bring to front (view only)")),
            &QAction::triggered,
            this,
            &MapCanvas::bring_selection_to_front);
    connect(menu.addAction(QStringLiteral("Send to back (view only)")),
            &QAction::triggered,
            this,
            &MapCanvas::send_selection_to_back);
    QAction* reset_order = menu.addAction(QStringLiteral("Reset draw order"));
    reset_order->setEnabled(has_draw_order_overrides());
    connect(reset_order, &QAction::triggered, this, &MapCanvas::reset_draw_order);

    menu.addSeparator();

    if (ElementOps::supports_player_id(snap)) {
      QMenu* player_menu = menu.addMenu(QStringLiteral("Set player"));
      const int current = ElementOps::player_id(snap);
      for (int id = min_player_id; id <= max_player_id; ++id) {
        QAction* action = player_menu->addAction(id == 0 ? QStringLiteral("0 (neutral)")
                                                         : QString::number(id));
        action->setCheckable(true);
        action->setChecked(id == current);
        connect(action, &QAction::triggered, this, [this, id]() {
          set_selection_player_id(id);
        });
      }
    }

    menu.addSeparator();
    QAction* copy_coords = menu.addAction(QStringLiteral("Copy coordinates"));
    copy_coords->setEnabled(!multi);
    connect(copy_coords, &QAction::triggered, this, [this, snap]() {
      const std::optional<QPointF> element_pos = ElementOps::position(snap);
      if (!element_pos.has_value()) {
        return;
      }
      const QString text = QStringLiteral("%1, %2")
                               .arg(element_pos->x(), 0, 'f', 2)
                               .arg(element_pos->y(), 0, 'f', 2);
      QApplication::clipboard()->setText(text);
      emit action_feedback(QStringLiteral("Copied coordinates %1.").arg(text));
    });
  } else {
    auto* header = menu.addAction(QStringLiteral("Canvas (%1, %2)")
                                      .arg(grid_pos.x(), 0, 'f', 0)
                                      .arg(grid_pos.y(), 0, 'f', 0));
    header->setEnabled(false);
    menu.addSeparator();

    QAction* paste = menu.addAction(QStringLiteral("Paste here"));
    paste->setShortcut(QKeySequence::Paste);
    paste->setEnabled(has_clipboard());
    connect(paste, &QAction::triggered, this, [this, grid_pos]() {
      paste_from_clipboard(grid_pos);
    });

    if (m_current_tool != ToolType::Select && m_current_tool != ToolType::Eraser) {
      connect(menu.addAction(QStringLiteral("Place here")),
              &QAction::triggered,
              this,
              [this, grid_pos]() {
                place_element(grid_pos);
                update();
              });
    }

    menu.addSeparator();
    QAction* select_all_action = menu.addAction(QStringLiteral("Select all"));
    select_all_action->setShortcut(QKeySequence::SelectAll);
    connect(select_all_action, &QAction::triggered, this, &MapCanvas::select_all);

    QAction* reset_order = menu.addAction(QStringLiteral("Reset draw order"));
    reset_order->setEnabled(has_draw_order_overrides());
    connect(reset_order, &QAction::triggered, this, &MapCanvas::reset_draw_order);

    menu.addSeparator();
    connect(menu.addAction(QStringLiteral("Zoom to fit")),
            &QAction::triggered,
            this,
            &MapCanvas::zoom_to_fit);
    connect(menu.addAction(QStringLiteral("Resize map…")),
            &QAction::triggered,
            this,
            &MapCanvas::grid_double_clicked);
  }

  menu.exec(global_pos);
}

bool MapCanvas::event(QEvent* event) {
  if (event->type() == QEvent::ToolTip) {
    auto* help_event = static_cast<QHelpEvent*>(event);
    const HitResult hit = hit_test(help_event->pos());
    const ElementSnapshot snap =
        m_map_data != nullptr
            ? ElementOps::snapshot(*m_map_data, hit.element_type, hit.index)
            : ElementSnapshot{};
    const QString text = ElementOps::summary(snap);
    if (text.isEmpty()) {
      QToolTip::hideText();
      event->ignore();
    } else {
      QToolTip::showText(help_event->globalPos(), text, this);
    }
    return true;
  }

  return QWidget::event(event);
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  if (!m_read_only && event->button() == Qt::LeftButton) {
    HitResult const hit = hit_test(event->pos());
    if (hit.element_type >= 0 && hit.index >= 0) {
      emit element_double_clicked(hit.element_type, hit.index);
    } else {

      emit grid_double_clicked();
    }
  }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QPointF const cursor_pos = event->position();
#else
  QPointF cursor_pos = event->posF();
#endif

  apply_zoom(event->angleDelta().y() > 0 ? m_zoom * 1.1F : m_zoom / 1.1F, cursor_pos);
}

void MapCanvas::resizeEvent(QResizeEvent*) {

  if (m_pan_offset.isNull() && (m_map_data != nullptr)) {
    zoom_to_fit();
  }
}

MapCanvas::HitResult MapCanvas::hit_test(const QPoint& pos) const {
  HitResult result;
  if (m_map_data == nullptr) {
    return result;
  }

  const QVector2D cursor(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
  const float point_hit_radius_px = static_cast<float>(marker_radius_px()) + 4.0F;
  float best_dist = std::numeric_limits<float>::infinity();
  int best_priority = std::numeric_limits<int>::max();

  auto consider_hit = [&](int element_type,
                          int index,
                          int endpoint,
                          float distance,
                          float max_distance,
                          int priority) {
    if (distance > max_distance || !layer_visible(element_type)) {
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

  const auto& undead_zones = m_map_data->undead_zones();
  for (int i = undead_zones.size() - 1; i >= 0; --i) {
    const auto& elem = undead_zones[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    const float zone_radius_px = elem.radius * m_zoom * grid_cell_size;
    consider_hit(5, i, -1, (cursor - center_vec).length(), zone_radius_px + 4.0F, 6);
  }

  const auto& forests = m_map_data->forests();
  for (int i = forests.size() - 1; i >= 0; --i) {
    const auto& elem = forests[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    const float forest_radius_px = elem.radius * m_zoom * grid_cell_size;
    consider_hit(static_cast<int>(ElementKind::Forest),
                 i,
                 -1,
                 (cursor - center_vec).length(),
                 forest_radius_px + 4.0F,
                 8);
  }

  const auto& wildlife_areas = m_map_data->wildlife_areas();
  for (int i = wildlife_areas.size() - 1; i >= 0; --i) {
    const auto& elem = wildlife_areas[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));

    consider_hit(static_cast<int>(ElementKind::WildlifeArea),
                 i,
                 -1,
                 (cursor - center_vec).length(),
                 point_hit_radius_px,
                 7);
  }

  const auto& structures = m_map_data->structures();
  for (int i = structures.size() - 1; i >= 0; --i) {
    const auto& elem = structures[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));

    const float reach = elem.type == QStringLiteral("wall_gate")
                            ? std::max(point_hit_radius_px,
                                       WallGeometry::k_gate_span * 0.5F *
                                           static_cast<float>(grid_cell_size) * m_zoom)
                            : point_hit_radius_px;
    consider_hit(3, i, -1, (cursor - center_vec).length(), reach, 1);
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
    consider_hit(
        0, i, -1, (cursor - center_vec).length(), terrain_hit_radius_px(elem), 3);
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
    const QPolygonF path = linear_polyline_px(elem);
    if (path.size() < 2) {
      continue;
    }

    float dist = std::numeric_limits<float>::infinity();
    for (int point = 1; point < path.size(); ++point) {
      const QVector2D a(path[point - 1]);
      const QVector2D b(path[point]);
      const QVector2D ab = b - a;
      const float ab_length_sq = QVector2D::dotProduct(ab, ab);
      if (ab_length_sq < 0.0001F) {
        dist = std::min(dist, (cursor - a).length());
        continue;
      }
      const float t =
          std::clamp(QVector2D::dotProduct(cursor - a, ab) / ab_length_sq, 0.0F, 1.0F);
      dist = std::min(dist, (cursor - (a + t * ab)).length());
    }

    const float line_hit_radius_px = (linear_width_px(elem) * 0.5F) + 4.0F;
    if ((cursor - QVector2D(path.first())).length() <= point_hit_radius_px ||
        (cursor - QVector2D(path.last())).length() <= point_hit_radius_px) {
      continue;
    }
    consider_hit(2, i, -1, dist, line_hit_radius_px, 5);
  }

  return result;
}

void MapCanvas::place_gate(const QPointF& grid_pos) {
  namespace WG = WallGeometry;
  const WG::GatePlacement plan = WG::plan_gate(m_map_data->linear_elements(), grid_pos);

  StructureElement gate;
  gate.type = QStringLiteral("wall_gate");
  gate.player_id = m_current_player_id;
  gate.nation = m_current_nation;
  gate.max_population = 100;

  gate.x = plan.x;
  gate.z = plan.z;
  gate.rotation = plan.rotation;

  if (plan.wall_index < 0) {
    m_map_data->execute_command(std::make_unique<AddStructureCmd>(m_map_data, gate));
    emit status_hint_changed(
        "Gate placed clear of any wall - draw a wall through it to seal the ring");
    return;
  }

  const LinearElement run = m_map_data->linear_elements()[plan.wall_index];
  gate.player_id = run.player_id > 0 ? run.player_id : m_current_player_id;
  gate.nation = run.nation.isEmpty() ? m_current_nation : run.nation;

  std::vector<std::unique_ptr<Command>> steps;
  steps.push_back(std::make_unique<RemoveLinearCmd>(m_map_data, plan.wall_index, run));
  for (const bool keep_low : {true, false}) {
    if (auto piece =
            WG::trim_run_to_gate(run, plan.horizontal, plan.centre, keep_low)) {
      steps.push_back(std::make_unique<AddLinearCmd>(m_map_data, *piece));
    }
  }
  steps.push_back(std::make_unique<AddStructureCmd>(m_map_data, gate));
  m_map_data->execute_command(
      std::make_unique<CompositeCmd>(std::move(steps), QStringLiteral("Set gate")));
  emit status_hint_changed("");
}

void MapCanvas::place_element(const QPointF& raw_grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const QPointF grid_pos = clamp_to_grid(raw_grid_pos);

  const auto at = [&grid_pos](auto& elem) {
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
  };
  const QString element_type = element_type_for_tool(m_current_tool);

  switch (tool_placement(m_current_tool)) {
  case ToolPlacement::Terrain: {
    TerrainElement elem = default_terrain_element(m_current_tool);
    at(elem);
    m_map_data->execute_command(std::make_unique<AddTerrainCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::WorldProp: {
    WorldPropElement elem;
    elem.type = element_type;
    at(elem);
    if (elem.type == QStringLiteral("firecamp")) {
      elem.intensity = 1.0F;
      elem.radius = 3.0F;
    } else {
      elem.scale = 1.0F;
      elem.rotation = 0.0F;
    }
    m_map_data->execute_command(std::make_unique<AddWorldPropCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::Structure: {
    StructureElement elem;
    elem.type = element_type;
    at(elem);
    elem.player_id = m_current_player_id;
    elem.max_population = default_max_population;
    elem.nation = m_current_nation;
    m_map_data->execute_command(std::make_unique<AddStructureCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::Gate:
    place_gate(grid_pos);
    break;
  case ToolPlacement::Troop: {
    TroopSpawnElement elem;
    elem.type = element_type;
    at(elem);
    elem.player_id = m_current_player_id;
    elem.max_population = default_troop_max_population;
    elem.nation = m_current_nation;
    if (MapData::is_commander_troop_type(elem.type) &&
        m_map_data->commander_spawn_index_for_player(elem.player_id) >= 0) {
      emit action_feedback(
          QStringLiteral("Player %1 already has a commander — only one commander "
                         "per owner is spawned.")
              .arg(elem.player_id));
      return;
    }
    m_map_data->execute_command(std::make_unique<AddTroopSpawnCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::Forest: {
    ForestElement elem;
    at(elem);
    m_map_data->execute_command(std::make_unique<AddForestCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::Wildlife: {
    WildlifeAreaElement elem;
    elem.species = element_type;
    at(elem);
    elem.radius = default_wildlife_radius(elem.species);
    m_map_data->execute_command(std::make_unique<AddWildlifeAreaCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::UndeadZone: {
    UndeadZoneElement elem;
    elem.anchor_type = QStringLiteral("magic_shrine");
    at(elem);
    elem.radius = 8.0F;
    elem.leash_radius = 14.0F;
    elem.owner_id = 99;
    elem.team_id = 99;
    elem.awaken_on = QJsonArray{QStringLiteral("unit_enters_radius")};

    QJsonObject units_obj;
    units_obj[QStringLiteral("skeleton_swordsman")] = 2;
    units_obj[QStringLiteral("grave_priest")] = 1;
    QJsonObject wave;
    wave[QStringLiteral("trigger")] = QStringLiteral("initial");
    wave[QStringLiteral("units")] = units_obj;
    elem.waves = QJsonArray{wave};
    m_map_data->execute_command(std::make_unique<AddUndeadZoneCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::None:
  case ToolPlacement::Erase:
  case ToolPlacement::Linear:
    break;
  }
}

void MapCanvas::start_linear_element(const QPointF& grid_pos) {
  m_is_placing_linear = true;
  m_linear_start = clamp_to_grid(grid_pos);

  emit status_hint_changed("Drawing " + element_type_for_tool(m_current_tool) +
                           " \u2014 click to place end point"
                           " (right-click to cancel)");
}

void MapCanvas::finish_linear_element(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const QPointF end_pos = clamp_to_grid(grid_pos);

  LinearElement elem;
  elem.start = QVector2D(static_cast<float>(m_linear_start.x()),
                         static_cast<float>(m_linear_start.y()));
  elem.end =
      QVector2D(static_cast<float>(end_pos.x()), static_cast<float>(end_pos.y()));

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
    elem.width = std::max(
        k_min_bridge_width,
        compute_min_bridge_width(elem.start, elem.end, m_map_data->linear_elements()));
    elem.height = 0.5F;
    break;
  case ToolType::Ford:
    elem.type = "ford";
    elem.width = k_default_ford_width;
    break;
  case ToolType::Wall:
    elem.type = "wall";
    elem.width = WallGeometry::k_lattice;
    elem.player_id = m_current_player_id;
    elem.nation = m_current_nation;
    elem.start = QVector2D(WallGeometry::snap(elem.start.x()),
                           WallGeometry::snap(elem.start.y()));
    elem.end = axis_aligned_endpoint(
        elem.start,
        QVector2D(WallGeometry::snap(elem.end.x()), WallGeometry::snap(elem.end.y())));
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

  std::unique_ptr<Command> cmd =
      ElementOps::make_remove(*m_map_data, hit.element_type, hit.index);
  if (cmd) {
    m_selection.removeAll(ElementRef{hit.element_type, hit.index});
    m_map_data->execute_command(std::move(cmd));
  }
}

void MapCanvas::keyPressEvent(QKeyEvent* event) {
  if (m_read_only) {
    QWidget::keyPressEvent(event);
    return;
  }
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
    delete_selection();
    break;
  case Qt::Key_Left:
  case Qt::Key_Right:
  case Qt::Key_Up:
  case Qt::Key_Down: {
    if (!has_selection()) {
      QWidget::keyPressEvent(event);
      break;
    }

    const bool fine = (event->modifiers() & Qt::ShiftModifier) != 0U;
    const double step = fine ? 0.25 : 1.0;
    QPointF delta;
    if (event->key() == Qt::Key_Left) {
      delta.setX(step);
    } else if (event->key() == Qt::Key_Right) {
      delta.setX(-step);
    } else if (event->key() == Qt::Key_Up) {
      delta.setY(step);
    } else {
      delta.setY(-step);
    }
    nudge_selection(delta);
    event->accept();
    break;
  }
  case Qt::Key_D:
    if ((event->modifiers() & Qt::ControlModifier) != 0U) {
      duplicate_selection();
      event->accept();
    } else {
      QWidget::keyPressEvent(event);
    }
    break;
  case Qt::Key_C:
    if ((event->modifiers() & Qt::ControlModifier) != 0U) {
      copy_selection();
      event->accept();
    } else {
      QWidget::keyPressEvent(event);
    }
    break;
  case Qt::Key_V:
    if ((event->modifiers() & Qt::ControlModifier) != 0U) {
      paste_at_cursor();
      event->accept();
    } else {
      QWidget::keyPressEvent(event);
    }
    break;
  case Qt::Key_A:
    if ((event->modifiers() & Qt::ControlModifier) != 0U) {
      select_all();
      event->accept();
    } else {
      QWidget::keyPressEvent(event);
    }
    break;
  case Qt::Key_F:
    frame_selection();
    event->accept();
    break;
  case Qt::Key_Escape:
    if (m_band_active) {
      m_band_active = false;
      update();
    } else if (m_is_placing_linear) {
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
      set_selection(-1, -1);
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
