#pragma once

#include <QVector3D>

namespace Render::GL {
class Camera;
}

namespace App::Core {

struct RtsCameraBookmark {
  QVector3D position{};
  QVector3D target{};
  QVector3D up{0.0F, 1.0F, 0.0F};
  float fov{45.0F};
  float near_plane{0.1F};
  float far_plane{200.0F};
  bool valid{false};

  static auto capture(const Render::GL::Camera& camera) -> RtsCameraBookmark;
  void restore(Render::GL::Camera& camera) const;
};

} // namespace App::Core
