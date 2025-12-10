#pragma once

#include <memory>

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class CameraController;
class CameraFollowSystem;
class SelectionSystem;

class CameraService {
public:
  CameraService();
  ~CameraService();

  void move(Render::GL::Camera &camera, float dx, float dz);
  void elevate(Render::GL::Camera &camera, float dy);
  void zoom(Render::GL::Camera &camera, float delta);
  void yaw(Render::GL::Camera &camera, float degrees);
  void orbit(Render::GL::Camera &camera, float yaw_deg, float pitch_deg);
  void orbitDirection(Render::GL::Camera &camera, int direction, bool shift);
  void followSelection(Render::GL::Camera &camera, Engine::Core::World &world,
                       bool enable);
  void setFollowLerp(Render::GL::Camera &camera, float alpha);
  [[nodiscard]] static auto
  get_distance(const Render::GL::Camera &camera) -> float;
  static void resetCamera(Render::GL::Camera &camera,
                          Engine::Core::World &world, int local_owner_id,
                          unsigned int playerUnitId);
  static void snapToEntity(Render::GL::Camera &camera,
                           Engine::Core::Entity &entity);
  void updateFollow(Render::GL::Camera &camera, Engine::Core::World &world,
                    bool follow_enabled);

private:
  std::unique_ptr<CameraController> m_controller;
  std::unique_ptr<CameraFollowSystem> m_followSystem;
};

} // namespace Game::Systems
