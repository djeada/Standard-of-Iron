#include "app/viewmodels/orders_view_model.h"

#include "app/core/client_context.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/input_command_handler.h"
#include "app/orders/command_controller.h"
#include "app/orders/rts_action_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/placement_view_model.h"

namespace App::ViewModels {
namespace {

constexpr double k_drag_threshold_squared = 36.0;

} // namespace

OrdersViewModel::OrdersViewModel(const App::Core::ClientContext& context,
                                 App::Core::ClientHost& host,
                                 PlacementViewModel& placement,
                                 CommanderViewModel& commander,
                                 QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host)
    , m_placement(placement)
    , m_commander(commander) {
}

void OrdersViewModel::on_click_select(qreal sx, qreal sy, bool additive) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->on_click_select(
        sx, sy, additive, m_context.local_owner_id, *m_context.viewport);
  }
}

void OrdersViewModel::on_area_selected(
    qreal x1, qreal y1, qreal x2, qreal y2, bool additive) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->on_area_selected(
        x1, y1, x2, y2, additive, m_context.local_owner_id, *m_context.viewport);
  }
}

void OrdersViewModel::select_all_troops() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->select_all_troops(m_context.local_owner_id);
  }
}

void OrdersViewModel::select_unit_by_id(qulonglong unit_id) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->select_unit_by_id(static_cast<Engine::Core::EntityID>(unit_id),
                                       m_context.local_owner_id);
  }
}

void OrdersViewModel::select_by_type(const QString& unit_type) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->select_selected_units_by_type(unit_type, m_context.local_owner_id);
  }
}

void OrdersViewModel::set_hover_at_screen(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->set_hover_at_screen(sx, sy, *m_context.viewport);
  }

  m_commander.update_rally_preview_at(sx, sy);
}

