#pragma once

#include <QVector3D>
#include <cstdint>
#include <mutex>

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class CameraVisibilityService {
public:
  static auto instance() -> CameraVisibilityService &;

  void set_camera(const Render::GL::Camera *camera);
  void clear_camera();

  [[nodiscard]] auto is_position_visible(float world_x, float world_y,
                                         float world_z,
                                         float radius = 1.0F) const -> bool;

  [[nodiscard]] auto is_position_visible(const QVector3D &position,
                                         float radius = 1.0F) const -> bool;

  [[nodiscard]] auto is_entity_visible(float world_x, float world_z,
                                       float radius = 2.0F) const -> bool;

  [[nodiscard]] auto should_process_detailed_effects(
      float world_x, float world_y, float world_z,
      float max_detail_distance = 50.0F) const -> bool;

  [[nodiscard]] auto get_camera_position() const -> QVector3D;

  [[nodiscard]] auto has_camera() const -> bool;

private:
  CameraVisibilityService() = default;
  ~CameraVisibilityService() = default;
  CameraVisibilityService(const CameraVisibilityService &) = delete;
  auto operator=(const CameraVisibilityService &) -> CameraVisibilityService & =
                                                         delete;

  const Render::GL::Camera *m_camera{nullptr};
  mutable std::mutex m_mutex;
};

} // namespace Game::Systems
