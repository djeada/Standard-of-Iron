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
#include <QPointer>
#include <QPolygonF>
#include <QResizeEvent>
#include <QSet>
#include <QSizeF>
#include <QThreadPool>
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
#include "hill_projection_model.h"
#include "map_canvas_internal.h"
#include "map_json_keys.h"
#include "move_handle.h"
#include "tool_catalog.h"
#include "town_block.h"
#include "wall_geometry.h"

namespace MapEditor {

using CanvasDetail::default_terrain_element;
using CanvasDetail::default_wildlife_radius;

namespace {

constexpr int k_backdrop_debounce_ms = 90;
constexpr int k_ambient_frame_ms = 66;
constexpr qint64 k_ambient_paint_budget_ms = 14;

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

auto placement_layer(ToolType tool) -> int {
  switch (tool_placement(tool)) {
  case ToolPlacement::Terrain:
    return MapCanvas::LayerTerrain;
  case ToolPlacement::WorldProp:
    return MapCanvas::LayerWorldProp;
  case ToolPlacement::Linear:
    return MapCanvas::LayerLinear;
  case ToolPlacement::Structure:
  case ToolPlacement::TownBlock:
  case ToolPlacement::Gate:
    return MapCanvas::LayerStructure;
  case ToolPlacement::Troop:
    return MapCanvas::LayerTroopSpawn;
  case ToolPlacement::Forest:
    return MapCanvas::LayerForest;
  case ToolPlacement::Wildlife:
    return MapCanvas::LayerWildlifeArea;
  case ToolPlacement::UndeadZone:
    return MapCanvas::LayerUndeadZone;
  case ToolPlacement::None:
  case ToolPlacement::Erase:
    return -1;
  }
  return -1;
}

} // namespace

MapCanvas::MapCanvas(QWidget* parent)
    : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(360, 300);
  m_layer_visible.fill(true);
  setAttribute(Qt::WA_OpaquePaintEvent);

  m_backdrop_debounce.setSingleShot(true);
  m_backdrop_debounce.setInterval(k_backdrop_debounce_ms);
  connect(
      &m_backdrop_debounce, &QTimer::timeout, this, &MapCanvas::start_world_backdrop);

  m_world_clock.start();
  m_ambient_timer.setInterval(k_ambient_frame_ms);
  connect(&m_ambient_timer, &QTimer::timeout, this, [this]() {
    if (m_living_world && m_gesture == PointerGesture::None && painting_world() &&
        isVisible() && m_last_paint_ms <= k_ambient_paint_budget_ms) {
      update();
    }
  });
  m_ambient_timer.start();
}

void MapCanvas::set_world_view(bool enabled) {
  if (m_world_view == enabled) {
    return;
  }
  cancel_pointer_gesture();
  m_world_view = enabled;
  if (preview_pending()) {
    m_backdrop_debounce.start();
  } else {
    m_backdrop_debounce.stop();
  }
  emit preview_changed();
  update();
}

void MapCanvas::set_living_world(bool enabled) {
  m_living_world = enabled;
  update();
}

bool MapCanvas::preview_pending() const {
  return m_world_view && m_map_data != nullptr &&
         m_backdrop_completed != m_backdrop_requested;
}

bool MapCanvas::edits_paused() const {
  return preview_pending() && m_backdrop_completed < m_backdrop_required;
}

QString MapCanvas::preview_error() const {
  return m_world_view && !preview_pending() ? m_backdrop.error : QString{};
}

void MapCanvas::invalidate_world_backdrop(WorldPreviewChange change) {
  ++m_backdrop_requested;
  // Terrain and paths are baked into the raster. Scenery edits draw live while
  // the worker refreshes their effect on procedural decoration.
  if (change == WorldPreviewChange::Terrain) {
    m_backdrop_required = m_backdrop_requested;
  }
  if (preview_pending()) {
    m_backdrop_debounce.start();
    emit preview_changed();
    update();
  }
}

