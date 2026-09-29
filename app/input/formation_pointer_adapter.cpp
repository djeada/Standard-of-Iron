#include "app/input/formation_pointer_adapter.h"

#include <QPointF>

#include "app/orders/army_formation_controller.h"
#include "game/render_bridge/picking_service.h"
#include "scene/camera.h"

namespace {
constexpr float k_scroll_degrees_per_notch = 5.0F;
}

FormationPointerAdapter::FormationPointerAdapter(
    App::Controllers::ArmyFormationController* formation,
    Render::GL::Camera* camera,
    Game::Systems::PickingService* picking_service)
    : m_formation(formation)
    , m_camera(camera)
    , m_picking_service(picking_service) {
}

auto FormationPointerAdapter::is_placing() const -> bool {
  return m_formation != nullptr && m_formation->is_placing_formation();
}

auto FormationPointerAdapter::is_dragging() const -> bool {
  return m_formation != nullptr && m_formation->is_dragging_formation();
}

auto FormationPointerAdapter::any_selected_in_formation_mode() const -> bool {
  return m_formation != nullptr && m_formation->any_selected_in_formation_mode();
}

auto FormationPointerAdapter::ground_at(qreal sx,
                                        qreal sy,
                                        const ViewportState& viewport,
                                        QVector3D& out) const -> bool {
  return Game::Systems::PickingService::screen_to_ground(
      QPointF(sx, sy), *m_camera, viewport.width, viewport.height, out);
}

void FormationPointerAdapter::on_mouse_move(qreal sx,
                                            qreal sy,
                                            const ViewportState& viewport) {
  if (m_formation == nullptr || m_camera == nullptr || m_picking_service == nullptr ||
      !m_formation->is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (!ground_at(sx, sy, viewport, hit)) {
    return;
  }

  if (m_formation->is_dragging_formation()) {
    m_formation->update_formation_drag(hit);
    return;
  }
  m_formation->update_formation_placement(hit);
}

void FormationPointerAdapter::on_drag_begin(qreal sx,
                                            qreal sy,
                                            const ViewportState& viewport) {
  if (m_formation == nullptr || m_camera == nullptr || m_picking_service == nullptr ||
      !m_formation->is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (ground_at(sx, sy, viewport, hit)) {
    m_formation->begin_formation_drag(hit);
  }
}

void FormationPointerAdapter::on_drag_end() {
  if (m_formation != nullptr) {
    m_formation->end_formation_drag();
  }
}

void FormationPointerAdapter::on_right_drag_orient(qreal sx,
                                                   qreal sy,
                                                   const ViewportState& viewport) {
  if (m_formation == nullptr || m_camera == nullptr || m_picking_service == nullptr ||
      !m_formation->is_placing_formation()) {
    return;
  }

  QVector3D hit;
  if (ground_at(sx, sy, viewport, hit)) {
    m_formation->aim_formation_at(hit);
  }
}

void FormationPointerAdapter::on_scroll(float delta) {
  if (m_formation == nullptr || !m_formation->is_placing_formation()) {
    return;
  }
  m_formation->update_formation_rotation(m_formation->get_formation_facing_degrees() +
                                         delta * k_scroll_degrees_per_notch);
}

void FormationPointerAdapter::confirm() {
  if (m_formation != nullptr) {
    m_formation->confirm_formation_placement();
  }
}

void FormationPointerAdapter::cancel() {
  if (m_formation != nullptr) {
    m_formation->cancel_formation_placement();
  }
}

auto FormationPointerAdapter::begin_move_placement(const QVector3D& position) -> bool {
  return m_formation != nullptr &&
         m_formation->begin_move_placement_at_position(position);
}
