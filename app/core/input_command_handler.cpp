#include "input_command_handler.h"

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/cursor_manager.h"
#include "../models/cursor_mode.h"
#include "../models/hover_tracker.h"
#include "../utils/movement_utils.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "render/gl/camera.h"

InputCommandHandler::InputCommandHandler(
    Engine::Core::World *world,
    Game::Systems::SelectionController *selection_controller,
    App::Controllers::CommandController *command_controller,
    CursorManager *cursor_manager, HoverTracker *hover_tracker,
    Game::Systems::PickingService *picking_service, Render::GL::Camera *camera)
    : m_world(world), m_selection_controller(selection_controller),
      m_command_controller(command_controller),
      m_cursor_manager(cursor_manager), m_hover_tracker(hover_tracker),
      m_picking_service(picking_service), m_camera(camera) {}

void InputCommandHandler::on_map_clicked(qreal sx, qreal sy, int local_owner_id,
                                         const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller && m_camera) {
    m_selection_controller->on_click_select(sx, sy, false, viewport.width,
                                            viewport.height, m_camera,
                                            local_owner_id);
  }
}

void InputCommandHandler::on_right_click(qreal sx, qreal sy, int local_owner_id,
                                         const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_world) {
    return;
  }

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  if (m_cursor_manager->mode() == CursorMode::Patrol ||
      m_cursor_manager->mode() == CursorMode::Attack ||
      m_cursor_manager->mode() == CursorMode::Guard) {
    m_cursor_manager->set_mode(CursorMode::Normal);
    return;
  }

  const auto &sel = selection_system->get_selected_units();
  if (sel.empty()) {
    return;
  }

  if (m_picking_service && m_camera) {
    Engine::Core::EntityID const target_id = m_picking_service->pick_unit_first(
        float(sx), float(sy), *m_world, *m_camera, viewport.width,
        viewport.height, 0);

    if (target_id != 0U) {
      auto *target_entity = m_world->get_entity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->get_component<Engine::Core::UnitComponent>();
        if (target_unit != nullptr) {
          bool const is_enemy = (target_unit->owner_id != local_owner_id);

          if (is_enemy) {
            Game::Systems::CommandService::attack_target(*m_world, sel,
                                                         target_id, true);
            return;
          }
        }
      }
    }
  }

  if (m_picking_service && m_camera) {
    QVector3D hit;
    if (m_picking_service->screen_to_ground(
            QPointF(sx, sy), *m_camera, viewport.width, viewport.height, hit)) {
      auto targets = Game::Systems::FormationPlanner::spreadFormation(
          int(sel.size()), hit,
          Game::GameConfig::instance().gameplay().formation_spacing_default);
      Game::Systems::CommandService::MoveOptions opts;
      opts.group_move = sel.size() > 1;
      Game::Systems::CommandService::moveUnits(*m_world, sel, targets, opts);
    }
  }
}

void InputCommandHandler::on_attack_click(qreal sx, qreal sy,
                                          const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_command_controller || !m_camera) {
    return;
  }

  auto result = m_command_controller->on_attack_click(sx, sy, viewport.width,
                                                      viewport.height, m_camera);

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if ((selection_system == nullptr) || !m_picking_service || !m_camera ||
      !m_world) {
    return;
  }

  const auto &selected = selection_system->get_selected_units();
  if (!selected.empty()) {
    Engine::Core::EntityID const target_id = m_picking_service->pick_unit_first(
        float(sx), float(sy), *m_world, *m_camera, viewport.width,
        viewport.height, 0);

    if (target_id != 0) {
      auto *target_entity = m_world->get_entity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->get_component<Engine::Core::UnitComponent>();
        if ((target_unit != nullptr)) {
          App::Controllers::ActionVFX::spawnAttackArrow(m_world, target_id);
        }
      }
    }
  }

  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::reset_movement(Engine::Core::Entity *entity) {
  App::Utils::reset_movement(entity);
}

void InputCommandHandler::on_stop_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_command_controller) {
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
  if (!m_command_controller) {
    return;
  }

  auto result = m_command_controller->on_hold_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_guard_command() {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_command_controller) {
    return;
  }

  auto result = m_command_controller->on_guard_command();
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_guard_click(qreal sx, qreal sy,
                                         const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_command_controller || !m_camera) {
    return;
  }

  auto result = m_command_controller->on_guard_click(sx, sy, viewport.width,
                                                     viewport.height, m_camera);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

auto InputCommandHandler::any_selected_in_hold_mode() const -> bool {
  if (!m_command_controller) {
    return false;
  }
  return m_command_controller->any_selected_in_hold_mode();
}

auto InputCommandHandler::any_selected_in_guard_mode() const -> bool {
  if (!m_command_controller) {
    return false;
  }
  return m_command_controller->any_selected_in_guard_mode();
}

void InputCommandHandler::on_patrol_click(qreal sx, qreal sy,
                                          const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_command_controller || !m_camera) {
    return;
  }

  auto result = m_command_controller->on_patrol_click(sx, sy, viewport.width,
                                                      viewport.height, m_camera);
  if (result.reset_cursor_to_normal) {
    m_cursor_manager->set_mode(CursorMode::Normal);
  }
}

void InputCommandHandler::on_click_select(qreal sx, qreal sy, bool additive,
                                          int local_owner_id,
                                          const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller && m_camera) {
    m_selection_controller->on_click_select(sx, sy, additive, viewport.width,
                                            viewport.height, m_camera,
                                            local_owner_id);
  }
}

void InputCommandHandler::on_area_selected(qreal x1, qreal y1, qreal x2,
                                           qreal y2, bool additive,
                                           int local_owner_id,
                                           const ViewportState &viewport) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller && m_camera) {
    m_selection_controller->on_area_selected(x1, y1, x2, y2, additive,
                                             viewport.width, viewport.height,
                                             m_camera, local_owner_id);
  }
}

void InputCommandHandler::select_all_troops(int local_owner_id) {
  if (m_is_spectator_mode) {
    return;
  }
  if (m_selection_controller) {
    m_selection_controller->select_all_player_troops(local_owner_id);
  }
}

void InputCommandHandler::select_unit_by_id(int unit_id, int local_owner_id) {
  if (m_is_spectator_mode) {
    return;
  }
  if (!m_selection_controller || (unit_id <= 0)) {
    return;
  }
  m_selection_controller->select_single_unit(
      static_cast<Engine::Core::EntityID>(unit_id), local_owner_id);
}

void InputCommandHandler::set_hover_at_screen(qreal sx, qreal sy,
                                              const ViewportState &viewport) {
  if (!m_hover_tracker || !m_camera || !m_world) {
    return;
  }

  m_hover_tracker->update_hover(float(sx), float(sy), *m_world, *m_camera,
                                viewport.width, viewport.height);
}
