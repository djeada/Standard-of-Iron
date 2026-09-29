#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>

#include "app/commander/commander_camera_rig.h"
#include "game/core/component.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

class CommanderPresentation {
public:
  void publish_sample(Engine::Core::Entity& commander,
                      const Engine::Core::TransformComponent& transform,
                      float dt);

  [[nodiscard]] auto advance_pose(Engine::Core::Entity& commander,
                                  const Engine::Core::TransformComponent& transform,
                                  float dt) -> Engine::Core::PresentationPose;

  void snap();

  [[nodiscard]] auto pose() const -> const Engine::Core::PresentationPose& {
    return m_pose;
  }

private:
  Engine::Core::PresentationPose m_pose;
  bool m_snap_requested = true;
  Engine::Core::PresentationClock m_clock;
};

struct CameraFeed {
  float dt{0.0F};
  float view_yaw{0.0F};
  float view_pitch{0.0F};
  float move_speed{0.0F};
  int move_right_axis{0};
  bool move_running{false};
  float dodge_fov_kick{0.0F};
  bool dodge_rolling{false};
  float dodge_timer{0.0F};
  QVector3D dodge_direction{0.0F, 0.0F, 1.0F};
  std::optional<QVector3D> lock_target_position;
};

[[nodiscard]] auto build_camera_inputs(Engine::Core::World& world,
                                       Engine::Core::Entity& commander,
                                       const CameraFeed& feed,
                                       const Engine::Core::PresentationPose& pose)
    -> CommanderCameraInputs;

void play_footstep_if_stride_landed(const CommanderCameraRig& rig,
                                    const Engine::Core::Entity& commander,
                                    bool running,
                                    float previous_bob_phase);

} // namespace App::Core
