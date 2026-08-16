#include "app/input/input_command_handler.h"

#include <cmath>

#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/orders/command_controller.h"
#include "app/orders/movement_utils.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "game/view/selection_controller.h"
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
    , m_camera(camera) {
}

void InputCommandHandler::on_map_clicked(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_selection_controller != nullptr) && (m_camera != nullptr)) {
    m_selection_controller->on_click_select(
        sx, sy, false, viewport.width, viewport.height, m_camera, local_owner_id);
  }
}

void InputCommandHandler::on_right_click(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  if (m_cursor_manager->mode() == CursorMode::Patrol ||
      m_cursor_manager->mode() == CursorMode::Attack ||
      m_cursor_manager->mode() == CursorMode::Guard ||
      m_cursor_manager->mode() == CursorMode::PlaceBuilding ||
      m_cursor_manager->mode() == CursorMode::Heal ||
      m_cursor_manager->mode() == CursorMode::Build ||
      m_cursor_manager->mode() == CursorMode::Collect ||
      m_cursor_manager->mode() == CursorMode::Deliver ||
      m_cursor_manager->mode() == CursorMode::PlaceCommanderRally ||
      m_cursor_manager->mode() == CursorMode::PlaceBarracksRally) {
    m_cursor_manager->set_mode(CursorMode::Normal);
    return;
  }

  const auto& sel = selection_system->get_selected_units();
  if (sel.empty() || (m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  m_command_controller->disable_run_mode_for_selected();
  (void)m_command_controller->on_move_or_attack_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  m_command_controller->disable_run_mode_for_selected();
}

void InputCommandHandler::on_minimap_right_click(const QVector3D& world_target,
                                                 int local_owner_id) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  const auto& selected = selection_system->get_selected_units();
  if (selected.empty() || (m_command_controller == nullptr)) {
    return;
  }

  (void)m_command_controller->on_minimap_move(world_target, local_owner_id);
}

void InputCommandHandler::on_right_double_click(qreal sx,
                                                qreal sy,
                                                int local_owner_id,
                                                const ViewportState& viewport) {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  if ((m_command_controller != nullptr) &&
      m_command_controller->formation().is_placing_formation()) {
    return;
  }

  if (m_cursor_manager->mode() == CursorMode::Patrol ||
      m_cursor_manager->mode() == CursorMode::Attack ||
      m_cursor_manager->mode() == CursorMode::Guard ||
      m_cursor_manager->mode() == CursorMode::PlaceBuilding ||
      m_cursor_manager->mode() == CursorMode::Heal ||
      m_cursor_manager->mode() == CursorMode::Build ||
      m_cursor_manager->mode() == CursorMode::Collect ||
      m_cursor_manager->mode() == CursorMode::Deliver ||
      m_cursor_manager->mode() == CursorMode::PlaceCommanderRally ||
      m_cursor_manager->mode() == CursorMode::PlaceBarracksRally) {
    m_cursor_manager->set_mode(CursorMode::Normal);
    return;
  }

  const auto& sel = selection_system->get_selected_units();
  if (sel.empty() || (m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  m_command_controller->enable_run_mode_for_selected();
  (void)m_command_controller->on_move_or_attack_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  m_command_controller->enable_run_mode_for_selected();
}

auto InputCommandHandler::on_right_press(qreal sx,
                                         qreal sy,
                                         int local_owner_id,
                                         const ViewportState& viewport) -> bool {
  if (m_is_spectator_mode || (m_world == nullptr)) {
    return false;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return false;
  }

  if (m_cursor_manager->mode() == CursorMode::Patrol ||
      m_cursor_manager->mode() == CursorMode::Attack ||
      m_cursor_manager->mode() == CursorMode::Guard ||
      m_cursor_manager->mode() == CursorMode::PlaceBuilding ||
      m_cursor_manager->mode() == CursorMode::Heal ||
      m_cursor_manager->mode() == CursorMode::Build ||
      m_cursor_manager->mode() == CursorMode::Collect ||
      m_cursor_manager->mode() == CursorMode::Deliver ||
      m_cursor_manager->mode() == CursorMode::PlaceCommanderRally ||
      m_cursor_manager->mode() == CursorMode::PlaceBarracksRally) {
    m_cursor_manager->set_mode(CursorMode::Normal);
    return true;
  }

  const auto& sel = selection_system->get_selected_units();
  if (sel.empty()) {
    return false;
  }

  if ((m_command_controller == nullptr) || (m_camera == nullptr) ||
      (m_picking_service == nullptr)) {
    return false;
  }

  {
    auto const attack = m_command_controller->on_attack_press(
        sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
    if (attack.input_consumed) {
      return true;
    }
  }

  QVector3D hit;
  if (Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    hit = App::Utils::snap_to_walkable_ground(hit);
    if (m_command_controller->formation().begin_move_placement_at_position(hit)) {
      m_command_controller->disable_run_mode_for_selected();
      return true;
    }
  }
  return false;
}

void InputCommandHandler::on_right_drag_orient(qreal sx,
                                               qreal sy,
                                               const ViewportState& viewport) {
  if ((m_command_controller == nullptr) || (m_camera == nullptr) ||
      (m_picking_service == nullptr)) {
    return;
  }

  if (!m_command_controller->formation().is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  m_command_controller->formation().aim_formation_at(hit);
}

void InputCommandHandler::on_attack_click(qreal sx,
                                          qreal sy,
                                          const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto result = m_command_controller->on_attack_click(
      sx, sy, viewport.width, viewport.height, m_camera);

  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_stop_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_stop_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_hold_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_hold_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_gate_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_gate_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_guard_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_guard_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_formation_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->formation().on_formation_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_auto_gather_command(const QString& priority_product_type) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_auto_gather_command(priority_product_type);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_run_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_command_controller == nullptr) {
    return;
  }

  auto result = m_command_controller->on_run_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_guard_click(qreal sx,
                                         qreal sy,
                                         const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto result = m_command_controller->on_guard_click(
      sx, sy, viewport.width, viewport.height, m_camera);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_civilian_delivery_click(qreal sx,
                                                     qreal sy,
                                                     int local_owner_id,
                                                     const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto result = m_command_controller->on_civilian_delivery_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_builder_repair_click(qreal sx,
                                                  qreal sy,
                                                  int local_owner_id,
                                                  const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto result = m_command_controller->on_builder_repair_click(
      sx, sy, viewport.width, viewport.height, m_camera, local_owner_id);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

auto InputCommandHandler::any_selected_in_hold_mode() const -> bool {
  if (m_command_controller == nullptr) {
    return false;
  }
  return m_command_controller->any_selected_in_hold_mode();
}

auto InputCommandHandler::any_selected_in_guard_mode() const -> bool {
  if (m_command_controller == nullptr) {
    return false;
  }
  return m_command_controller->any_selected_in_guard_mode();
}

auto InputCommandHandler::any_selected_in_formation_mode() const -> bool {
  if (m_command_controller == nullptr) {
    return false;
  }
  return m_command_controller->formation().any_selected_in_formation_mode();
}

auto InputCommandHandler::any_selected_in_run_mode() const -> bool {
  if (m_command_controller == nullptr) {
    return false;
  }
  return m_command_controller->any_selected_in_run_mode();
}

auto InputCommandHandler::is_placing_formation() const -> bool {
  if (m_command_controller == nullptr) {
    return false;
  }
  return m_command_controller->formation().is_placing_formation();
}

void InputCommandHandler::on_formation_mouse_move(qreal sx,
                                                  qreal sy,
                                                  const ViewportState& viewport) {
  if ((m_command_controller == nullptr) || (m_camera == nullptr) ||
      (m_picking_service == nullptr)) {
    return;
  }

  if (!m_command_controller->formation().is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    return;
  }

  if (m_command_controller->formation().is_dragging_formation()) {
    m_command_controller->formation().update_formation_drag(hit);
    return;
  }
  m_command_controller->formation().update_formation_placement(hit);
}

void InputCommandHandler::on_formation_drag_begin(qreal sx,
                                                  qreal sy,
                                                  const ViewportState& viewport) {
  if ((m_command_controller == nullptr) || (m_camera == nullptr) ||
      (m_picking_service == nullptr)) {
    return;
  }
  if (!m_command_controller->formation().is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
    m_command_controller->formation().begin_formation_drag(hit);
  }
}

void InputCommandHandler::on_formation_drag_update(qreal sx,
                                                   qreal sy,
                                                   const ViewportState& viewport) {
  on_formation_mouse_move(sx, sy, viewport);
}

void InputCommandHandler::on_formation_drag_end() {
  if (m_command_controller == nullptr) {
    return;
  }
  m_command_controller->formation().end_formation_drag();
}

auto InputCommandHandler::is_dragging_formation() const -> bool {
  return m_command_controller != nullptr &&
         m_command_controller->formation().is_dragging_formation();
}

void InputCommandHandler::on_formation_scroll(float delta) {
  if (m_command_controller == nullptr) {
    return;
  }

  if (!m_command_controller->formation().is_placing_formation()) {
    return;
  }

  float const current_angle =
      m_command_controller->formation().get_formation_facing_degrees();
  m_command_controller->formation().update_formation_rotation(current_angle +
                                                              delta * 5.0F);
}

void InputCommandHandler::on_formation_confirm() {
  if (m_command_controller == nullptr) {
    return;
  }
  m_command_controller->formation().confirm_formation_placement();
}

void InputCommandHandler::on_formation_cancel() {
  if (m_command_controller == nullptr) {
    return;
  }
  m_command_controller->formation().cancel_formation_placement();
}

void InputCommandHandler::on_patrol_click(qreal sx,
                                          qreal sy,
                                          const ViewportState& viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if ((m_command_controller == nullptr) || (m_camera == nullptr)) {
    return;
  }

  auto result = m_command_controller->on_patrol_click(
      sx, sy, viewport.width, viewport.height, m_camera);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
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
