#pragma once

#include <memory>

namespace Engine {
namespace Core {
class World;
class Entity;
} // namespace Core
} // namespace Engine

namespace Render {
namespace GL {
class Camera;
} // namespace GL
} // namespace Render

namespace Game {
namespace Systems {

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
  void orbit(Render::GL::Camera &camera, float yawDeg, float pitchDeg);
  void orbitDirection(Render::GL::Camera &camera, int direction, bool shift);
  void followSelection(Render::GL::Camera &camera, Engine::Core::World &world,
                       bool enable);
  void setFollowLerp(Render::GL::Camera &camera, float alpha);
  float getDistance(const Render::GL::Camera &camera) const;
  void resetCamera(Render::GL::Camera &camera, Engine::Core::World &world,
                   int localOwnerId, unsigned int playerUnitId);
  void snapToEntity(Render::GL::Camera &camera, Engine::Core::Entity &entity);
  void updateFollow(Render::GL::Camera &camera, Engine::Core::World &world,
                    bool followEnabled);

private:
  std::unique_ptr<CameraController> m_controller;
  std::unique_ptr<CameraFollowSystem> m_followSystem;
};

} // namespace Systems
} // namespace Game
