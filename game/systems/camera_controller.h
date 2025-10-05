#pragma once

namespace Render {
namespace GL {
class Camera;
}
} // namespace Render

namespace Game {
namespace Systems {

class CameraController {
public:
  void move(Render::GL::Camera &camera, float dx, float dz) const;
  void elevate(Render::GL::Camera &camera, float dy) const;
  void yaw(Render::GL::Camera &camera, float degrees) const;
  void orbit(Render::GL::Camera &camera, float yawDeg, float pitchDeg) const;
  void setFollowEnabled(Render::GL::Camera &camera, bool enable) const;
  void setFollowLerp(Render::GL::Camera &camera, float alpha) const;
};

} // namespace Systems
} // namespace Game
