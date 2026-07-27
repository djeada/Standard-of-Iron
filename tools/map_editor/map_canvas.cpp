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
#include <QPolygonF>
#include <QSizeF>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

#include "canvas_transform.h"
#include "element_ops.h"
#include "spawn_icon_library.h"
#include "troop_tool_specs.h"
#include "ui/theme.h"

namespace MapEditor {

namespace {

const QColor k_canvas_background = Theme::backgroundDeep();
const QColor k_canvas_border = Theme::borderSubtle();
const QColor k_grid_line_color = Theme::panelIron();
const QColor k_grid_outline_color = Theme::borderSubtle();
const QColor k_grid_text_color = Theme::textSecondary();
const QColor k_empty_state_text(159, 217, 255);
constexpr int k_base_grid_step = 10;
constexpr float k_major_grid_spacing_px = 48.0F;
constexpr float k_minor_grid_spacing_px = 12.0F;
constexpr int k_minor_grid_alpha = 90;
const QColor k_hover_select_color(100, 200, 255);
const QColor k_hover_erase_color(255, 80, 80);

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

auto world_prop_type_for_tool(ToolType tool) -> QString {
  switch (tool) {
  case ToolType::PropFirecamp:
    return QStringLiteral("firecamp");
  case ToolType::PropTent:
    return QStringLiteral("tent");
  case ToolType::PropSupplyCart:
    return QStringLiteral("supply_cart");
  case ToolType::PropWeaponRack:
    return QStringLiteral("weapon_rack");
  case ToolType::PropRuins:
    return QStringLiteral("ruins");
  case ToolType::PropMagicShrine:
    return QStringLiteral("magic_shrine");
  case ToolType::PropDeadTree:
    return QStringLiteral("dead_tree");
  case ToolType::PropBoulder:
    return QStringLiteral("boulder");
  case ToolType::PropPineTree:
    return QStringLiteral("pine_tree");
  case ToolType::PropOliveTree:
    return QStringLiteral("olive_tree");
  case ToolType::PropPlant:
    return QStringLiteral("plant");
  case ToolType::PropIronOre:
    return QStringLiteral("iron_ore");
  default:
    return {};
  }
}

// Terrain paints back-to-front by footprint, so this only needs to rank features
// against each other, not match the pixel geometry exactly.
auto terrain_footprint_cells(const TerrainElement& elem) -> float {
  const float extent = std::max(elem.width, elem.depth);
  return std::max(extent > 0.0F ? extent : 0.0F, elem.radius);
}

// Category paint order, back to front. Terrain is first by construction: hills,
// mountains and lakes are the ground everything else stands on.
constexpr std::array<int, 6> k_category_paint_order = {
    static_cast<int>(ElementKind::Terrain),
    static_cast<int>(ElementKind::Linear),
    static_cast<int>(ElementKind::WorldProp),
    static_cast<int>(ElementKind::Structure),
    static_cast<int>(ElementKind::TroopSpawn),
    static_cast<int>(ElementKind::UndeadZone),
};

void draw_troop_marker(QPainter& painter,
                       const QPoint& pos,
                       const QString& type,
                       int player_id,
                       float badge_size) {
  const QRectF bounds(
      pos.x() - badge_size * 0.5F, pos.y() - badge_size * 0.5F, badge_size, badge_size);
  paint_troop_badge(painter, bounds, type, player_color_for_editor(player_id));
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
  pal.setColor(QPalette::Window, k_canvas_background);
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

  // Adding or removing shifts indices, and both the selection and the draw-order
  // overrides address elements by index. Drop what can no longer be trusted.
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

  // Never leave a hidden element selected or hovered: it cannot be seen and it
  // would still react to Delete and drags.
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

  // Keep the copied elements' relative layout: the first one lands on the target
  // and the rest follow by the same delta.
  const std::optional<QPointF> anchor = ElementOps::group_anchor(m_clipboard);
  if (!anchor.has_value()) {
    return;
  }
  const QPointF delta = clamp_to_grid(grid_pos) - *anchor;

  QVector<ElementSnapshot> pasted;
  pasted.reserve(m_clipboard.size());
  for (const ElementSnapshot& snap : m_clipboard) {
    pasted.append(ElementOps::translated(snap, delta));
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

  // Offset the copies by one cell so they do not hide underneath the originals.
  QVector<ElementSnapshot> copies;
  copies.reserve(snaps.size());
  for (const ElementSnapshot& snap : snaps) {
    copies.append(ElementOps::translated(snap, QPointF(1.0, 1.0)));
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

  // make_add_many appends in order, so the new elements are the last N of each
  // category.
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

  // Only elements the transform actually changes take part, so a group edit never
  // records no-op sub-commands.
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

  std::unique_ptr<Command> cmd =
      ElementOps::make_update_many(*m_map_data, refs, before, after, description);
  if (cmd) {
    m_map_data->execute_command(std::move(cmd));
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
  apply_to_selection(
      [player_id](const ElementSnapshot& snap) -> std::optional<ElementSnapshot> {
        if (!ElementOps::supports_player_id(snap) ||
            ElementOps::player_id(snap) == player_id) {
          return std::nullopt;
        }
        return ElementOps::with_player_id(snap, player_id);
      },
      QStringLiteral("Set player %1").arg(player_id));
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

  // A group keeps its shape: shrink the shared delta until every member stays
  // inside the map rather than clamping each element on its own.
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
    return {}; // nothing can move without leaving the map
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

void MapCanvas::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.fillRect(rect(), k_canvas_background);
  painter.setPen(QPen(k_canvas_border, 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  if (m_map_data == nullptr) {
    painter.setPen(k_empty_state_text);
    painter.drawText(rect(), Qt::AlignCenter, "No map loaded");
    return;
  }

  draw_grid(painter);
  draw_fog_zones(painter);

  // Elements pushed to the back are painted before every category, terrain
  // included; elements brought to the front are painted after all of them.
  for (const ElementRef& ref : m_back_order) {
    draw_one_element(painter, ref);
  }
  for (int kind : k_category_paint_order) {
    draw_category(painter, kind);
  }
  draw_mission_overlays(painter);
  for (const ElementRef& ref : m_front_order) {
    draw_one_element(painter, ref);
  }

  draw_linear_preview(painter);
  draw_current_placement(painter);
  draw_rubber_band(painter);

  const bool is_empty =
      m_map_data->terrain_elements().isEmpty() && m_map_data->world_props().isEmpty() &&
      m_map_data->linear_elements().isEmpty() && m_map_data->structures().isEmpty() &&
      m_map_data->troop_spawns().isEmpty() && m_map_data->undead_zones().isEmpty();
  if (is_empty) {
    painter.setOpacity(0.55);
    painter.setPen(k_empty_state_text);
    QFont hint_font = painter.font();
    hint_font.setPointSize(11);
    painter.setFont(hint_font);
    painter.drawText(rect().adjusted(0, 24, 0, 0),
                     Qt::AlignHCenter | Qt::AlignTop,
                     "Select a tool from the panel and click to add elements");
    painter.setOpacity(1.0);
  }
}

void MapCanvas::draw_fog_zones(QPainter& painter) {
  if (m_map_data == nullptr || !m_layer_visible[LayerFogZone]) {
    return;
  }

  painter.save();
  for (const FogZoneElement& zone : m_map_data->fog_zones()) {
    const QPoint center = grid_to_widget(zone.x, zone.z);
    const int width_px =
        std::max(1, static_cast<int>(zone.width * m_zoom * grid_cell_size));
    const int height_px =
        std::max(1, static_cast<int>(zone.height * m_zoom * grid_cell_size));
    const QRect bounds(
        center.x() - width_px / 2, center.y() - height_px / 2, width_px, height_px);
    const int alpha = std::clamp(static_cast<int>(zone.density * 105.0F), 20, 125);
    painter.setBrush(QColor(150, 175, 190, alpha));
    painter.setPen(QPen(QColor(190, 220, 235, 170), 1, Qt::DashLine));
    painter.drawRect(bounds);
    painter.setPen(QColor(220, 235, 245, 210));
    painter.drawText(bounds.adjusted(4, 2, -4, -2),
                     Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("FOG %1%").arg(qRound(zone.density * 100.0F)));
  }
  painter.restore();
}

void MapCanvas::draw_grid(QPainter& painter) {
  if (m_map_data == nullptr) {
    return;
  }

  const GridSettings& grid = m_map_data->grid();
  float const cell_size = grid_cell_size * m_zoom;

  if (cell_size <= 0.0F || grid.width <= 0 || grid.height <= 0) {
    return;
  }

  auto const start_x = static_cast<float>(m_pan_offset.x());
  auto const start_y = static_cast<float>(m_pan_offset.y());
  float const end_x = start_x + grid.width * cell_size;
  float const end_y = start_y + grid.height * cell_size;

  float const clip_left = std::max(0.0F, start_x);
  float const clip_right = std::min(static_cast<float>(width()), end_x);
  float const clip_top = std::max(0.0F, start_y);
  float const clip_bottom = std::min(static_cast<float>(height()), end_y);

  if (clip_right <= clip_left || clip_bottom <= clip_top) {
    return;
  }

  int const major_step = CanvasTransform::grid_step_for_spacing(
      cell_size, k_major_grid_spacing_px, k_base_grid_step);
  int const minor_step =
      CanvasTransform::grid_step_for_spacing(cell_size, k_minor_grid_spacing_px, 1);

  auto draw_grid_lines = [&](int step, const QPen& pen) {
    painter.setPen(pen);

    int const first_col =
        static_cast<int>(std::floor((clip_left - start_x) / cell_size)) / step * step;
    int const last_col = std::min(
        grid.width, static_cast<int>(std::ceil((clip_right - start_x) / cell_size)));
    for (int i = std::max(0, first_col); i <= last_col; i += step) {
      float const x = start_x + static_cast<float>(i) * cell_size;
      painter.drawLine(QPointF(x, clip_top), QPointF(x, clip_bottom));
    }

    int const first_row =
        static_cast<int>(std::floor((clip_top - start_y) / cell_size)) / step * step;
    int const last_row = std::min(
        grid.height, static_cast<int>(std::ceil((clip_bottom - start_y) / cell_size)));
    for (int i = std::max(0, first_row); i <= last_row; i += step) {
      float const y = start_y + static_cast<float>(i) * cell_size;
      painter.drawLine(QPointF(clip_left, y), QPointF(clip_right, y));
    }
  };

  if (minor_step < major_step && major_step % minor_step == 0) {
    QColor minor_color = k_grid_line_color;
    minor_color.setAlpha(k_minor_grid_alpha);
    draw_grid_lines(minor_step, QPen(minor_color, 1));
  }
  draw_grid_lines(major_step, QPen(k_grid_line_color, 1));

  painter.setPen(QPen(k_grid_outline_color, 2));
  painter.drawRect(
      QRectF(start_x, start_y, grid.width * cell_size, grid.height * cell_size));

  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);

  // Ruler ticks along the top and left edges of the visible map area. The view is
  // mirrored (grid x = grid.width - view column), so labels count down as columns
  // count up.
  painter.setPen(k_grid_text_color);
  const float label_y = std::clamp(start_y, clip_top, clip_bottom - 4.0F) + 11.0F;
  const float label_x = std::clamp(start_x, clip_left, clip_right - 4.0F) + 3.0F;

  int const first_col =
      static_cast<int>(std::floor((clip_left - start_x) / cell_size)) / major_step *
      major_step;
  int const last_col = std::min(
      grid.width, static_cast<int>(std::ceil((clip_right - start_x) / cell_size)));
  for (int i = std::max(0, first_col); i <= last_col; i += major_step) {
    float const x = start_x + static_cast<float>(i) * cell_size;
    if (x < clip_left || x > clip_right - 12.0F) {
      continue;
    }
    painter.drawText(QPointF(x + 3.0F, label_y), QString::number(grid.width - i));
  }

  int const first_row = static_cast<int>(std::floor((clip_top - start_y) / cell_size)) /
                        major_step * major_step;
  int const last_row = std::min(
      grid.height, static_cast<int>(std::ceil((clip_bottom - start_y) / cell_size)));
  for (int i = std::max(0, first_row); i <= last_row; i += major_step) {
    float const y = start_y + static_cast<float>(i) * cell_size;
    if (y < clip_top + 12.0F || y > clip_bottom) {
      continue;
    }
    painter.drawText(QPointF(label_x, y - 3.0F), QString::number(grid.height - i));
  }

  // Origin marker, so the mirrored axes stay unambiguous at a glance.
  const QPoint origin = grid_to_widget(0.0F, 0.0F);
  painter.setPen(k_grid_text_color);
  painter.drawText(QPointF(origin.x() - 26, origin.y() - 4), QStringLiteral("0,0"));
}

QVector<int> MapCanvas::category_draw_order(int kind) const {
  QVector<int> order;
  if (m_map_data == nullptr) {
    return order;
  }

  const int count = ElementOps::count(*m_map_data, kind);
  order.reserve(count);
  for (int i = 0; i < count; ++i) {
    order.append(i);
  }

  if (kind == static_cast<int>(ElementKind::Terrain)) {
    // Largest footprint first, so a wide mountain cannot bury the small hills
    // authored on top of it.
    const auto& terrain = m_map_data->terrain_elements();
    std::stable_sort(order.begin(), order.end(), [&terrain](int lhs, int rhs) {
      return terrain_footprint_cells(terrain[lhs]) >
             terrain_footprint_cells(terrain[rhs]);
    });
  }

  return order;
}

void MapCanvas::draw_category(QPainter& painter, int kind) {
  if (m_map_data == nullptr || !layer_visible(kind)) {
    return;
  }

  for (int index : category_draw_order(kind)) {
    const ElementRef ref{kind, index};
    if (m_front_order.contains(ref) || m_back_order.contains(ref)) {
      continue; // drawn by the front/back pass instead
    }
    draw_one_element(painter, ref);
  }
}

void MapCanvas::draw_one_element(QPainter& painter, const ElementRef& ref) {
  if (m_map_data == nullptr || !layer_visible(ref.kind) ||
      !ElementOps::index_valid(*m_map_data, ref.kind, ref.index)) {
    return;
  }

  switch (static_cast<ElementKind>(ref.kind)) {
  case ElementKind::Terrain:
    draw_terrain_element(painter, ref.index);
    break;
  case ElementKind::WorldProp:
    draw_world_prop_element(painter, ref.index);
    break;
  case ElementKind::Linear:
    draw_linear_element(painter, ref.index);
    break;
  case ElementKind::Structure:
    draw_structure_element(painter, ref.index);
    break;
  case ElementKind::TroopSpawn:
    draw_troop_spawn_element(painter, ref.index);
    break;
  case ElementKind::UndeadZone:
    draw_undead_zone_element(painter, ref.index);
    break;
  }
}

void MapCanvas::draw_terrain_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->terrain_elements()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(0, i);
  bool const is_hovered = (m_hovered_type == 0 && m_hovered_index == i);

  const QSizeF ellipse = terrain_ellipse_px(elem);
  const auto rx = static_cast<int>(ellipse.width());
  const auto ry = static_cast<int>(ellipse.height());

  draw_terrain_feature(painter, elem, pos);

  QPen outline_pen;
  if (is_selected) {
    outline_pen = QPen(Qt::yellow, 2);
  } else if (is_hovered) {
    outline_pen = QPen(hover_ring_color, 2);
  } else {
    outline_pen = QPen(Qt::white, 1);
  }
  painter.save();
  painter.setPen(outline_pen);
  painter.setBrush(Qt::NoBrush);
  painter.translate(pos);
  painter.rotate(static_cast<double>(elem.rotation));
  painter.drawEllipse(QPointF(0, 0), static_cast<double>(rx), static_cast<double>(ry));
  painter.restore();

  if (is_hovered && !is_selected) {
    painter.save();
    painter.setPen(QPen(hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.translate(pos);
    painter.rotate(static_cast<double>(elem.rotation));
    painter.drawEllipse(
        QPointF(0, 0), static_cast<double>(rx + 4), static_cast<double>(ry + 4));
    painter.restore();
  }
}

void MapCanvas::draw_world_prop_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->world_props()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(1, i);
  bool const is_hovered = (m_hovered_type == 1 && m_hovered_index == i);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(Qt::white, 1));
  }

  draw_element(painter, elem.type, pos);

  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() + 4, marker_radius_px() + 4);
    painter.restore();
  }
}

void MapCanvas::draw_structure_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->structures()[i];
  QPoint const pos = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(3, i);
  bool const is_hovered = (m_hovered_type == 3 && m_hovered_index == i);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(Qt::white, 1));
  }

  draw_element(painter, elem.type, pos, elem.player_id);

  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() + 4, marker_radius_px() + 4);
    painter.restore();
  }
}

