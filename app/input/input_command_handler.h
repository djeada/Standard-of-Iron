#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>

#include "app/input/context_interaction_resolver.h"
#include "app/input/formation_pointer_adapter.h"
#include "app/input/viewport_state.h"

namespace Engine::Core {
class World;
class Entity;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class SelectionController;
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {
class CommandController;
}

namespace Game::Session {
class SelectionService;
}

class CursorManager;
class HoverTracker;

class InputCommandHandler {
public:
  InputCommandHandler(Engine::Core::World* world,
                      Game::Systems::SelectionController* selection_controller,
                      App::Controllers::CommandController* command_controller,
                      CursorManager* cursor_manager,
                      HoverTracker* hover_tracker,
                      Game::Systems::PickingService* picking_service,
                      Render::GL::Camera* camera);

  void
  on_map_clicked(qreal sx, qreal sy, int local_owner_id, const ViewportState& viewport);
  void
  on_right_click(qreal sx, qreal sy, int local_owner_id, const ViewportState& viewport);
  void on_right_double_click(qreal sx,
                             qreal sy,
                             int local_owner_id,
                             const ViewportState& viewport);
  [[nodiscard]] bool
  on_right_press(qreal sx, qreal sy, int local_owner_id, const ViewportState& viewport);
  void on_right_drag_orient(qreal sx, qreal sy, const ViewportState& viewport);
  void on_minimap_right_click(const QVector3D& world_target, int local_owner_id);
  void on_attack_click(qreal sx,
                       qreal sy,
                       int local_owner_id,
                       const ViewportState& viewport);
  void on_stop_command();
  void on_hold_command();
  void on_gate_command();
  void on_roll_stones_command();
  void on_guard_command();
  void on_formation_command();
  void on_auto_gather_command(const QString& priority_product_type = {});
  void divide_selected_squads();

  void merge_selected_squads();

  void set_auto_gather(bool active, const QString& priority_product_type = {});
  void on_run_command();
  void on_guard_click(qreal sx, qreal sy, const ViewportState& viewport);
  void on_civilian_delivery_click(qreal sx,
                                  qreal sy,
                                  int local_owner_id,
                                  const ViewportState& viewport);
  void on_builder_repair_click(qreal sx,
                               qreal sy,
                               int local_owner_id,
                               const ViewportState& viewport);
  void on_builder_dismantle_click(qreal sx,
                                  qreal sy,
                                  int local_owner_id,
                                  const ViewportState& viewport);
  [[nodiscard]] bool
  resolve_context_interaction(qreal sx,
                              qreal sy,
                              const ViewportState& viewport,
                              QString& out_product_type,
                              Engine::Core::EntityID& out_target) const;

  [[nodiscard]] auto resolve_context_interaction(
      qreal sx, qreal sy, const ViewportState& viewport) const -> ContextInteraction;

  [[nodiscard]] bool any_selected_in_hold_mode() const;
  [[nodiscard]] bool any_selected_in_guard_mode() const;
  [[nodiscard]] bool any_selected_in_formation_mode() const;
  [[nodiscard]] bool any_selected_in_run_mode() const;
  [[nodiscard]] bool is_placing_formation() const;
  void on_formation_mouse_move(qreal sx, qreal sy, const ViewportState& viewport);
  void on_formation_scroll(float delta);
  void on_formation_confirm();
  void on_formation_cancel();
  void on_formation_drag_begin(qreal sx, qreal sy, const ViewportState& viewport);
  void on_formation_drag_update(qreal sx, qreal sy, const ViewportState& viewport);
  void on_formation_drag_end();
  [[nodiscard]] bool is_dragging_formation() const;
  void on_patrol_click(qreal sx, qreal sy, const ViewportState& viewport);
  void on_click_select(qreal sx,
                       qreal sy,
                       bool additive,
                       int local_owner_id,
                       const ViewportState& viewport);
  void on_area_selected(qreal x1,
                        qreal y1,
                        qreal x2,
                        qreal y2,
                        bool additive,
                        int local_owner_id,
                        const ViewportState& viewport);
  void select_all_troops(int local_owner_id);
  void select_unit_by_id(Engine::Core::EntityID unit_id, int local_owner_id);
  void select_selected_units_by_type(const QString& unit_type, int local_owner_id);
  void set_hover_at_screen(qreal sx, qreal sy, const ViewportState& viewport);

  void set_spectator_mode(bool is_spectator) { m_is_spectator_mode = is_spectator; }

private:
  void reset_order_modes();
  [[nodiscard]] auto cursor_cancels_order_mode() const -> bool;
  [[nodiscard]] auto selection() const -> Game::Session::SelectionService&;
  [[nodiscard]] auto issue_context_interaction(qreal sx,
                                               qreal sy,
                                               int local_owner_id,
                                               const ViewportState& viewport) -> bool;
  template <class Issue>
  void issue_command(Issue&& issue);
  template <class Issue>
  void issue_camera_command(Issue&& issue);

  Engine::Core::World* m_world;
  Game::Systems::SelectionController* m_selection_controller;
  App::Controllers::CommandController* m_command_controller;
  CursorManager* m_cursor_manager;
  HoverTracker* m_hover_tracker;
  Game::Systems::PickingService* m_picking_service;
  Render::GL::Camera* m_camera;
  ContextInteractionResolver m_context;
  FormationPointerAdapter m_formation;
  bool m_is_spectator_mode = false;
};