void MapCanvas::start_world_backdrop() {
  if (!preview_pending() || m_backdrop_running) {
    return;
  }
  m_backdrop_running = true;
  const quint64 generation = m_backdrop_requested;
  const QByteArray document = m_map_data->to_canonical_json_bytes();
  const WorldBackdropOptions options{.show_terrain = layer_visible(LayerTerrain),
                                     .show_paths = layer_visible(LayerLinear)};
  const QPointer<MapCanvas> guard(this);
  QThreadPool::globalInstance()->start([guard, generation, document, options]() {
    WorldBackdrop painted = render_world_backdrop(document, options);
    QMetaObject::invokeMethod(
        qApp,
        [guard, generation, painted = std::move(painted)]() mutable {
          if (guard.isNull()) {
            return;
          }
          guard->m_backdrop_running = false;
          if (generation == guard->m_backdrop_requested) {
            guard->m_backdrop_completed = generation;
            guard->m_backdrop = std::move(painted);
            guard->update_canvas_cursor(guard->m_last_mouse_pos);
            emit guard->preview_changed();
            guard->update();
          }
          if (!guard->m_backdrop_debounce.isActive()) {
            guard->start_world_backdrop();
          }
        },
        Qt::QueuedConnection);
  });
}

void MapCanvas::set_map_data(MapData* data) {
  if (m_map_data == data) {
    return;
  }
  if (m_map_data != nullptr) {
    disconnect(m_map_data, nullptr, this, nullptr);
  }
  cancel_pointer_gesture();
  m_is_placing_linear = false;
  m_map_data = data;
  m_selection.clear();
  m_selected_refs.clear();
  m_front_order.clear();
  m_back_order.clear();
  m_element_counts.fill(0);
  m_backdrop = {};
  if (m_map_data != nullptr) {
    connect(m_map_data, &MapData::data_changed, this, [this]() {
      if (m_gesture == PointerGesture::Move) {
        cancel_pointer_gesture();
      }
      drop_stale_view_state();
      update();
    });
    connect(m_map_data,
            &MapData::world_preview_changed,
            this,
            &MapCanvas::invalidate_world_backdrop);
    drop_stale_view_state();
    zoom_to_fit();
  } else {
    m_backdrop_debounce.stop();
    emit preview_changed();
  }
  invalidate_world_backdrop(WorldPreviewChange::Terrain);
  notify_selection_changed();
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
    m_selected_refs = QSet<ElementRef>(m_selection.cbegin(), m_selection.cend());
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
  cancel_pointer_gesture();
  m_current_tool = tool;
  m_is_placing_linear = false;
  m_hovered = {};
  const int layer = placement_layer(tool);
  if (layer >= 0) {
    set_layer_visible(layer, true);
  }
  update_canvas_cursor(m_last_mouse_pos);
  emit status_hint_changed({});
  update();
}

void MapCanvas::clear_tool() {
  set_current_tool(ToolType::Select);
  emit tool_cleared();
}

void MapCanvas::update_canvas_cursor(const QPoint& pos) {
  if (m_gesture == PointerGesture::Pan || m_gesture == PointerGesture::Move) {
    setCursor(Qt::ClosedHandCursor);
    return;
  }

  if (m_space_pan_active || m_read_only) {
    setCursor(Qt::OpenHandCursor);
    return;
  }

  if (move_handle_at(pos) != MoveHandle::None) {
    setCursor(Qt::SizeAllCursor);
    return;
  }
  if (m_current_tool == ToolType::Select || edits_paused()) {
    setCursor(m_hovered.is_valid() ? Qt::PointingHandCursor : Qt::OpenHandCursor);
  } else {
    setCursor(Qt::CrossCursor);
  }
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
  QVector<ElementRef> selection;
  QSet<ElementRef> selected_refs;
  for (const ElementRef& ref : refs) {
    if (m_map_data != nullptr && layer_visible(ref.kind) &&
        ElementOps::index_valid(*m_map_data, ref.kind, ref.index) &&
        !selected_refs.contains(ref)) {
      selection.append(ref);
      selected_refs.insert(ref);
    }
  }
  if (m_selection == selection) {
    return;
  }
  if (m_gesture == PointerGesture::Move) {
    cancel_pointer_gesture();
  }
  m_selection = std::move(selection);
  m_selected_refs = std::move(selected_refs);
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
    m_selected_refs.remove(ref);
  } else {
    m_selection.append(ref);
    m_selected_refs.insert(ref);
  }
  notify_selection_changed();
}

void MapCanvas::notify_selection_changed() {
  const ElementRef primary = primary_selection();
  emit selection_changed(primary.kind, primary.index);
  update();
}