void MapCanvas::draw_troop_spawn_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->troop_spawns()[i];
  const QPoint pos = grid_to_widget(elem.x, elem.z);

  const bool is_selected = is_selected_element(4, i);
  const bool is_hovered = (m_hovered_type == 4 && m_hovered_index == i);
  if (is_selected || is_hovered) {
    painter.save();
    painter.setPen(QPen(is_selected ? QColor(Qt::yellow) : hover_ring_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, marker_radius_px() / 2 + 6, marker_radius_px() / 2 + 6);
    painter.restore();
  }

  draw_troop_marker(painter,
                    pos,
                    elem.type,
                    elem.player_id,
                    static_cast<float>(marker_radius_px()) * 1.375F);
}

void MapCanvas::draw_linear_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;
  const auto& elem = m_map_data->linear_elements()[i];
  QPoint const start_pos = grid_to_widget(elem.start.x(), elem.start.y());
  QPoint const end_pos = grid_to_widget(elem.end.x(), elem.end.y());

  bool const is_selected = is_selected_element(2, i);
  bool const is_hovered = (m_hovered_type == 2 && m_hovered_index == i);

  QColor color;
  int line_width = static_cast<int>(elem.width * m_zoom);
  line_width = std::max(2, std::min(line_width, 20));

  if (elem.type == "river") {
    color = QColor(70, 130, 200);
  } else if (elem.type == "road") {
    color = QColor(139, 119, 101);
  } else if (elem.type == "bridge") {
    color = QColor(160, 140, 100);
  } else if (elem.type == "wall") {
    color = player_color_for_editor(elem.player_id);
  }

  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, line_width + 4));
    painter.drawLine(start_pos, end_pos);
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, line_width + 4));
    painter.drawLine(start_pos, end_pos);
  }

  painter.setPen(QPen(color, line_width));
  painter.drawLine(start_pos, end_pos);

  int const endpoint_size = 6;
  painter.setBrush(color.lighter());
  painter.setPen(Qt::white);
  painter.drawEllipse(start_pos, endpoint_size, endpoint_size);
  painter.drawEllipse(end_pos, endpoint_size, endpoint_size);
}

