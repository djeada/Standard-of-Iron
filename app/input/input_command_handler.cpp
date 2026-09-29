#include "app/input/input_command_handler.h"

#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/orders/command_controller.h"
#include "app/orders/movement_utils.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/navigation/nav_grid.h"
#include "scene/camera.h"

InputCommandHandler::InputCommandHandler(
    Engine::Core::World* world,
    Game::Systems::SelectionController* selection_controller,
    App::Controllers::CommandController* command_controller,
    CursorManager* cursor_manager,
    HoverTracker* hover_tracker,
    Game::Systems::PickingService* picking_service,
    Render::GL::Camera* camera)
    : m_world(world)
    , m_selection_controller(selection_controller)
    , m_command_controller(command_controller)
    , m_cursor_manager(cursor_manager)
    , m_hover_tracker(hover_tracker)
    , m_picking_service(picking_service)
    , m_camera(camera)
    , m_context(world, camera, picking_service, hover_tracker)
    , m_formation(command_controller != nullptr ? &command_controller->formation()
                                                : nullptr,
                  camera,
                  picking_service) {
}

namespace {

auto right_click_cancels_mode(CursorMode mode) -> bool {
  switch (mode) {
  case CursorMode::Attack:
  case CursorMode::Guard:
  case CursorMode::Patrol:
  case CursorMode::Build:
  case CursorMode::PlaceBuilding:
  case CursorMode::Deliver:
  case CursorMode::Heal:
  case CursorMode::PlaceCommanderRally:
  case CursorMode::PlaceBarracksRally:
  case CursorMode::Collect:
  case CursorMode::Repair:
  case CursorMode::Dismantle:
    return true;
  case CursorMode::Normal:
    break;
  }
  return false;
}

constexpr float k_max_click_nudge = 3.0F;

} // namespace

void InputCommandHandler::reset_order_modes() {
  if (m_cursor_manager != nullptr) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
  if (m_command_controller != nullptr) {
    m_command_controller->clear_patrol_first_waypoint();
  }
}

auto InputCommandHandler::cursor_cancels_order_mode() const -> bool {
  return right_click_cancels_mode(m_cursor_manager->mode());
}

auto InputCommandHandler::selection() const -> Game::Session::SelectionService& {
  return Game::Session::session_for(*m_world).selection();
}