bool MapCanvas::is_selected_element(int kind, int index) const {
  return m_selected_refs.contains(ElementRef{kind, index});
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
  case LayerLinear:
    return QStringLiteral("Paths & crossings");
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
  cancel_pointer_gesture();

  if (!visible) {
    const qsizetype before = m_selection.size();
    m_selection.removeIf([layer](const ElementRef& ref) { return ref.kind == layer; });
    if (m_selection.size() != before) {
      m_selected_refs = QSet<ElementRef>(m_selection.cbegin(), m_selection.cend());
      notify_selection_changed();
    }
  }

  if (layer == LayerTerrain || layer == LayerLinear) {
    invalidate_world_backdrop(WorldPreviewChange::Terrain);
  }
  m_hovered = {};
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

  m_fit_view = false;
  m_zoom = new_zoom;
  m_pan_offset =
      anchor_widget_pos - (anchor_widget_pos - m_pan_offset) * (m_zoom / old_zoom);
  emit zoom_changed(m_zoom);
  update();
}

void MapCanvas::pan_by(const QPointF& delta) {
  if (!delta.isNull()) {
    m_fit_view = false;
    m_pan_offset += delta;
    update();
  }
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

  const QRectF bounds = map_coordinate_bounds();
  if (bounds.isEmpty()) {
    return;
  }

  constexpr double k_fit_margin = 0.94;
  const double fit_x = width() * k_fit_margin / (bounds.width() * grid_cell_size);
  const double fit_y = height() * k_fit_margin / (bounds.height() * grid_cell_size);

  m_zoom = std::clamp(static_cast<float>(std::min(fit_x, fit_y)), min_zoom, max_zoom);
  center_on_grid_pos(bounds.center());
  m_fit_view = true;
  emit zoom_changed(m_zoom);
}