void MapCanvas::draw_linear_preview(QPainter& painter) {
  if (m_is_placing_linear) {
    QPoint const start_pos = grid_to_widget(static_cast<float>(m_linear_start.x()),
                                            static_cast<float>(m_linear_start.y()));
    QPointF const current_grid = map_to_grid(m_last_mouse_pos);
    QPoint const end_pos = grid_to_widget(static_cast<float>(current_grid.x()),
                                          static_cast<float>(current_grid.y()));

    QPen const pen(Qt::white, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(start_pos, end_pos);

    painter.save();
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QColor(100, 200, 255, 180));
    painter.drawEllipse(start_pos, 7, 7);
    painter.restore();
  }
}

void MapCanvas::draw_undead_zone_element(QPainter& painter, int i) {
  const QColor& hover_ring_color =
      m_current_tool == ToolType::Eraser ? k_hover_erase_color : k_hover_select_color;

  static const QColor k_zone_fill(100, 40, 140, 55);
  static const QColor k_zone_border(180, 80, 220, 200);
  static const QColor k_leash_ring(140, 100, 180, 80);

  const auto& elem = m_map_data->undead_zones()[i];
  QPoint const center = grid_to_widget(elem.x, elem.z);

  bool const is_selected = is_selected_element(5, i);
  bool const is_hovered = (m_hovered_type == 5 && m_hovered_index == i);

  const int radius_px = static_cast<int>(elem.radius * m_zoom * grid_cell_size);
  const int leash_px = static_cast<int>(elem.leash_radius * m_zoom * grid_cell_size);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(k_leash_ring, 1, Qt::DotLine));
  painter.drawEllipse(center, leash_px, leash_px);

  painter.setBrush(k_zone_fill);
  if (is_selected) {
    painter.setPen(QPen(Qt::yellow, 2));
  } else if (is_hovered) {
    painter.setPen(QPen(hover_ring_color, 2));
  } else {
    painter.setPen(QPen(k_zone_border, 1));
  }
  painter.drawEllipse(center, radius_px, radius_px);

  const QString icon = (elem.anchor_type == QStringLiteral("ruins"))
                           ? QStringLiteral("\u25A9")
                           : QStringLiteral("\u2726");
  painter.setPen(QColor(230, 180, 255));
  QFont f = painter.font();
  f.setPointSize(9);
  painter.setFont(f);
  painter.drawText(
      QRect(center.x() - 10, center.y() - 10, 20, 20), Qt::AlignCenter, icon);

  if (!elem.id.isEmpty() && labels_visible()) {
    f.setPointSize(7);
    painter.setFont(f);
    painter.setPen(QColor(210, 170, 240, 200));
    painter.drawText(QRect(center.x() - 40, center.y() + radius_px + 2, 80, 14),
                     Qt::AlignCenter,
                     elem.id);
  }

  painter.restore();
}

