#pragma once

#include <QPointF>
#include <QString>

namespace Engine::Core {
class World;
class Entity;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class SelectionSystem;
class SelectionController;
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {
class CommandController;
}

class CursorManager;
class HoverTracker;

struct ViewportState {
  int width = 0;
  int height = 0;
};

class InputCommandHandler {
public:
  InputCommandHandler(Engine::Core::World *world,
                      Game::Systems::SelectionController *selection_controller,
                      App::Controllers::CommandController *command_controller,
                      CursorManager *cursor_manager,
                      HoverTracker *hover_tracker,
                      Game::Systems::PickingService *picking_service,
                      Render::GL::Camera *camera);

  void on_map_clicked(qreal sx, qreal sy, int local_owner_id,
                      const ViewportState &viewport);
  void on_right_click(qreal sx, qreal sy, int local_owner_id,
                      const ViewportState &viewport);
  void on_attack_click(qreal sx, qreal sy, const ViewportState &viewport);
  void on_stop_command();
  void on_hold_command();
  void on_guard_command();
  void on_formation_command();
  void on_guard_click(qreal sx, qreal sy, const ViewportState &viewport);
  [[nodiscard]] bool any_selected_in_hold_mode() const;
  [[nodiscard]] bool any_selected_in_guard_mode() const;
  [[nodiscard]] bool any_selected_in_formation_mode() const;
  void on_patrol_click(qreal sx, qreal sy, const ViewportState &viewport);
  void on_click_select(qreal sx, qreal sy, bool additive, int local_owner_id,
                       const ViewportState &viewport);
  void on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive,
                        int local_owner_id, const ViewportState &viewport);
  void select_all_troops(int local_owner_id);
  void select_unit_by_id(int unit_id, int local_owner_id);
  void set_hover_at_screen(qreal sx, qreal sy, const ViewportState &viewport);
  static void reset_movement(Engine::Core::Entity *entity);

  void set_spectator_mode(bool is_spectator) {
    m_is_spectator_mode = is_spectator;
  }

private:
  Engine::Core::World *m_world;
  Game::Systems::SelectionController *m_selection_controller;
  App::Controllers::CommandController *m_command_controller;
  CursorManager *m_cursor_manager;
  HoverTracker *m_hover_tracker;
  Game::Systems::PickingService *m_picking_service;
  Render::GL::Camera *m_camera;
  bool m_is_spectator_mode = false;
};
