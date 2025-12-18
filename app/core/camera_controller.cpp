#include "camera_controller.h"

#include "game/core/world.h"
#include "game/systems/camera_service.h"
#include "game/systems/game_state_serializer.h"
#include "render/gl/camera.h"
#include <QDebug>
#include <cmath>

CameraController::CameraController(Render::GL::Camera *camera,
                                   Game::Systems::CameraService *camera_service,
                                   Engine::Core::World *world)
    : m_camera(camera), m_camera_service(camera_service), m_world(world) {}

void CameraController::move(float dx, float dz) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->move(*m_camera, dx, dz);
}

void CameraController::elevate(float dy) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->elevate(*m_camera, dy);
}

void CameraController::reset(int local_owner_id,
                             const Game::Systems::LevelSnapshot &level) {
  if (!m_camera || !m_world || !m_camera_service) {
    return;
  }
  m_camera_service->reset_camera(*m_camera, *m_world, local_owner_id,
                                 level.player_unit_id);
}

void CameraController::zoom(float delta) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->zoom(*m_camera, delta);
}

auto CameraController::distance() const -> float {
  if (!m_camera || !m_camera_service) {
    return 0.0F;
  }
  return m_camera_service->get_distance(*m_camera);
}

void CameraController::yaw(float degrees) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->yaw(*m_camera, degrees);
}

void CameraController::orbit(float yaw_deg, float pitch_deg) {
  if (!m_camera || !m_camera_service) {
    return;
  }

  if (!std::isfinite(yaw_deg) || !std::isfinite(pitch_deg)) {
    qWarning() << "CameraController::orbit received invalid input, ignoring:"
               << yaw_deg << pitch_deg;
    return;
  }

  m_camera_service->orbit(*m_camera, yaw_deg, pitch_deg);
}

void CameraController::orbit_direction(int direction, bool shift) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->orbit_direction(*m_camera, direction, shift);
}

void CameraController::follow_selection(bool enable) {
  if (!m_camera || !m_world || !m_camera_service) {
    return;
  }
  m_camera_service->follow_selection(*m_camera, *m_world, enable);
}

void CameraController::set_follow_lerp(float alpha) {
  if (!m_camera || !m_camera_service) {
    return;
  }
  m_camera_service->set_follow_lerp(*m_camera, alpha);
}

void CameraController::update_follow(bool follow_enabled) {
  if (follow_enabled && m_camera && m_world && m_camera_service) {
    m_camera_service->update_follow(*m_camera, *m_world, follow_enabled);
  }
}