void MapCanvas::draw_mission_overlays(QPainter& painter) {
  if (m_mission_data == nullptr || m_map_data == nullptr ||
      !m_layer_visible[LayerMissionOverlay]) {
    return;
  }

  painter.save();
  const QJsonArray ai_setups = m_mission_data->array(QStringLiteral("ai_setups"));
  int owner_id = 2;
  for (const QJsonValue& ai_value : ai_setups) {
    const QJsonObject ai = ai_value.toObject();
    const QString ai_id = ai.value(QStringLiteral("id")).toString();

    for (const QJsonValue& wave_value : ai.value(QStringLiteral("waves")).toArray()) {
      const QJsonObject wave = wave_value.toObject();
      const QJsonObject entry = wave.value(QStringLiteral("entry_point")).toObject();
      const QPoint pos =
          grid_to_widget(static_cast<float>(entry.value("x").toDouble()),
                         static_cast<float>(entry.value("z").toDouble()));
      painter.setBrush(QColor(255, 145, 40, 55));
      painter.setPen(QPen(QColor(255, 170, 70, 230), 2, Qt::DashLine));
      painter.drawEllipse(pos, 14, 14);
      painter.drawLine(pos + QPoint(-19, 0), pos + QPoint(19, 0));
      painter.drawLine(pos + QPoint(0, -19), pos + QPoint(0, 19));
      painter.setPen(QColor(255, 205, 130));
      painter.drawText(QRect(pos.x() - 70, pos.y() + 18, 140, 18),
                       Qt::AlignHCenter | Qt::AlignTop,
                       QStringLiteral("%1 wave @ %2s")
                           .arg(ai_id)
                           .arg(wave.value("timing").toDouble(), 0, 'f', 0));
    }

    const auto draw_setup_marker = [this, &painter, owner_id](const QJsonObject& setup,
                                                              const QString& label,
                                                              bool building) {
      const QJsonObject position = setup.value("position").toObject();
      const QPoint pos =
          grid_to_widget(static_cast<float>(position.value("x").toDouble()),
                         static_cast<float>(position.value("z").toDouble()));
      if (building) {
        painter.setBrush(player_color_for_editor(owner_id));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(QRect(pos.x() - 6, pos.y() - 6, 12, 12));
      } else {
        draw_troop_marker(painter,
                          pos,
                          setup.value("type").toString(),
                          owner_id,
                          static_cast<float>(marker_radius_px()) * 1.375F);
      }
      painter.setPen(QColor(180, 225, 255));
      painter.drawText(QRect(pos.x() - 55, pos.y() + 10, 110, 16),
                       Qt::AlignHCenter | Qt::AlignTop,
                       label);
    };

    for (const QJsonValue& unit_value :
         ai.value(QStringLiteral("starting_units")).toArray()) {
      const QJsonObject unit = unit_value.toObject();
      draw_setup_marker(
          unit, ai_id + QStringLiteral(": ") + unit.value("type").toString(), false);
    }
    for (const QJsonValue& building_value :
         ai.value(QStringLiteral("starting_buildings")).toArray()) {
      const QJsonObject building = building_value.toObject();
      draw_setup_marker(building,
                        ai_id + QStringLiteral(": ") +
                            building.value("type").toString(),
                        true);
    }
    ++owner_id;
  }

  const QStringList objective_keys = {QStringLiteral("victory_conditions"),
                                      QStringLiteral("optional_objectives")};
  for (const QString& key : objective_keys) {
    for (const QJsonValue& condition_value : m_mission_data->array(key)) {
      const QJsonObject condition = condition_value.toObject();
      const QString zone_id = condition.value(QStringLiteral("zone_id")).toString();
      if (zone_id.isEmpty()) {
        continue;
      }
      for (const UndeadZoneElement& zone : m_map_data->undead_zones()) {
        if (zone.id != zone_id) {
          continue;
        }
        const QPoint pos = grid_to_widget(zone.x, zone.z);
        const int radius =
            std::max(12, static_cast<int>(zone.radius * m_zoom * grid_cell_size));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 220, 70), 3));
        painter.drawEllipse(pos, radius + 4, radius + 4);
        painter.setPen(QColor(255, 235, 130));
        painter.drawText(QRect(pos.x() - 90, pos.y() - radius - 24, 180, 18),
                         Qt::AlignCenter,
                         QStringLiteral("OBJECTIVE: %1").arg(zone_id));
      }
    }
  }
  painter.restore();
}

