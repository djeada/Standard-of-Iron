#pragma once

#include <QVector3D>

#include "app/input/viewport_state.h"

namespace App::Controllers {
class ArmyFormationController;
}

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class PickingService;
}

class FormationPointerAdapter {
public:
  FormationPointerAdapter(App::Controllers::ArmyFormationController* formation,
                          Render::GL::Camera* camera,
                          Game::Systems::PickingService* picking_service);

  [[nodiscard]] auto is_placing() const -> bool;
  [[nodiscard]] auto is_dragging() const -> bool;
  [[nodiscard]] auto any_selected_in_formation_mode() const -> bool;

  void on_mouse_move(qreal sx, qreal sy, const ViewportState& viewport);
  void on_drag_begin(qreal sx, qreal sy, const ViewportState& viewport);
  void on_drag_end();
  void on_right_drag_orient(qreal sx, qreal sy, const ViewportState& viewport);
  void on_scroll(float delta);
  void confirm();
  void cancel();
  [[nodiscard]] auto begin_move_placement(const QVector3D& position) -> bool;

private:
  [[nodiscard]] auto ground_at(qreal sx,
                               qreal sy,
                               const ViewportState& viewport,
                               QVector3D& out) const -> bool;

  App::Controllers::ArmyFormationController* m_formation;
  Render::GL::Camera* m_camera;
  Game::Systems::PickingService* m_picking_service;
};