template <class Issue>
void InputCommandHandler::issue_command(Issue&& issue) {
  if (m_is_spectator_mode || m_command_controller == nullptr) {
    return;
  }
  if (issue(*m_command_controller).reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

template <class Issue>
void InputCommandHandler::issue_camera_command(Issue&& issue) {
  if (m_is_spectator_mode || m_command_controller == nullptr || m_camera == nullptr) {
    return;
  }
  if (issue(*m_command_controller).reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_map_clicked(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) {
  on_click_select(sx, sy, false, local_owner_id, viewport);
}

void InputCommandHandler::on_right_click(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  if (cursor_cancels_order_mode()) {
    reset_order_modes();
    return;
  }

  if (selection().get_selected_units().empty() || (m_command_controller == nullptr) ||
      (m_camera == nullptr)) {
    return;
  }

  Engine::Core::EntityID const enemy_id = App::Utils::pick_enemy_unit_at_screen(
      m_world, m_camera, sx, sy, viewport.width, viewport.height, local_owner_id);
  if (enemy_id == 0 && issue_context_interaction(sx, sy, local_owner_id, viewport)) {
    return;
  }

  m_command_controller->disable_run_mode_for_selected();
  (void)m_command_controller->on_move_or_attack_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  m_command_controller->disable_run_mode_for_selected();
  reset_order_modes();
}

auto InputCommandHandler::issue_context_interaction(
    qreal sx, qreal sy, int local_owner_id, const ViewportState& viewport) -> bool {
  auto const interaction = m_context.resolve(sx, sy, viewport);
  if (interaction.is_gather()) {
    m_command_controller->disable_run_mode_for_selected();
    (void)m_command_controller->set_auto_gather(true, interaction.gather_product_type);
    return true;
  }
  if (interaction.is_food_task()) {
    m_command_controller->disable_run_mode_for_selected();
    (void)m_command_controller->start_food_harvest(
        interaction.target, interaction.food_product_type, local_owner_id);
    return true;
  }
  if (interaction.is_repair()) {
    on_builder_repair_click(sx, sy, local_owner_id, viewport);
    return true;
  }
  return false;
}

auto InputCommandHandler::resolve_context_interaction(
    qreal sx,
    qreal sy,
    const ViewportState& viewport,
    QString& out_product_type,
    Engine::Core::EntityID& out_target) const -> bool {
  auto const interaction = m_context.resolve(sx, sy, viewport);
  out_product_type = interaction.gather_product_type;
  out_target = interaction.target;
  return interaction.is_gather() || interaction.is_food_task() ||
         interaction.is_repair();
}

auto InputCommandHandler::resolve_context_interaction(
    qreal sx, qreal sy, const ViewportState& viewport) const -> ContextInteraction {
  return m_context.resolve(sx, sy, viewport);
}

void InputCommandHandler::on_minimap_right_click(const QVector3D& world_target,
                                                 int local_owner_id) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  if (selection().get_selected_units().empty() || (m_command_controller == nullptr)) {
    return;
  }

  (void)m_command_controller->on_minimap_move(world_target, local_owner_id);
  reset_order_modes();
}

void InputCommandHandler::on_right_double_click(qreal sx,
                                                qreal sy,
                                                int local_owner_id,
                                                const ViewportState& viewport) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  if (m_formation.is_placing()) {
    return;
  }

  if (cursor_cancels_order_mode()) {
    reset_order_modes();
    return;
  }

  if (selection().get_selected_units().empty() || (m_command_controller == nullptr) ||
      (m_camera == nullptr)) {
    return;
  }

  m_command_controller->enable_run_mode_for_selected();
  (void)m_command_controller->on_move_or_attack_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  m_command_controller->enable_run_mode_for_selected();
  reset_order_modes();
}

auto InputCommandHandler::on_right_press(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) -> bool {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return false;
  }

  if (cursor_cancels_order_mode()) {
    reset_order_modes();
    return true;
  }

  if (selection().get_selected_units().empty()) {
    return false;
  }

  if ((m_command_controller == nullptr) || (m_camera == nullptr) ||
      (m_picking_service == nullptr)) {
    return false;
  }

  if (m_command_controller
          ->on_attack_press(
              sx, sy, viewport.width, viewport.height, m_camera, local_owner_id)
          .input_consumed) {
    return true;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return false;
  }

  const QVector3D clicked = hit;
  hit = App::Utils::snap_to_walkable_ground(hit);
  if (!Game::Systems::NavGrid::is_world_position_walkable(clicked) &&
      (!Game::Systems::NavGrid::is_world_position_walkable(hit) ||
       (hit - clicked).length() > k_max_click_nudge)) {
    (void)m_command_controller->refuse_unreachable_move(clicked);
    return true;
  }
  if (m_formation.begin_move_placement(hit)) {
    m_command_controller->disable_run_mode_for_selected();
    return true;
  }
  return false;
}

void InputCommandHandler::on_right_drag_orient(qreal sx,
                                               qreal sy,
                                               const ViewportState& viewport) {
  if (m_command_controller == nullptr) {
    return;
  }
  m_formation.on_right_drag_orient(sx, sy, viewport);
}

void InputCommandHandler::on_attack_click(qreal sx,
                                          qreal sy,
                                          int local_owner_id,
                                          const ViewportState& viewport) {
  issue_camera_command([&](App::Controllers::CommandController& commands) {
    return commands.on_attack_click(
        sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  });
}

void InputCommandHandler::on_stop_command() {
  issue_command([](auto& commands) { return commands.on_stop_command(); });
}

void InputCommandHandler::on_hold_command() {
  issue_command([](auto& commands) { return commands.on_hold_command(); });
}

void InputCommandHandler::on_gate_command() {
  issue_command([](auto& commands) { return commands.on_gate_command(); });
}

void InputCommandHandler::on_guard_command() {
  issue_command([](auto& commands) { return commands.on_guard_command(); });
}

void InputCommandHandler::on_formation_command() {
  issue_command(
      [](auto& commands) { return commands.formation().on_formation_command(); });
}

void InputCommandHandler::on_auto_gather_command(const QString& priority_product_type) {
  issue_command([&](auto& commands) {
    return commands.on_auto_gather_command(priority_product_type);
  });
}

void InputCommandHandler::divide_selected_squads() {
  if (m_is_spectator_mode || m_command_controller == nullptr) {
    return;
  }
  static_cast<void>(m_command_controller->divide_selected_squads());
}

void InputCommandHandler::merge_selected_squads() {
  if (m_is_spectator_mode || m_command_controller == nullptr) {
    return;
  }
  static_cast<void>(m_command_controller->merge_selected_squads());
}

void InputCommandHandler::set_auto_gather(bool active,
                                          const QString& priority_product_type) {
  issue_command([&](auto& commands) {
    return commands.set_auto_gather(active, priority_product_type);
  });
}

void InputCommandHandler::on_run_command() {
  issue_command([](auto& commands) { return commands.on_run_command(); });
}

void InputCommandHandler::on_guard_click(qreal sx,
                                         qreal sy,
                                         const ViewportState& viewport) {
  issue_camera_command([&](auto& commands) {
    return commands.on_guard_click(sx, sy, viewport.width, viewport.height, m_camera);
  });
}

void InputCommandHandler::on_civilian_delivery_click(qreal sx,
                                                     qreal sy,
                                                     int local_owner_id,
                                                     const ViewportState& viewport) {
  issue_camera_command([&](auto& commands) {
    return commands.on_civilian_delivery_click(
        sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  });
}

void InputCommandHandler::on_builder_repair_click(qreal sx,
                                                  qreal sy,
                                                  int local_owner_id,
                                                  const ViewportState& viewport) {
  issue_camera_command([&](auto& commands) {
    return commands.on_builder_repair_click(
        sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  });
}

void InputCommandHandler::on_builder_dismantle_click(qreal sx,
                                                     qreal sy,
                                                     int local_owner_id,
                                                     const ViewportState& viewport) {
  issue_camera_command([&](auto& commands) {
    return commands.on_builder_dismantle_click(
        sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  });
}

void InputCommandHandler::on_patrol_click(qreal sx,
                                          qreal sy,
                                          const ViewportState& viewport) {
  issue_camera_command([&](auto& commands) {
    return commands.on_patrol_click(sx, sy, viewport.width, viewport.height, m_camera);
  });
}

auto InputCommandHandler::any_selected_in_hold_mode() const -> bool {
  return m_command_controller != nullptr &&
         m_command_controller->any_selected_in_hold_mode();
}

auto InputCommandHandler::any_selected_in_guard_mode() const -> bool {
  return m_command_controller != nullptr &&
         m_command_controller->any_selected_in_guard_mode();
}

auto InputCommandHandler::any_selected_in_formation_mode() const -> bool {
  return m_formation.any_selected_in_formation_mode();
}

auto InputCommandHandler::any_selected_in_run_mode() const -> bool {
  return m_command_controller != nullptr &&
         m_command_controller->any_selected_in_run_mode();
}

auto InputCommandHandler::is_placing_formation() const -> bool {
  return m_formation.is_placing();
}

auto InputCommandHandler::is_dragging_formation() const -> bool {
  return m_formation.is_dragging();
}

void InputCommandHandler::on_formation_mouse_move(qreal sx,
                                                  qreal sy,
                                                  const ViewportState& viewport) {
  m_formation.on_mouse_move(sx, sy, viewport);
}

void InputCommandHandler::on_formation_drag_begin(qreal sx,
                                                  qreal sy,
                                                  const ViewportState& viewport) {
  m_formation.on_drag_begin(sx, sy, viewport);
}

void InputCommandHandler::on_formation_drag_update(qreal sx,
                                                   qreal sy,
                                                   const ViewportState& viewport) {
  m_formation.on_mouse_move(sx, sy, viewport);
}

void InputCommandHandler::on_formation_drag_end() {
  m_formation.on_drag_end();
}

void InputCommandHandler::on_formation_scroll(float delta) {
  m_formation.on_scroll(delta);
}

void InputCommandHandler::on_formation_confirm() {
  if (m_command_controller == nullptr) {
    return;
  }
  m_formation.confirm();
  reset_order_modes();
}

void InputCommandHandler::on_formation_cancel() {
  m_formation.cancel();
}

void InputCommandHandler::on_click_select(qreal sx,
                                          qreal sy,
                                          bool additive,
                                          int local_owner_id,
                                          const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_selection_controller != nullptr) && (m_camera != nullptr)) {
    m_selection_controller->on_click_select(
        sx, sy, additive, viewport.width, viewport.height, m_camera, local_owner_id);
  }
}

void InputCommandHandler::on_area_selected(qreal x1,
                                           qreal y1,
                                           qreal x2,
                                           qreal y2,
                                           bool additive,
                                           int local_owner_id,
                                           const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_selection_controller != nullptr) && (m_camera != nullptr)) {
    m_selection_controller->on_area_selected(x1,
                                             y1,
                                             x2,
                                             y2,
                                             additive,
                                             viewport.width,
                                             viewport.height,
                                             m_camera,
                                             local_owner_id);
  }
}

void InputCommandHandler::select_all_troops(int local_owner_id) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller != nullptr) {
    m_selection_controller->select_all_player_troops(local_owner_id);
  }
}

void InputCommandHandler::select_unit_by_id(Engine::Core::EntityID unit_id,
                                            int local_owner_id) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller == nullptr || unit_id == Engine::Core::NULL_ENTITY) {
    return;
  }
  m_selection_controller->select_single_unit(unit_id, local_owner_id);
}

void InputCommandHandler::select_selected_units_by_type(const QString& unit_type,
                                                        int local_owner_id) {
  if (m_is_spectator_mode || m_selection_controller == nullptr) {
    return;
  }
  m_selection_controller->select_selected_units_by_type(unit_type, local_owner_id);
}

void InputCommandHandler::set_hover_at_screen(qreal sx,
                                              qreal sy,
                                              const ViewportState& viewport) {
  if ((m_hover_tracker == nullptr) || (m_camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  m_hover_tracker->update_hover(
      float(sx), float(sy), *m_world, *m_camera, viewport.width, viewport.height);
}