void MapCanvas::draw_current_placement(QPainter& painter) {
  if (m_current_tool == ToolType::Select || m_current_tool == ToolType::Eraser) {
    return;
  }

  QPointF const grid_pos = map_to_grid(m_last_mouse_pos);
  QPoint const widget_pos = grid_to_widget(static_cast<float>(grid_pos.x()),
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
  case ToolType::PropFirecamp:
  case ToolType::PropTent:
  case ToolType::PropSupplyCart:
  case ToolType::PropWeaponRack:
  case ToolType::PropRuins:
  case ToolType::PropMagicShrine:
  case ToolType::PropDeadTree:
  case ToolType::PropBoulder:
  case ToolType::PropPineTree:
  case ToolType::PropOliveTree:
  case ToolType::PropPlant:
  case ToolType::PropIronOre:
    type = world_prop_type_for_tool(m_current_tool);
    break;
  case ToolType::Barracks:
    type = "barracks";
    break;
  case ToolType::Village:
    type = "village";
    break;
  case ToolType::DefenseTower:
    type = "defense_tower";
    break;
  case ToolType::Home:
    type = "home";
    break;
  case ToolType::Marketplace:
    type = "marketplace";
    break;
  default:
    break;
  }

  if (is_troop_tool(m_current_tool)) {
    draw_troop_marker(painter,
                      widget_pos,
                      troop_type_for_tool(m_current_tool),
                      m_current_player_id,
                      static_cast<float>(marker_radius_px()) * 1.375F);
  } else if (m_current_tool == ToolType::UndeadZone) {
    const int radius_px = static_cast<int>(8.0F * m_zoom * grid_cell_size);
    painter.setBrush(QColor(100, 40, 140, 55));
    painter.setPen(QPen(QColor(180, 80, 220, 160), 1, Qt::DashLine));
    painter.drawEllipse(widget_pos, radius_px, radius_px);
    painter.setPen(QColor(230, 180, 255, 160));
    QFont f = painter.font();
    f.setPointSize(9);
    painter.setFont(f);
    painter.drawText(
        QRect(widget_pos.x() - 10, widget_pos.y() - 10, 20, 20), Qt::AlignCenter, "☠");
  } else if (!type.isEmpty()) {
    if (type == QStringLiteral("hill") || type == QStringLiteral("mountain")) {
      TerrainElement preview;
      preview.type = type;
      preview.radius = 10.0F;
      preview.width = 10.0F;
      preview.depth = 10.0F;
      draw_terrain_feature(painter, preview, widget_pos);
    } else if (type == QStringLiteral("barracks") ||
               type == QStringLiteral("village") ||
               type == QStringLiteral("defense_tower") ||
               type == QStringLiteral("home") ||
               type == QStringLiteral("marketplace")) {
      draw_element(painter, type, widget_pos, m_current_player_id);
    } else {
      draw_element(painter, type, widget_pos);
    }
  }

  painter.setOpacity(1.0);
}

void MapCanvas::draw_element(QPainter& painter,
                             const QString& type,
                             const QPoint& pos,
                             int player_id,
                             int marker_radius_px) {

  int const size = marker_radius_px > 0 ? marker_radius_px : this->marker_radius_px();

  QColor fill_color;
  QString symbol;

  if (type == "barracks") {
    fill_color = player_color_for_editor(player_id);
    symbol = "B";
  } else if (type == "village") {
    fill_color = player_color_for_editor(player_id);
    symbol = "V";
  } else if (type == "defense_tower") {
    fill_color = player_color_for_editor(player_id);
    symbol = "T";
  } else if (type == "home") {
    fill_color = player_color_for_editor(player_id);
    symbol = "H";
  } else if (type == "marketplace") {
    fill_color = player_color_for_editor(player_id);
    symbol = "M";
  } else {
    fill_color = QColor(128, 128, 128);
    symbol = "?";
  }

  if (type == "firecamp" || type == "tent" || type == "supply_cart" ||
      type == "weapon_rack" || type == "ruins" || type == "magic_shrine" ||
      type == "dead_tree" || type == "boulder" || type == "pine_tree" ||
      type == "olive_tree" || type == "plant" || type == "iron_ore") {
    draw_world_prop_icon(painter, type, pos, size);
  } else {
    painter.setBrush(fill_color);
    painter.setPen(QPen(fill_color.darker(140), 1));
    painter.drawEllipse(pos, size, size);
    QFont font = painter.font();
    const int symbol_point_size = std::clamp(size * 2 / 3, 8, 52);
    font.setPointSize(symbol_point_size);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(pos.x() - size, pos.y() - size, size * 2, size * 2),
                     Qt::AlignCenter,
                     symbol);
    if ((type == "barracks" || type == "village" || type == "defense_tower" ||
         type == "home" || type == "marketplace") &&
        player_id >= 0 && labels_visible()) {
      QString const player_text = player_id == 0 ? "N" : QString::number(player_id);
      font.setPointSize(8);
      painter.setFont(font);
      painter.setPen(Qt::black);
      painter.drawText(pos.x() + size - 6, pos.y() - size + 10, player_text);
    }
  }
}

