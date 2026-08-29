#include "app/viewmodels/camera_view_model.h"

#include <QVector3D>

#include <cmath>

#include "app/core/client_context.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/utils/engine_view_helpers.h"
#include "scene/camera.h"

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
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->move(dx, dz);
  }
}

void CameraViewModel::elevate(float dy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->elevate(dy);
  }
}

void CameraViewModel::zoom(float delta) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->zoom(delta);
    emit distance_changed();
  }
}

namespace {
constexpr float k_orbit_drag_yaw_degrees_per_pixel = 0.35F;
constexpr float k_orbit_drag_pitch_degrees_per_pixel = 0.2F;
} // namespace

auto CameraViewModel::ground_under_screen(float sx,
                                          float sy,
                                          QVector3D& out) const -> bool {
  const auto* camera = m_context.active_camera;
  if (camera == nullptr || m_context.viewport == nullptr ||
      m_context.viewport->width <= 0 || m_context.viewport->height <= 0) {
    return false;
  }
  return camera->screen_to_ground(
      sx, sy, m_context.viewport->width, m_context.viewport->height, out);
}

void CameraViewModel::zoom_at_screen(float delta, float sx, float sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  auto* controller = m_context.camera_controller;
  auto* camera = m_context.active_camera;
  if (controller == nullptr) {
    return;
  }
  controller->zoom(delta);
  if (camera != nullptr && m_context.viewport != nullptr &&
      m_context.viewport->width > 0 && m_context.viewport->height > 0) {
    camera->set_zoom_anchor(sx / static_cast<float>(m_context.viewport->width),
                            sy / static_cast<float>(m_context.viewport->height));
  }
  emit distance_changed();
}

void CameraViewModel::drag_pan_begin(float sx, float sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_drag_pan_active = ground_under_screen(sx, sy, m_drag_pan_anchor);
  if (m_drag_pan_active) {
    set_following_selection(false);
  }
}

void CameraViewModel::drag_pan_update(float sx, float sy) {
  if (!m_drag_pan_active) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* camera = m_context.active_camera;
  QVector3D current;
  if (camera == nullptr || !ground_under_screen(sx, sy, current)) {
    return;
  }
  camera->translate(m_drag_pan_anchor - current);
  emit moved();
}

void CameraViewModel::drag_pan_end() {
  m_drag_pan_active = false;
}

void CameraViewModel::orbit_drag_begin(float sx, float sy) {
  m_host.ensure_initialized();
  m_orbit_drag_active = true;
  m_orbit_drag_last_x = sx;
  m_orbit_drag_last_y = sy;
}

void CameraViewModel::orbit_drag_update(float sx, float sy) {
  if (!m_orbit_drag_active) {
    return;
  }
  float const dx = sx - m_orbit_drag_last_x;
  float const dy = sy - m_orbit_drag_last_y;
  m_orbit_drag_last_x = sx;
  m_orbit_drag_last_y = sy;
  if (!std::isfinite(dx) || !std::isfinite(dy)) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (auto* camera = m_context.active_camera) {
    camera->rotate_immediate(dx * k_orbit_drag_yaw_degrees_per_pixel,
                             dy * k_orbit_drag_pitch_degrees_per_pixel);
    emit moved();
  }
}

void CameraViewModel::orbit_drag_end() {
  m_orbit_drag_active = false;
}

void CameraViewModel::yaw(float degrees) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->yaw(degrees);
  }
}

void CameraViewModel::orbit(float yaw_deg, float pitch_deg) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->orbit(yaw_deg, pitch_deg);
  }
}

void CameraViewModel::tilt(int direction, bool shift) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  if (auto* camera = m_context.camera_controller) {
    camera->tilt(direction, shift);
  }
}

void CameraViewModel::reset() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  emit moved();
  auto* camera = m_context.camera_controller;
  if (camera == nullptr || m_context.level == nullptr) {
    return;
  }
  camera->reset(m_context.local_owner_id, *m_context.level);
  emit distance_changed();
}

void CameraViewModel::look_at_world(float x, float z) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* camera = m_context.active_camera;
  if (camera == nullptr) {
    return;
  }
  const QVector3D target(x, 0.0F, z);
  const QVector3D offset = camera->get_position() - camera->get_target();
  camera->look_at(target + offset, target, camera->get_up_vector());
  set_following_selection(false);
  emit moved();
}

void CameraViewModel::follow_selection(bool enable) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  set_following_selection(enable);
}

void CameraViewModel::set_follow_lerp(float alpha) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (auto* camera = m_context.camera_controller) {
    camera->set_follow_lerp(alpha);
  }
}

void CameraViewModel::publish_frame() {
  const auto* camera = m_context.active_camera;
  if (camera == nullptr || m_context.viewport == nullptr ||
      m_context.viewport->width <= 0 || m_context.viewport->height <= 0) {
    return;
  }
  m_projection.publish({.view_projection = camera->get_view_projection_matrix(),
                        .viewport_width = m_context.viewport->width,
                        .viewport_height = m_context.viewport->height,
                        .distance = m_context.camera_controller != nullptr
                                        ? m_context.camera_controller->distance()
                                        : camera->get_distance()});
}

auto CameraViewModel::project_world(float x, float y, float z) const -> QVariantMap {

  QVariantMap result;
  result["valid"] = false;
  result["x"] = 0.0;
  result["y"] = 0.0;
  const auto projection = m_projection.read();
  QPointF screen;
  if (projection && projection->project(QVector3D(x, y, z), screen)) {
    result["valid"] = true;
    result["x"] = screen.x();
    result["y"] = screen.y();
  }
  return result;
}

auto CameraViewModel::distance() const -> float {
  const auto projection = m_projection.read();
  return projection ? projection->distance : 0.0F;
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
