#include "app/viewmodels/camera_view_model.h"

#include "app/core/client_context.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/utils/engine_view_helpers.h"

namespace App::ViewModels {

CameraViewModel::CameraViewModel(const App::Core::ClientContext& context,
                                 App::Core::ClientHost& host,
                                 QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host) {
}

void CameraViewModel::move(float dx, float dz) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->move(dx, dz);
  }
}

void CameraViewModel::elevate(float dy) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->elevate(dy);
  }
}

void CameraViewModel::zoom(float delta) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->zoom(delta);
    emit distance_changed();
  }
}

void CameraViewModel::yaw(float degrees) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->yaw(degrees);
  }
}

void CameraViewModel::orbit(float yaw_deg, float pitch_deg) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->orbit(yaw_deg, pitch_deg);
  }
}

void CameraViewModel::tilt(int direction, bool shift) {
  m_host.ensure_initialized();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->tilt(direction, shift);
  }
}

void CameraViewModel::reset() {
  m_host.ensure_initialized();
  emit moved();
  auto* camera = m_context.camera_controller;
  if (camera == nullptr || m_context.level == nullptr) {
    return;
  }
  camera->reset(m_context.local_owner_id, *m_context.level);
  emit distance_changed();
}

void CameraViewModel::follow_selection(bool enable) {
  m_host.ensure_initialized();
  set_following_selection(enable);
}

void CameraViewModel::set_follow_lerp(float alpha) {
  m_host.ensure_initialized();
  if (auto* camera = m_context.camera_controller) {
    camera->set_follow_lerp(alpha);
  }
}

auto CameraViewModel::project_world(float x, float y, float z) const -> QVariantMap {
  QVariantMap result;
  result["valid"] = false;
  result["x"] = 0.0;
  result["y"] = 0.0;
  QPointF screen;
  if (m_context.viewport != nullptr &&
      App::Utils::world_to_screen(m_context.picking,
                                  m_context.active_camera,
                                  m_context.window,
                                  m_context.viewport->width,
                                  m_context.viewport->height,
                                  QVector3D(x, y, z),
                                  screen)) {
    result["valid"] = true;
    result["x"] = screen.x();
    result["y"] = screen.y();
  }
  return result;
}

auto CameraViewModel::distance() const -> float {
  if (auto* camera = m_context.camera_controller) {
    return camera->distance();
  }
  return 0.0F;
}

void CameraViewModel::update_follow() {
  if (auto* camera = m_context.camera_controller) {
    camera->update_follow(m_following_selection);
  }
}

void CameraViewModel::set_following_selection(bool enable) {
  if (auto* camera = m_context.camera_controller) {
    camera->follow_selection(enable);
  }
  if (m_following_selection == enable) {
    return;
  }
  m_following_selection = enable;
  emit following_selection_changed();
}

} // namespace App::ViewModels
