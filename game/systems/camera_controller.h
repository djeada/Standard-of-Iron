#pragma once

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class CameraController {
public:
  static void move(Render::GL::Camera &camera, float dx, float dz);
  static void elevate(Render::GL::Camera &camera, float dy);
  static void move_up(Render::GL::Camera &camera, float dy);
  static void yaw(Render::GL::Camera &camera, float degrees);
  static void orbit(Render::GL::Camera &camera, float yaw_deg, float pitch_deg);
  static void zoom_distance(Render::GL::Camera &camera, float delta);
  static void set_follow_enabled(Render::GL::Camera &camera, bool enable);
  static void set_follow_lerp(Render::GL::Camera &camera, float alpha);
};

} // namespace Game::Systems