void MapCanvas::center_on_grid_pos(const QPointF& grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  m_fit_view = false;
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
  if (!edit_available() || m_map_data == nullptr || m_selection.isEmpty()) {
    return;
  }

  std::unique_ptr<Command> cmd = ElementOps::make_remove_many(*m_map_data, m_selection);
  if (!cmd) {
    return;
  }

  const qsizetype removed = m_selection.size();
  m_selection.clear();
  m_selected_refs.clear();
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
  if (!edit_available() || m_map_data == nullptr || !has_clipboard()) {
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
  if (!edit_available() || m_map_data == nullptr) {
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
  if (!edit_available() || m_map_data == nullptr || m_selection.isEmpty()) {
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

  const QRectF bounds = map_coordinate_bounds();
  double min_dx = std::numeric_limits<double>::lowest();
  double max_dx = std::numeric_limits<double>::max();
  double min_dy = std::numeric_limits<double>::lowest();
  double max_dy = std::numeric_limits<double>::max();

  for (const ElementSnapshot& snap : snaps) {
    const std::optional<QPointF> pos = ElementOps::position(snap);
    if (!pos.has_value()) {
      continue;
    }
    min_dx = std::max(min_dx, bounds.left() - pos->x());
    max_dx = std::min(max_dx, bounds.right() - pos->x());
    min_dy = std::max(min_dy, bounds.top() - pos->y());
    max_dy = std::min(max_dy, bounds.bottom() - pos->y());
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

  const QRectF bounds = map_coordinate_bounds();
  return {std::clamp(grid_pos.x(), bounds.left(), bounds.right()),
          std::clamp(grid_pos.y(), bounds.top(), bounds.bottom())};
}

QRectF MapCanvas::map_coordinate_bounds() const {
  if (m_map_data == nullptr) {
    return {};
  }
  const GridSettings& grid = m_map_data->grid();
  const double width = std::max(0, grid.width);
  const double height = std::max(0, grid.height);
  if (m_map_data->uses_world_coordinates()) {
    const double tile = std::max(grid.tile_size, 0.0001F);
    return {(0.5 - width * 0.5) * tile,
            (0.5 - height * 0.5) * tile,
            width * tile,
            height * tile};
  }
  return {0, 0, width, height};
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

QRectF MapCanvas::move_handle_for(const ElementSnapshot& snap) const {
  const auto pos = ElementOps::position(snap);
  return pos.has_value() ? move_handle_rect(grid_to_widget(pos->x(), pos->y()), rect())
                         : QRectF{};
}

QRectF MapCanvas::endpoint_handle(const LinearElement& line, int endpoint) const {
  const QVector2D pos = endpoint == 0 ? line.start : line.end;
  return QRectF(grid_to_widget(pos.x(), pos.y()), QSizeF(0, 0)).adjusted(-7, -7, 7, 7);
}

MapCanvas::MoveHandle MapCanvas::move_handle_at(const QPoint& pos) const {
  if (!edit_available() || m_current_tool != ToolType::Select || !has_selection()) {
    return MoveHandle::None;
  }

  const ElementSnapshot snap = selected_snapshot();
  if (move_handle_for(snap).contains(pos)) {
    return MoveHandle::Whole;
  }

  const auto* line = std::get_if<LinearElement>(&snap);
  if (line != nullptr && selection_count() == 1 && !is_ring_river(*line)) {
    if (endpoint_handle(*line, 0).contains(pos)) {
      return MoveHandle::Start;
    }
    if (endpoint_handle(*line, 1).contains(pos)) {
      return MoveHandle::End;
    }
  }
  return MoveHandle::None;
}

const ElementSnapshot* MapCanvas::drag_preview(const ElementRef& ref) const {
  const auto it = m_drag_indices.constFind(ref);
  return it != m_drag_indices.cend() ? &m_drag_after.at(it.value()) : nullptr;
}

void MapCanvas::begin_move(MoveHandle handle, const QPoint& pos) {
  m_gesture = PointerGesture::Move;
  m_active_handle = handle;
  m_drag_press_grid = map_to_grid(pos);
  m_drag_before = selected_snapshots();
  m_drag_after = m_drag_before;
  for (qsizetype i = 0; i < m_selection.size(); ++i) {
    const ElementRef ref = m_selection[i];
    const auto* line = std::get_if<LinearElement>(&m_drag_before[i]);
    m_drag_changes_terrain |= ref.kind == LayerTerrain ||
                              (line != nullptr && line->type != QLatin1String("wall"));
    m_drag_indices.insert(ref, i);
  }
  const QString subject =
      handle == MoveHandle::Whole
          ? (selection_count() == 1
                 ? ElementOps::display_name(m_drag_before.back())
                 : QStringLiteral("%1 elements").arg(selection_count()))
          : QStringLiteral("endpoint");
  emit status_hint_changed(
      QStringLiteral("Moving %1 · Release to place · Esc cancels").arg(subject));
}

void MapCanvas::update_move(const QPoint& pos, bool snap) {
  if (!m_drag_dist_reached) {
    m_drag_dist_reached =
        (pos - m_press_pos).manhattanLength() >= QApplication::startDragDistance();
  }
  if (!m_drag_dist_reached) {
    return;
  }

  const QPointF raw_delta = map_to_grid(pos) - m_drag_press_grid;
  const QPointF delta = snap ? snap_pos(raw_delta) : raw_delta;
  if (m_active_handle == MoveHandle::Whole) {
    const QPointF clamped = clamp_group_delta(m_drag_before, delta);
    for (qsizetype i = 0; i < m_drag_before.size(); ++i) {
      m_drag_after[i] = ElementOps::translated(m_drag_before[i], clamped);
    }
  } else {
    LinearElement line = std::get<LinearElement>(m_drag_before.back());
    const bool start = m_active_handle == MoveHandle::Start;
    QVector2D& endpoint = start ? line.start : line.end;
    const QVector2D anchor = start ? line.end : line.start;
    const QPointF target = endpoint.toPointF() + delta;
    endpoint = QVector2D(clamp_to_grid(target));
    if (line.type == QLatin1String("wall")) {
      endpoint = axis_aligned_endpoint(anchor, endpoint);
    }
    if (line.waypoints.size() >= 2) {
      (start ? line.waypoints.first() : line.waypoints.last()) = endpoint.toPointF();
    }
    m_drag_after.back() = std::move(line);
  }
  update();
}

void MapCanvas::finish_move() {
  QVector<ElementRef> refs;
  QVector<ElementSnapshot> before;
  QVector<ElementSnapshot> after;
  for (qsizetype i = 0; i < m_drag_before.size(); ++i) {
    if (ElementOps::has_moved(m_drag_before[i], m_drag_after[i])) {
      refs.append(m_selection[i]);
      before.append(m_drag_before[i]);
      after.append(m_drag_after[i]);
    }
  }
  cancel_pointer_gesture();

  if (refs.isEmpty() || m_map_data == nullptr || !edit_available()) {
    return;
  }
  const QString description =
      refs.size() == 1
          ? QStringLiteral("Move %1").arg(ElementOps::display_name(after.front()))
          : QStringLiteral("Move %1 elements").arg(refs.size());
  if (auto command =
          ElementOps::make_update_many(*m_map_data, refs, before, after, description)) {
    m_map_data->execute_command(std::move(command));
    report_converted(ElementOps::count_generated(before));
  }
}

void MapCanvas::cancel_pointer_gesture() {
  const bool was_moving = m_gesture == PointerGesture::Move;
  m_gesture = PointerGesture::None;
  m_gesture_button = Qt::NoButton;
  m_band_active = false;
  m_active_handle = MoveHandle::None;
  m_drag_indices.clear();
  m_drag_before.clear();
  m_drag_after.clear();
  m_drag_dist_reached = false;
  m_drag_changes_terrain = false;
  if (was_moving) {
    emit status_hint_changed({});
  }
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
  const QPoint pos = event->pos();
  m_pointer_inside = true;

  if (event->button() == Qt::RightButton) {
    if (m_gesture == PointerGesture::Move) {
      cancel_pointer_gesture();
      update_canvas_cursor(pos);
      update();
      return;
    }
    if (m_gesture != PointerGesture::None || !edit_available()) {
      return;
    }
    switch (right_click_action(m_is_placing_linear, m_current_tool)) {
    case RightClickAction::CancelLinearDraw:
      m_is_placing_linear = false;
      emit status_hint_changed({});
      update();
      break;
    case RightClickAction::ClearTool:
      clear_tool();
      break;
    case RightClickAction::ShowContextMenu:
      show_context_menu(pos);
      break;
    }
    return;
  }

  if (m_gesture_button != Qt::NoButton ||
      (event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton)) {
    return;
  }
  setFocus(Qt::MouseFocusReason);
  m_last_mouse_pos = pos;
  m_press_pos = pos;
  m_gesture_button = event->button();

  if (is_forced_pan_gesture(event) || m_read_only ||
      (edits_paused() && m_current_tool != ToolType::Select)) {
    m_gesture = PointerGesture::Pan;
  } else {
    const bool extend = event->modifiers().testFlag(Qt::ShiftModifier);
    const MoveHandle handle = move_handle_at(pos);
    if (handle != MoveHandle::None) {
      begin_move(handle, pos);
    } else if (extend && m_current_tool == ToolType::Select) {
      m_gesture = PointerGesture::ExtendSelection;
      m_band_origin = pos;
      m_band_current = pos;
    } else {
      m_gesture = PointerGesture::Click;
    }
  }
  update_canvas_cursor(pos);
  update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != m_gesture_button) {
    return;
  }
  const QPoint pos = event->pos();
  const bool snap = !event->modifiers().testFlag(Qt::ShiftModifier);
  const bool moved =
      (pos - m_press_pos).manhattanLength() >= QApplication::startDragDistance();

  if (m_gesture == PointerGesture::Move) {
    update_move(pos, snap);
    finish_move();
  } else if (m_gesture == PointerGesture::Pan) {
    pan_by(pos - m_last_mouse_pos);
  } else if (m_gesture == PointerGesture::ExtendSelection) {
    if (m_band_active || moved) {
      m_band_current = pos;
      finish_rubber_band();
    } else if (const ElementRef hit = hit_test(pos); hit.is_valid()) {
      toggle_selection(hit);
    }
  } else if (m_gesture == PointerGesture::Click) {
    if (moved) {
      pan_by(pos - m_press_pos);
    } else if (m_current_tool == ToolType::Select) {
      const ElementRef hit = hit_test(pos);
      if (!m_selected_refs.contains(hit)) {
        set_selection(hit.kind, hit.index);
      } else if (hit != primary_selection()) {
        m_selection.removeAll(hit);
        m_selection.append(hit);
        notify_selection_changed();
      }
    } else if (edit_available()) {
      place_at_pointer(pos, snap);
    }
  }

  cancel_pointer_gesture();
  m_last_mouse_pos = pos;
  m_hovered = {};
  update_canvas_cursor(pos);
  update();
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event) {
  const QPoint pos = event->pos();
  const QPoint previous = m_last_mouse_pos;
  const QPoint delta = pos - m_last_mouse_pos;
  m_pointer_inside = rect().contains(pos);
  m_last_mouse_pos = pos;
  const bool moved =
      (pos - m_press_pos).manhattanLength() >= QApplication::startDragDistance();
  const bool snap = !event->modifiers().testFlag(Qt::ShiftModifier);

  if (m_gesture == PointerGesture::Click && moved) {
    m_gesture = PointerGesture::Pan;
    m_hovered = {};
    pan_by(pos - m_press_pos);
  } else if (m_gesture == PointerGesture::Pan) {
    pan_by(delta);
  } else if (m_gesture == PointerGesture::ExtendSelection && (m_band_active || moved)) {
    m_band_active = true;
    m_band_current = pos;
    update();
  } else if (m_gesture == PointerGesture::Move) {
    update_move(pos, snap);
  } else if (m_gesture == PointerGesture::None && !m_read_only &&
             (m_current_tool == ToolType::Select ||
              m_current_tool == ToolType::Eraser)) {
    const MoveHandle handle = move_handle_at(pos);
    const ElementRef hover = handle == MoveHandle::None && !m_space_pan_active
                                 ? hit_test(pos)
                                 : ElementRef{};
    if (hover != m_hovered || handle != move_handle_at(previous)) {
      m_hovered = hover;
      update();
    }
  }

  const QPointF cursor_grid = map_to_grid(pos);
  m_placement_pos = clamp_to_grid(snap ? snap_pos(cursor_grid) : cursor_grid);
  if (m_current_tool != ToolType::Select) {
    update();
  }
  emit cursor_moved(static_cast<int>(std::floor(cursor_grid.x())),
                    static_cast<int>(std::floor(cursor_grid.y())));
  update_canvas_cursor(pos);
}

void MapCanvas::place_at_pointer(const QPoint& pos, bool snap) {
  if (m_map_data == nullptr || !map_rect_px().contains(pos)) {
    return;
  }
  if (m_current_tool == ToolType::Eraser) {
    erase_at_position(pos);
    return;
  }
  const int layer = placement_layer(m_current_tool);
  if (!layer_visible(layer)) {
    emit action_feedback(QStringLiteral("Enable %1 in Layers to place this tool.")
                             .arg(layer_label(layer)),
                         false);
    return;
  }
  const QPointF raw_pos = map_to_grid(pos);
  const QPointF grid_pos = clamp_to_grid(snap ? snap_pos(raw_pos) : raw_pos);
  if (!is_linear_tool(m_current_tool)) {
    place_element(grid_pos);
  } else if (m_is_placing_linear) {
    finish_linear_element(grid_pos);
  } else {
    start_linear_element(grid_pos);
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
    if (!m_selected_refs.contains(ref)) {
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

  const ElementRef hit = hit_test(pos);

  const QPointF grid_pos = clamp_to_grid(snap_pos(map_to_grid(pos)));
  const QPoint global_pos = mapToGlobal(pos);

  QMenu menu(this);

  if (hit.kind >= 0 && hit.index >= 0) {

    if (!is_selected_element(hit.kind, hit.index)) {
      set_selection(hit.kind, hit.index);
    }
    const ElementSnapshot snap = ElementOps::snapshot(*m_map_data, hit.kind, hit.index);
    const bool multi = m_selection.size() > 1;

    auto* header = menu.addAction(
        multi ? QStringLiteral("%1 elements selected").arg(m_selection.size())
              : QStringLiteral("%1: %2").arg(ElementOps::category_label(hit.kind),
                                             ElementOps::display_name(snap)));
    header->setEnabled(false);
    menu.addSeparator();

    const int element_type = hit.kind;
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
            &MapCanvas::resize_requested);
  }

  menu.exec(global_pos);
}

bool MapCanvas::event(QEvent* event) {
  if (event->type() == QEvent::UngrabMouse || event->type() == QEvent::FocusOut ||
      event->type() == QEvent::Hide) {
    if (m_gesture_button != Qt::NoButton || m_space_pan_active) {
      cancel_pointer_gesture();
      m_space_pan_active = false;
      update_canvas_cursor(m_last_mouse_pos);
      update();
    }
  } else if (event->type() == QEvent::ToolTip) {
    auto* help_event = static_cast<QHelpEvent*>(event);
    const MoveHandle handle = move_handle_at(help_event->pos());
    QString text;
    if (handle == MoveHandle::Whole) {
      text =
          QStringLiteral("Drag to move the selection. Hold Shift for free movement.");
    } else if (handle != MoveHandle::None) {
      text = QStringLiteral("Drag to move this endpoint.");
    } else if (m_map_data != nullptr) {
      text = ElementOps::summary(
          ElementOps::snapshot(*m_map_data, hit_test(help_event->pos())));
    }
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

void MapCanvas::leaveEvent(QEvent* event) {
  m_pointer_inside = false;
  m_hovered = {};
  update();
  QWidget::leaveEvent(event);
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !edit_available() ||
      m_current_tool != ToolType::Select || is_forced_pan_gesture(event) ||
      move_handle_at(event->pos()) != MoveHandle::None) {
    return;
  }
  if (const ElementRef hit = hit_test(event->pos()); hit.is_valid()) {
    set_selection(hit.kind, hit.index);
    emit element_double_clicked(hit.kind, hit.index);
  }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
  const QPointF cursor_pos = event->position();
  const int angle = event->angleDelta().y();
  const double steps = angle != 0 ? angle / 120.0 : event->pixelDelta().y() / 60.0;
  if (steps == 0.0) {
    return;
  }
  apply_zoom(m_zoom * static_cast<float>(std::pow(1.12, steps)), cursor_pos);
  const QPointF cursor_grid = map_to_grid(cursor_pos.toPoint());
  m_placement_pos = clamp_to_grid(event->modifiers().testFlag(Qt::ShiftModifier)
                                      ? cursor_grid
                                      : snap_pos(cursor_grid));
  update_canvas_cursor(cursor_pos.toPoint());
  event->accept();
}

void MapCanvas::resizeEvent(QResizeEvent* event) {
  if (m_fit_view) {
    zoom_to_fit();
  } else if (event->oldSize().isValid()) {
    const QSize delta = event->size() - event->oldSize();
    m_pan_offset += QPointF(delta.width() * 0.5, delta.height() * 0.5);
    update();
  }
}

ElementRef MapCanvas::hit_test(const QPoint& pos) const {
  ElementRef result;
  if (m_map_data == nullptr) {
    return result;
  }

  const auto& linear = m_map_data->linear_elements();
  if (layer_visible(LayerLinear)) {
    double closest = std::numeric_limits<double>::infinity();
    for (int i = linear.size() - 1; i >= 0; --i) {
      const auto& line = linear[i];
      if (line.type == QLatin1String("ford") || is_fordable_river(line)) {
        const QRectF marker = ford_marker_bounds(line);
        if (!marker.contains(pos)) {
          continue;
        }
        const QPointF delta = marker.center() - pos;
        const double distance = std::hypot(delta.x(), delta.y());
        if (distance < closest) {
          result = {LayerLinear, i};
          closest = distance;
        }
      }
    }
    if (result.is_valid()) {
      return result;
    }
  }

  const QVector2D cursor(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
  const float point_hit_radius_px = static_cast<float>(marker_radius_px()) + 4.0F;
  float best_dist = std::numeric_limits<float>::infinity();
  int best_priority = std::numeric_limits<int>::max();

  auto consider_hit = [&](int element_type,
                          int index,
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
      result = {element_type, index};
    }
  };

  const auto& troop_spawns = m_map_data->troop_spawns();
  for (int i = troop_spawns.size() - 1; i >= 0; --i) {
    const auto& elem = troop_spawns[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(4, i, (cursor - center_vec).length(), point_hit_radius_px, 0);
  }

  const auto& undead_zones = m_map_data->undead_zones();
  for (int i = undead_zones.size() - 1; i >= 0; --i) {
    const auto& elem = undead_zones[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    const float zone_radius_px = elem.radius * m_zoom * grid_cell_size;
    consider_hit(5, i, (cursor - center_vec).length(), zone_radius_px + 4.0F, 6);
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
    consider_hit(3, i, (cursor - center_vec).length(), reach, 1);
  }

  const auto& world_props = m_map_data->world_props();
  for (int i = world_props.size() - 1; i >= 0; --i) {
    const auto& elem = world_props[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(1, i, (cursor - center_vec).length(), point_hit_radius_px, 2);
  }

  const auto& terrain = m_map_data->terrain_elements();
  for (int i = terrain.size() - 1; i >= 0; --i) {
    const auto& elem = terrain[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(0, i, (cursor - center_vec).length(), terrain_hit_radius_px(elem), 3);
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
    consider_hit(LayerLinear, i, dist, line_hit_radius_px, 5);
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
    emit action_feedback(
        QStringLiteral("Placed a gate. Draw a wall through it to close the ring."));
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
    if (elem.type == QLatin1String("hill") &&
        !HillProjection::ensure_entrance(elem,
                                         HillProjection::map_context(*m_map_data))) {
      emit action_feedback(
          QStringLiteral(
              "Place the hill far enough inside the map to fit an entrance."),
          false);
      return;
    }
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
    if (const QString style = structure_style_for_tool(m_current_tool);
        !style.isEmpty()) {
      elem.extra_fields[QStringLiteral("style")] = style;
    }
    m_map_data->execute_command(std::make_unique<AddStructureCmd>(m_map_data, elem));
    break;
  }
  case ToolPlacement::TownBlock: {
    TownBlock::BlockSpec spec;
    spec.type = element_type;
    spec.style = structure_style_for_tool(m_current_tool);
    spec.alley_lot = spec.lots_wide / 2;
    spec.owner.player_id = m_current_player_id;
    spec.owner.nation = m_current_nation;
    spec.owner.max_population = default_max_population;
    const TownBlock::BlockExtent extent = TownBlock::block_extent(spec);
    spec.origin = grid_pos - QPointF(extent.width * 0.5, extent.depth * 0.5);
    std::vector<std::unique_ptr<Command>> steps;
    for (const StructureElement& house : TownBlock::block(spec)) {
      steps.push_back(std::make_unique<AddStructureCmd>(m_map_data, house));
    }
    if (!steps.empty()) {
      const int count = static_cast<int>(steps.size());
      m_map_data->execute_command(std::make_unique<CompositeCmd>(
          std::move(steps), QStringLiteral("Stamp town block")));
      emit action_feedback(QStringLiteral("Stamped a block of %1 houses.").arg(count));
    }
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

  emit status_hint_changed(
      m_current_tool == ToolType::Ford
          ? QStringLiteral("Ford · Click the opposite bank · Esc cancels")
          : QStringLiteral("%1 · Click the endpoint · Esc cancels")
                .arg(tool_name(m_current_tool)));
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

  if (elem.start == elem.end) {
    emit action_feedback(QStringLiteral("Choose a different endpoint."), false);
    return;
  }
  m_is_placing_linear = false;
  emit status_hint_changed({});
  m_map_data->execute_command(std::make_unique<AddLinearCmd>(m_map_data, elem));
}

void MapCanvas::erase_at_position(const QPoint& pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const ElementRef hit = hit_test(pos);

  std::unique_ptr<Command> cmd =
      ElementOps::make_remove(*m_map_data, hit.kind, hit.index);
  if (cmd) {
    // A deletion shifts later indices in this category.
    clear_selection();
    m_map_data->execute_command(std::move(cmd));
  }
}

void MapCanvas::keyPressEvent(QKeyEvent* event) {
  if (m_read_only) {
    QWidget::keyPressEvent(event);
    return;
  }
  if (m_gesture != PointerGesture::None && event->key() != Qt::Key_Escape &&
      event->key() != Qt::Key_Space) {
    event->accept();
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
    if (has_selection() && edit_available()) {
      nudge_selection(delta);
    } else {
      pan_by(delta * 64.0);
    }
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
    if (m_gesture != PointerGesture::None) {
      const bool was_moving = m_gesture == PointerGesture::Move;
      cancel_pointer_gesture();
      update_canvas_cursor(m_last_mouse_pos);
      if (was_moving) {
        emit action_feedback(QStringLiteral("Move cancelled."));
      }
      update();
    } else if (m_is_placing_linear) {
      m_is_placing_linear = false;
      emit status_hint_changed("");
      update();
    } else if (m_current_tool != ToolType::Select) {
      clear_tool();
    } else {
      set_selection(-1, -1);
    }
    event->accept();
    break;
  default:
    QWidget::keyPressEvent(event);
    break;
  }
}

void MapCanvas::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
    m_space_pan_active = false;
    update_canvas_cursor(m_last_mouse_pos);
    event->accept();
    return;
  }

  QWidget::keyReleaseEvent(event);
}

} // namespace MapEditor
