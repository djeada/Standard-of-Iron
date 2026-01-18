#include "camera_visibility_service.h"
#include "../../render/gl/camera.h"
#include <cmath>

namespace Game::Systems {

auto CameraVisibilityService::instance() -> CameraVisibilityService & {
  static CameraVisibilityService s_instance;
  return s_instance;
}

void CameraVisibilityService::set_camera(const Render::GL::Camera *camera) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_camera = camera;
}

void CameraVisibilityService::clear_camera() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_camera = nullptr;
}

auto CameraVisibilityService::is_position_visible(float world_x, float world_y,
                                                  float world_z,
                                                  float radius) const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_camera == nullptr) {
    return true;
  }
  return m_camera->is_in_frustum(QVector3D(world_x, world_y, world_z), radius);
}

auto CameraVisibilityService::is_position_visible(const QVector3D &position,
                                                  float radius) const -> bool {
  return is_position_visible(position.x(), position.y(), position.z(), radius);
}

auto CameraVisibilityService::is_entity_visible(float world_x, float world_z,
                                                float radius) const -> bool {
  constexpr float k_default_entity_height = 0.5F;
  return is_position_visible(world_x, k_default_entity_height, world_z, radius);
}

namespace {
constexpr float k_detail_effects_frustum_radius = 2.0F;
}

auto CameraVisibilityService::should_process_detailed_effects(
    float world_x, float world_y, float world_z,
    float max_detail_distance) const -> bool {
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

auto CameraVisibilityService::get_camera_position() const -> QVector3D {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_camera == nullptr) {
    return QVector3D(0.0F, 0.0F, 0.0F);
  }
  return m_camera->get_position();
}

auto CameraVisibilityService::has_camera() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_camera != nullptr;
}

} // namespace Game::Systems
