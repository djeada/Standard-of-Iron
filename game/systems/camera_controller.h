#pragma once

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class CameraController {
public:
  static void move(Render::GL::Camera &camera, float dx, float dz);
  static void elevate(Render::GL::Camera &camera, float dy);
  static void moveUp(Render::GL::Camera &camera, float dy);
  static void yaw(Render::GL::Camera &camera, float degrees);
  static void orbit(Render::GL::Camera &camera, float yaw_deg, float pitch_deg);
  static void zoomDistance(Render::GL::Camera &camera, float delta);
  static void setFollowEnabled(Render::GL::Camera &camera, bool enable);
  static void setFollowLerp(Render::GL::Camera &camera, float alpha);
};

} // namespace Game::Systems
