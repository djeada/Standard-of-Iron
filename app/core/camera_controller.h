#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class CameraService;
struct LevelSnapshot;
} // namespace Game::Systems

class CameraController {
public:
  CameraController(Render::GL::Camera *camera,
                   Game::Systems::CameraService *camera_service,
                   Engine::Core::World *world);

  void move(float dx, float dz);
  void elevate(float dy);
  void reset(int local_owner_id, const Game::Systems::LevelSnapshot &level);
  void zoom(float delta);
  [[nodiscard]] float distance() const;
  void yaw(float degrees);
  void orbit(float yaw_deg, float pitch_deg);
  void orbit_direction(int direction, bool shift);
  void follow_selection(bool enable);
  void set_follow_lerp(float alpha);
  void update_follow(bool follow_enabled);

private:
  Render::GL::Camera *m_camera;
  Game::Systems::CameraService *m_camera_service;
  Engine::Core::World *m_world;
};
