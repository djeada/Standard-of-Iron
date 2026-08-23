#pragma once

#include <cstdint>
#include <memory>

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Game::Map {
class VisibilityService;
}

namespace Game::Systems {

class CameraController;
class CameraFollowSystem;
class SelectionSystem;

class CameraService {
public:
  explicit CameraService(const Game::Map::VisibilityService& visibility);
  ~CameraService();

  void sync_map_bounds(Render::GL::Camera& camera) const;
  void move(Render::GL::Camera& camera, float dx, float dz);
  void elevate(Render::GL::Camera& camera, float dy);
  void zoom(Render::GL::Camera& camera, float delta);
  void yaw(Render::GL::Camera& camera, float degrees);
  void orbit(Render::GL::Camera& camera, float yaw_deg, float pitch_deg);

  void tilt(Render::GL::Camera& camera, int direction, bool shift);
  void
  follow_selection(Render::GL::Camera& camera, Engine::Core::World& world, bool enable);
  void set_follow_lerp(Render::GL::Camera& camera, float alpha);
  [[nodiscard]] static auto get_distance(const Render::GL::Camera& camera) -> float;
  void reset_camera(Render::GL::Camera& camera,
                    Engine::Core::World& world,
                    int local_owner_id,
                    Engine::Core::EntityID player_unit_id);
  void snap_to_entity(Render::GL::Camera& camera, Engine::Core::Entity& entity);
  void update_follow(Render::GL::Camera& camera,
                     Engine::Core::World& world,
                     bool follow_enabled);

private:
  const Game::Map::VisibilityService& m_visibility;
  std::unique_ptr<CameraController> m_controller;
  std::unique_ptr<CameraFollowSystem> m_follow_system;
};

} // namespace Game::Systems