void MapCanvas::draw_world_prop_icon(QPainter& painter,
                                     const QString& type,
                                     const QPoint& pos,
                                     int size) {
  const auto s = static_cast<float>(size);
  painter.save();
  painter.translate(pos);

  if (type == QStringLiteral("firecamp")) {
    painter.setBrush(QColor(255, 140, 0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);

    const float fw = s * 0.38F;
    const float fh = s * 0.80F;
    const auto draw_flame = [&](float ox, float oy_base, float w, float h) {
      QPainterPath flame;
      flame.moveTo(ox, oy_base);
      flame.cubicTo(
          ox - w, oy_base - h * 0.5F, ox - w * 0.6F, oy_base - h, ox, oy_base - h);
      flame.cubicTo(
          ox + w * 0.6F, oy_base - h, ox + w, oy_base - h * 0.5F, ox, oy_base);
      painter.drawPath(flame);
    };
    painter.setBrush(QColor(255, 230, 80));
    draw_flame(-fw * 0.4F, s * 0.45F, fw * 0.7F, fh * 0.75F);
    draw_flame(fw * 0.4F, s * 0.45F, fw * 0.7F, fh * 0.75F);
    painter.setBrush(QColor(255, 255, 180));
    draw_flame(0.0F, s * 0.50F, fw, fh);

  } else if (type == QStringLiteral("tent")) {
    painter.setBrush(QColor(176, 126, 78));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float th = s * 0.75F;
    const float tw = s * 0.85F;
    QPolygonF tent;
    tent << QPointF(0, -th) << QPointF(tw, th * 0.55F) << QPointF(-tw, th * 0.55F);
    painter.setBrush(QColor(210, 165, 100));
    painter.drawPolygon(tent);

    painter.setPen(QPen(QColor(120, 85, 50), std::max(1.0F, s * 0.1F)));
    painter.drawLine(QPointF(0, 0), QPointF(0, th * 0.55F));

  } else if (type == QStringLiteral("supply_cart")) {
    painter.setBrush(QColor(140, 106, 64));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float bw = s * 0.75F;
    const float bh = s * 0.45F;
    painter.setBrush(QColor(180, 140, 90));
    painter.setPen(QPen(QColor(100, 72, 40), std::max(1.0F, s * 0.1F)));
    painter.drawRect(QRectF(-bw, -bh * 0.6F, bw * 2, bh));
    const float wr = s * 0.22F;
    painter.setBrush(QColor(80, 60, 35));
    painter.drawEllipse(QPointF(-bw * 0.6F, bh * 0.55F), wr, wr);
    painter.drawEllipse(QPointF(bw * 0.6F, bh * 0.55F), wr, wr);

  } else if (type == QStringLiteral("weapon_rack")) {
    painter.setBrush(QColor(120, 120, 132));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float arm = s * 0.70F;
    QPen const cross_pen(
        QColor(210, 210, 220), std::max(2.0F, s * 0.18F), Qt::SolidLine, Qt::RoundCap);
    painter.setPen(cross_pen);
    painter.drawLine(QPointF(-arm, -arm), QPointF(arm, arm));
    painter.drawLine(QPointF(arm, -arm), QPointF(-arm, arm));

  } else if (type == QStringLiteral("ruins")) {
    painter.setBrush(QColor(102, 98, 90));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float rs = s * 0.65F;
    QPen const rp(
        QColor(170, 165, 150), std::max(1.5F, s * 0.12F), Qt::SolidLine, Qt::SquareCap);
    painter.setPen(rp);

    const float gap = rs * 0.35F;
    painter.drawLine(QPointF(-rs, -rs), QPointF(-rs + gap, -rs));
    painter.drawLine(QPointF(-rs, -rs), QPointF(-rs, -rs + gap));
    painter.drawLine(QPointF(rs - gap, -rs), QPointF(rs, -rs));
    painter.drawLine(QPointF(rs, -rs), QPointF(rs, -rs + gap));
    painter.drawLine(QPointF(-rs, rs - gap), QPointF(-rs, rs));
    painter.drawLine(QPointF(-rs, rs), QPointF(-rs + gap, rs));
    painter.drawLine(QPointF(rs, rs - gap), QPointF(rs, rs));
    painter.drawLine(QPointF(rs, rs), QPointF(rs - gap, rs));

  } else if (type == QStringLiteral("magic_shrine")) {
    painter.setBrush(QColor(118, 96, 196));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    const float sr = s * 0.78F;
    const int points = 6;
    const float inner_r = sr * 0.45F;
    QPolygonF star;
    for (int k = 0; k < points * 2; ++k) {
      const float angle =
          static_cast<float>(k) * M_PI / static_cast<float>(points) - M_PI_2;
      const float r = (k % 2 == 0) ? sr : inner_r;
      star << QPointF(r * std::cos(angle), r * std::sin(angle));
    }
    painter.setBrush(QColor(210, 180, 255));
    painter.drawPolygon(star);
    painter.setBrush(QColor(255, 240, 255));
    painter.drawEllipse(QPointF(0, 0), sr * 0.18F, sr * 0.18F);

  } else if (type == QStringLiteral("dead_tree")) {
    painter.setBrush(QColor(111, 86, 67));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPen const tp(
        QColor(200, 170, 135), std::max(1.5F, s * 0.14F), Qt::SolidLine, Qt::RoundCap);
    painter.setPen(tp);

    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.15F));

    painter.drawLine(QPointF(0, -s * 0.15F), QPointF(-s * 0.55F, -s * 0.65F));
    painter.drawLine(QPointF(0, -s * 0.15F), QPointF(s * 0.55F, -s * 0.65F));
    painter.drawLine(QPointF(0, s * 0.20F), QPointF(-s * 0.42F, -s * 0.22F));
    painter.drawLine(QPointF(0, s * 0.20F), QPointF(s * 0.42F, -s * 0.22F));

  } else if (type == QStringLiteral("boulder")) {
    painter.setBrush(QColor(110, 110, 110));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);

    QPolygonF rock;
    rock << QPointF(0, -s * 0.72F) << QPointF(s * 0.58F, -s * 0.40F)
         << QPointF(s * 0.70F, s * 0.20F) << QPointF(s * 0.30F, s * 0.68F)
         << QPointF(-s * 0.38F, s * 0.68F) << QPointF(-s * 0.70F, s * 0.18F)
         << QPointF(-s * 0.55F, -s * 0.42F);
    painter.setBrush(QColor(175, 175, 175));
    painter.drawPolygon(rock);

    QPen const hp(QColor(215, 215, 215), std::max(1.0F, s * 0.10F));
    painter.setPen(hp);
    painter.drawLine(QPointF(-s * 0.30F, -s * 0.55F), QPointF(s * 0.40F, -s * 0.35F));
  } else if (type == QStringLiteral("pine_tree") ||
             type == QStringLiteral("olive_tree")) {
    const QColor crown = type == QStringLiteral("pine_tree") ? QColor(38, 110, 70)
                                                             : QColor(105, 126, 62);
    painter.setBrush(crown.darker(145));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    painter.setPen(QPen(QColor(105, 72, 42), std::max(1.5F, s * 0.16F)));
    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.15F));
    painter.setBrush(crown);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, -s * 0.25F), s * 0.62F, s * 0.62F);
  } else if (type == QStringLiteral("plant")) {
    painter.setBrush(QColor(54, 122, 58));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    painter.setPen(QPen(QColor(150, 205, 92), std::max(1.5F, s * 0.14F)));
    painter.drawLine(QPointF(0, s * 0.55F), QPointF(0, -s * 0.55F));
    painter.drawLine(QPointF(0, 0), QPointF(-s * 0.5F, -s * 0.25F));
    painter.drawLine(QPointF(0, s * 0.15F), QPointF(s * 0.5F, -s * 0.15F));
  } else if (type == QStringLiteral("iron_ore")) {
    painter.setBrush(QColor(68, 78, 88));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, 0), s, s);
    QPolygonF ore;
    ore << QPointF(0, -s * 0.75F) << QPointF(s * 0.65F, -s * 0.15F)
        << QPointF(s * 0.35F, s * 0.7F) << QPointF(-s * 0.55F, s * 0.55F)
        << QPointF(-s * 0.7F, -s * 0.2F);
    painter.setBrush(QColor(150, 164, 176));
    painter.drawPolygon(ore);
  }

  painter.restore();
}

QSizeF MapCanvas::terrain_ellipse_px(const TerrainElement& elem) const {
  float rx_cells = NAN;
  float ry_cells = NAN;

  if (elem.type == QStringLiteral("mountain")) {

    const float r = std::max(elem.radius, 1.0F);
    rx_cells = r * 1.8F;
    ry_cells = r * 0.22F;
  } else if (elem.type == QStringLiteral("lake")) {
    rx_cells = elem.width > 0.0F ? elem.width * 0.5F : std::max(elem.radius, 1.0F);
    ry_cells = elem.depth > 0.0F ? elem.depth * 0.5F : std::max(elem.radius, 1.0F);
  } else {

    if (elem.width > 0.0F && elem.depth > 0.0F) {
      rx_cells = elem.width;
      ry_cells = elem.depth;
    } else {
      rx_cells = ry_cells = std::max(elem.radius, 1.0F);
    }
  }

  const float rx = rx_cells * static_cast<float>(grid_cell_size) * m_zoom;
  const float ry = ry_cells * static_cast<float>(grid_cell_size) * m_zoom;

  return {std::max(static_cast<float>(marker_radius_px()), rx), std::max(4.0F, ry)};
}

