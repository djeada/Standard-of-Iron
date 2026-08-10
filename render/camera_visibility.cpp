#include "render/camera_visibility.h"

#include <cmath>

#include "scene/camera.h"

namespace Render::GL {

auto CameraVisibility::instance() -> CameraVisibility& {
  static CameraVisibility s_instance;
  return s_instance;
}

void CameraVisibility::set_camera(const Camera* camera) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_camera = camera;
}

void CameraVisibility::clear_camera() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_camera = nullptr;
}

auto CameraVisibility::is_position_visible(float world_x,
                                           float world_y,
                                           float world_z,
                                           float radius) const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_camera == nullptr) {
    return true;
  }
  return m_camera->is_in_frustum(QVector3D(world_x, world_y, world_z), radius);
}

auto CameraVisibility::is_position_visible(const QVector3D& position,
                                           float radius) const -> bool {
  return is_position_visible(position.x(), position.y(), position.z(), radius);
}

auto CameraVisibility::is_entity_visible(float world_x,
                                         float world_z,
                                         float radius) const -> bool {
  constexpr float k_default_entity_height = 0.5F;
  return is_position_visible(world_x, k_default_entity_height, world_z, radius);
}

namespace {
constexpr float k_detail_effects_frustum_radius = 2.0F;
}

auto CameraVisibility::should_process_detailed_effects(float world_x,
                                                       float world_y,
                                                       float world_z,
                                                       float max_detail_distance) const
    -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_camera == nullptr) {
    return true;
  }

  if (!m_camera->is_in_frustum(QVector3D(world_x, world_y, world_z),
                               k_detail_effects_frustum_radius)) {
    return false;
  }

  QVector3D const cam_pos = m_camera->get_position();
  float const dx = world_x - cam_pos.x();
  float const dy = world_y - cam_pos.y();
  float const dz = world_z - cam_pos.z();
  float const dist_sq = dx * dx + dy * dy + dz * dz;
  return dist_sq <= max_detail_distance * max_detail_distance;
}

auto CameraVisibility::get_camera_position() const -> QVector3D {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_camera == nullptr) {
    return QVector3D(0.0F, 0.0F, 0.0F);
  }
  return m_camera->get_position();
}

auto CameraVisibility::has_camera() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_camera != nullptr;
}

} // namespace Render::GL