void OrdersViewModel::on_map_clicked(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->on_map_clicked(
        sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
}

void OrdersViewModel::on_right_click(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->on_right_click(
        sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
}

void OrdersViewModel::on_right_double_click(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();

  if (m_right_mouse.placement_was_active_on_press) {
    m_right_mouse.double_click_handled = true;
    return;
  }

  auto* input = m_context.input;
  const bool started_formation_placement = m_right_mouse.started_formation_placement &&
                                           input != nullptr &&
                                           input->is_placing_formation();
  if (started_formation_placement) {
    input->on_formation_cancel();
  } else if (m_right_mouse.suppress_release_click ||
             m_placement.is_placing_construction()) {
    m_right_mouse.double_click_handled = true;
    return;
  }

  if (input != nullptr) {
    input->on_right_double_click(sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
  m_right_mouse.double_click_handled = true;
}

auto OrdersViewModel::on_right_press(qreal sx, qreal sy) -> bool {
  if (m_context.window == nullptr) {
    return false;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_right_mouse.reset();
  m_right_mouse.active = true;
  m_right_mouse.press_position = QPointF(sx, sy);
  m_right_mouse.placement_was_active_on_press =
      m_placement.is_placing_formation() || m_placement.is_placing_construction();

  if (m_placement.is_placing_formation()) {
    m_placement.on_formation_cancel();
    m_right_mouse.suppress_release_click = true;
    return true;
  }
  if (m_placement.is_placing_construction()) {
    m_placement.on_construction_cancel();
    m_right_mouse.suppress_release_click = true;
    return true;
  }

  auto* input = m_context.input;
  if (input == nullptr) {
    return false;
  }
  m_right_mouse.suppress_release_click =
      input->on_right_press(sx, sy, m_context.local_owner_id, *m_context.viewport);
  m_right_mouse.started_formation_placement =
      !m_right_mouse.placement_was_active_on_press &&
      m_right_mouse.suppress_release_click && input->is_placing_formation();
  return m_right_mouse.suppress_release_click;
}

void OrdersViewModel::on_right_move(qreal sx, qreal sy) {
  if (m_context.window == nullptr || !m_right_mouse.active) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();

  const QPointF delta = QPointF(sx, sy) - m_right_mouse.press_position;
  if ((delta.x() * delta.x() + delta.y() * delta.y()) > k_drag_threshold_squared) {
    m_right_mouse.dragged = true;
  }

  auto* input = m_context.input;
  if (m_right_mouse.dragged && input != nullptr && input->is_placing_formation()) {
    input->on_right_drag_orient(sx, sy, *m_context.viewport);
  }
}

void OrdersViewModel::on_right_release(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    m_right_mouse.reset();
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();

  if (m_right_mouse.double_click_handled) {
    m_right_mouse.reset();
    return;
  }

  auto* input = m_context.input;
  if (input != nullptr && input->is_placing_formation()) {
    input->on_formation_confirm();
    m_right_mouse.reset();
    return;
  }

  if (!m_right_mouse.suppress_release_click && input != nullptr) {
    input->on_right_click(sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
  m_right_mouse.reset();
}

void OrdersViewModel::on_right_drag_orient(qreal sx, qreal sy) {
  on_right_move(sx, sy);
}

void OrdersViewModel::attack_at(qreal sx, qreal sy) {
  if (m_context.window == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.input != nullptr) {
    m_context.input->on_attack_click(sx, sy, *m_context.viewport);
  }
}

void OrdersViewModel::guard_at(qreal sx, qreal sy) {
  if (m_context.input == nullptr || m_context.active_camera == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_guard_click(sx, sy, *m_context.viewport);
}

void OrdersViewModel::patrol_at(qreal sx, qreal sy) {
  if (m_context.input == nullptr || m_context.active_camera == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_patrol_click(sx, sy, *m_context.viewport);
}

void OrdersViewModel::deliver_civilians_at(qreal sx, qreal sy) {
  if (m_context.input == nullptr || m_context.active_camera == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_civilian_delivery_click(
      sx, sy, m_context.local_owner_id, *m_context.viewport);
}

void OrdersViewModel::stop() {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_stop_command();
}

void OrdersViewModel::hold() {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_hold_command();
}

void OrdersViewModel::gate() {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_gate_command();
}

void OrdersViewModel::guard() {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_guard_command();
}

void OrdersViewModel::run() {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_run_command();
}

void OrdersViewModel::heal() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.cursor != nullptr && action_enabled(QStringLiteral("heal"))) {
    m_host.set_cursor_mode(CursorMode::Heal);
  }
}

void OrdersViewModel::build() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.cursor != nullptr && action_enabled(QStringLiteral("build"))) {
    m_host.set_cursor_mode(CursorMode::Build);
  }
}

namespace {

auto action_context(const App::Core::ClientContext& context)
    -> App::Core::ActionContext {
  App::Core::ActionContext out;
  out.world = context.world;
  out.cursor_mode =
      context.cursor != nullptr ? context.cursor->mode() : CursorMode::Normal;
  out.placing_construction =
      context.production != nullptr && context.production->is_placing_construction();
  out.pending_builder_construction_type =
      context.production != nullptr
          ? context.production->pending_builder_construction_type()
          : QString{};
  out.placing_formation = context.commands != nullptr &&
                          context.commands->formation().is_placing_formation();
  out.has_patrol_first_waypoint =
      context.commands != nullptr && context.commands->has_patrol_first_waypoint();
  return out;
}

} // namespace

auto OrdersViewModel::action_states() const -> QVariantMap {
  return App::Core::get_action_states(action_context(m_context));
}

auto OrdersViewModel::command_mode() const -> QString {
  return App::Core::get_current_action_mode(action_context(m_context));
}

auto OrdersViewModel::toggle_state(const QString& mode) const -> QString {
  return App::Core::get_toggle_state(m_context.world, mode);
}

auto OrdersViewModel::mode_availability() const -> QVariantMap {
  return App::Core::get_mode_availability(m_context.world);
}

auto OrdersViewModel::action_enabled(const QString& action_id) const -> bool {
  return action_states()
      .value(action_id)
      .toMap()
      .value(QStringLiteral("enabled"))
      .toBool();
}

} // namespace App::ViewModels