void MapCanvas::draw_terrain_feature(QPainter& painter,
                                     const TerrainElement& elem,
                                     const QPoint& center) {
  const QSizeF ellipse = terrain_ellipse_px(elem);
  const auto rx = ellipse.width();
  const auto ry = ellipse.height();

  painter.save();
  painter.translate(center);
  painter.rotate(static_cast<double>(elem.rotation));

  if (elem.type == QStringLiteral("hill")) {

    const QColor outer(168, 148, 102);
    const QColor mid(144, 122, 78);
    const QColor inner(120, 98, 58);
    const QColor peak(96, 74, 42);

    painter.setPen(Qt::NoPen);
    painter.setBrush(outer);
    painter.drawEllipse(QPointF(0, 0), rx, ry);

    painter.setBrush(mid);
    painter.drawEllipse(QPointF(0, 0), rx * 0.72, ry * 0.72);

    painter.setBrush(inner);
    painter.drawEllipse(QPointF(0, 0), rx * 0.45, ry * 0.45);

    painter.setBrush(peak);
    const double dot_r = std::max(2.5, std::min(rx, ry) * 0.18);
    painter.drawEllipse(QPointF(0, 0), dot_r, dot_r);

  } else if (elem.type == QStringLiteral("mountain")) {

    const QColor outer(148, 148, 162);
    const QColor mid(115, 115, 130);
    const QColor inner(85, 85, 100);
    const QColor peak(230, 235, 245);

    painter.setPen(Qt::NoPen);
    painter.setBrush(outer);
    painter.drawEllipse(QPointF(0, 0), rx, ry);

    painter.setBrush(mid);
    painter.drawEllipse(QPointF(0, 0), rx * 0.72, ry * 0.72);

    painter.setBrush(inner);
    painter.drawEllipse(QPointF(0, 0), rx * 0.45, ry * 0.45);

    painter.setBrush(peak);
    const double dot_r = std::max(2.5, std::min(rx, ry) * 0.18);
    painter.drawEllipse(QPointF(0, 0), dot_r, dot_r);
  } else if (elem.type == QStringLiteral("lake")) {
    painter.setPen(QPen(QColor(35, 80, 105), 2.0));
    painter.setBrush(QColor(55, 135, 165, 205));
    painter.drawEllipse(QPointF(0, 0), rx, ry);
    painter.setPen(QPen(QColor(145, 205, 215, 175), 1.0));
    painter.drawEllipse(QPointF(0, 0), rx * 0.82, ry * 0.82);
  }

  painter.restore();
}

int MapCanvas::terrain_marker_radius_px(const TerrainElement& elem) const {
  if (elem.type != QStringLiteral("hill") && elem.type != QStringLiteral("mountain") &&
      elem.type != QStringLiteral("lake")) {
    return marker_radius_px();
  }
  const QSizeF e = terrain_ellipse_px(elem);
  return std::max(marker_radius_px(),
                  static_cast<int>(std::round(std::max(e.width(), e.height()))));
}

float MapCanvas::terrain_hit_radius_px(const TerrainElement& elem) const {
  return static_cast<float>(terrain_marker_radius_px(elem)) + 4.0F;
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
  m_last_mouse_pos = event->pos();

  if (event->button() == Qt::RightButton) {
    // While drawing a river/road/wall, right-click still means "cancel"; anywhere
    // else it opens the context menu.
    if (m_is_placing_linear) {
      m_is_placing_linear = false;
      emit status_hint_changed("");
      update();
      return;
    }
    show_context_menu(event->pos());
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
          // Shift on empty canvas starts a rubber band; a plain drag still pans.
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
        // Clicking an element outside the current selection starts a fresh one;
        // clicking one inside it keeps the group so it can be dragged together.
        set_selection(hit_ref.kind, hit_ref.index);
      } else {
        // Make the clicked element the primary one without dropping the group.
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
    case ToolType::Hill:
    case ToolType::Mountain:
    case ToolType::PropFirecamp:
    case ToolType::PropTent:
    case ToolType::PropSupplyCart:
    case ToolType::PropWeaponRack:
    case ToolType::PropRuins:
    case ToolType::PropMagicShrine:
    case ToolType::PropDeadTree:
    case ToolType::PropBoulder:
    case ToolType::Barracks:
    case ToolType::Village:
    case ToolType::TroopArcher:
    case ToolType::TroopSwordsman:
    case ToolType::TroopSpearman:
    case ToolType::TroopHorseSwordsman:
    case ToolType::TroopHorseArcher:
    case ToolType::TroopHorseSpearman:
    case ToolType::TroopHealer:
    case ToolType::TroopCatapult:
    case ToolType::TroopBallista:
    case ToolType::TroopElephant:
    case ToolType::TroopRomanLegionOrganizer:
    case ToolType::TroopRomanVeteranConsul:
    case ToolType::TroopRomanFieldCommander:
    case ToolType::TroopCarthageMercenaryBroker:
    case ToolType::TroopCarthageCavalryPatron:
    case ToolType::TroopCarthageElephantMaster:
    case ToolType::TroopSkeletonSwordsman:
    case ToolType::TroopSkeletonArcher:
    case ToolType::TroopGravePriest:
    case ToolType::TroopCivilian:
    case ToolType::TroopBuilder:
    case ToolType::UndeadZone:
    case ToolType::DefenseTower:
    case ToolType::Home:
      place_element(grid_pos);
      break;
    case ToolType::River:
    case ToolType::Road:
    case ToolType::Bridge:
    case ToolType::Wall:
      if (!m_is_placing_linear) {
        start_linear_element(grid_pos);
      } else {
        finish_linear_element(grid_pos);
      }
      break;
    case ToolType::Eraser:
      erase_at_position(grid_pos);
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
    // The elements were updated live during the drag; record the whole gesture as
    // a single undo entry now that it is finished.
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
      if (std::unique_ptr<Command> cmd = ElementOps::make_update_many(
              *m_map_data, refs, before, after, description)) {
        m_map_data->record_command(std::move(cmd));
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
      // Endpoint drags are the one case that is not a plain translation: walls stay
      // axis aligned relative to their opposite end.
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

  // The element under the cursor lands on the target; the rest of the group keeps
  // its relative layout by moving the same delta.
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
        // A segment counts as picked when either end is inside the band.
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
    return; // treat a stray click as "no band"
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

void MapCanvas::draw_rubber_band(QPainter& painter) {
  if (!m_band_active) {
    return;
  }

  painter.save();
  painter.setPen(QPen(k_hover_select_color, 1, Qt::DashLine));
  painter.setBrush(QColor(100, 200, 255, 40));
  painter.drawRect(QRect(m_band_origin, m_band_current).normalized());
  painter.restore();
}

void MapCanvas::show_context_menu(const QPoint& pos) {
  if (m_map_data == nullptr) {
    return;
  }

  const HitResult hit = hit_test(pos);
  // Match what a left click would do at the same spot.
  const QPointF grid_pos = clamp_to_grid(snap_pos(map_to_grid(pos)));
  const QPoint global_pos = mapToGlobal(pos);

  QMenu menu(this);

  if (hit.element_type >= 0 && hit.index >= 0) {
    // Right-clicking outside the current selection starts a new one; inside it,
    // the whole group stays selected so the menu acts on all of it.
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

    // Draw order is a view aid only: it is never written to the map file.
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

    if (m_current_tool != ToolType::Select) {
      menu.addSeparator();
      connect(menu.addAction(QStringLiteral("Clear tool")),
              &QAction::triggered,
              this,
              &MapCanvas::clear_tool);
    }
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
  if (event->button() == Qt::LeftButton) {
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
  // First real layout: frame the whole map instead of parking the view in a
  // corner, which on large maps used to look like an empty canvas.
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
    // Hidden layers are not pickable, otherwise invisible elements would still
    // swallow clicks and Delete.
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

  const auto& structures = m_map_data->structures();
  for (int i = structures.size() - 1; i >= 0; --i) {
    const auto& elem = structures[i];
    const QPoint center = grid_to_widget(elem.x, elem.z);
    const QVector2D center_vec(static_cast<float>(center.x()),
                               static_cast<float>(center.y()));
    consider_hit(3, i, -1, (cursor - center_vec).length(), point_hit_radius_px, 1);
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
    const QPoint start_pos = grid_to_widget(elem.start.x(), elem.start.y());
    const QPoint end_pos = grid_to_widget(elem.end.x(), elem.end.y());

    const QVector2D a(static_cast<float>(start_pos.x()),
                      static_cast<float>(start_pos.y()));
    const QVector2D b(static_cast<float>(end_pos.x()), static_cast<float>(end_pos.y()));
    const QVector2D ab = b - a;
    const float ab_length_sq = QVector2D::dotProduct(ab, ab);

    float dist = std::numeric_limits<float>::infinity();
    if (ab_length_sq < 0.0001F) {
      dist = (cursor - a).length();
    } else {
      const float t =
          std::clamp(QVector2D::dotProduct(cursor - a, ab) / ab_length_sq, 0.0F, 1.0F);
      const QVector2D closest = a + t * ab;
      dist = (cursor - closest).length();
    }

    int line_width_px = static_cast<int>(elem.width * m_zoom);
    line_width_px = std::max(2, std::min(line_width_px, 20));
    const float line_hit_radius_px = static_cast<float>(line_width_px) * 0.5F + 4.0F;
    if ((cursor - a).length() <= point_hit_radius_px ||
        (cursor - b).length() <= point_hit_radius_px) {
      continue;
    }
    consider_hit(2, i, -1, dist, line_hit_radius_px, 5);
  }

  return result;
}

void MapCanvas::place_element(const QPointF& raw_grid_pos) {
  if (m_map_data == nullptr) {
    return;
  }

  // Elements outside the grid are silently invalid at load time, so keep every
  // placement inside the map bounds.
  const QPointF grid_pos = clamp_to_grid(raw_grid_pos);

  if (m_current_tool == ToolType::Hill || m_current_tool == ToolType::Mountain) {
    TerrainElement elem;
    elem.type = (m_current_tool == ToolType::Hill) ? "hill" : "mountain";
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.radius = 10.0F;
    elem.height = (m_current_tool == ToolType::Hill) ? 3.0F : 8.0F;
    m_map_data->execute_command(std::make_unique<AddTerrainCmd>(m_map_data, elem));
  } else if (!world_prop_type_for_tool(m_current_tool).isEmpty()) {
    WorldPropElement elem;
    elem.type = world_prop_type_for_tool(m_current_tool);
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    if (elem.type == QStringLiteral("firecamp")) {
      elem.intensity = 1.0F;
      elem.radius = 3.0F;
    } else {
      elem.scale = 1.0F;
      elem.rotation = 0.0F;
    }
    m_map_data->execute_command(std::make_unique<AddWorldPropCmd>(m_map_data, elem));
  } else if (m_current_tool == ToolType::Barracks ||
             m_current_tool == ToolType::Village ||
             m_current_tool == ToolType::DefenseTower ||
             m_current_tool == ToolType::Home ||
             m_current_tool == ToolType::Marketplace) {
    StructureElement elem;
    switch (m_current_tool) {
    case ToolType::Barracks:
      elem.type = "barracks";
      break;
    case ToolType::Village:
      elem.type = "village";
      break;
    case ToolType::DefenseTower:
      elem.type = "defense_tower";
      break;
    case ToolType::Home:
      elem.type = "home";
      break;
    case ToolType::Marketplace:
      elem.type = "marketplace";
      break;
    default:
      break;
    }
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.player_id = m_current_player_id;
    elem.max_population = default_max_population;
    elem.nation = m_current_nation;
    m_map_data->execute_command(std::make_unique<AddStructureCmd>(m_map_data, elem));
  } else if (is_troop_tool(m_current_tool)) {
    TroopSpawnElement elem;
    elem.type = troop_type_for_tool(m_current_tool);
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
    elem.player_id = m_current_player_id;
    elem.max_population = default_troop_max_population;
    elem.nation = m_current_nation;
    m_map_data->execute_command(std::make_unique<AddTroopSpawnCmd>(m_map_data, elem));
  } else if (m_current_tool == ToolType::UndeadZone) {
    UndeadZoneElement elem;
    static int zone_counter = 0;
    elem.id = QStringLiteral("zone_%1").arg(++zone_counter);
    elem.anchor_type = QStringLiteral("magic_shrine");
    elem.x = static_cast<float>(grid_pos.x());
    elem.z = static_cast<float>(grid_pos.y());
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
  }
}

void MapCanvas::start_linear_element(const QPointF& grid_pos) {
  m_is_placing_linear = true;
  m_linear_start = clamp_to_grid(grid_pos);

  QString type_name;
  if (m_current_tool == ToolType::River) {
    type_name = "river";
  } else if (m_current_tool == ToolType::Road) {
    type_name = "road";
  } else if (m_current_tool == ToolType::Bridge) {
    type_name = "bridge";
  } else if (m_current_tool == ToolType::Wall) {
    type_name = "wall";
  }
  emit status_hint_changed("Drawing " + type_name +
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
  case ToolType::Wall:
    elem.type = "wall";
    elem.width = 2.0F;
    elem.player_id = m_current_player_id;
    elem.nation = m_current_nation;
    elem.end = axis_aligned_endpoint(elem.start, elem.end);
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
    // Grid x/z run opposite to screen x/y because the canvas mirrors the map to
    // match the in-game camera, so the arrow deltas are negated.
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
