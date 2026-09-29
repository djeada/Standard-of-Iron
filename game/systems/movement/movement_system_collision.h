#pragma once

#include <QVector3D>

#include "body_profile.h"
#include "core/component_combat.h"

namespace Game::Systems::MovementCollision {

struct SweepResult {
  float accepted_dx{0.0F};
  float accepted_dz{0.0F};
  float rejected_dx{0.0F};
  float rejected_dz{0.0F};
  float accepted_fraction{1.0F};
  bool contact{false};
  bool blocked{false};
  float normal_x{0.0F};
  float normal_z{0.0F};
};

class MotorCollision {
public:
  MotorCollision(const Engine::Core::Entity& entity,
                 float origin_x,
                 float origin_z,
                 bool respect_body_radius = false,
                 bool escaping = false);

  [[nodiscard]] auto was_on_valid_tile() const -> bool { return m_valid_tile; }
  [[nodiscard]] auto point_allowed(float wx, float wz) const -> bool;
  [[nodiscard]] auto
  step_allowed(float from_x, float from_z, float to_x, float to_z) const -> bool;

  [[nodiscard]] static auto
  body_profile(const Engine::Core::Entity& entity) -> BodyProfile {
    return body_profile_for(entity);
  }

private:
  [[nodiscard]] auto terrain_allows(float wx, float wz) const -> bool;
  [[nodiscard]] auto allowed_here(const QVector3D& point) const -> bool;

  const Engine::Core::Entity* m_entity;
  bool m_respect_body_radius{false};
  bool m_escaping{false};
  bool m_origin_on_terrain{true};
  bool m_valid_tile{true};
  float m_trapped_depth{0.0F};
};

[[nodiscard]] auto sweep_through(const MotorCollision& collision,
                                 float origin_x,
                                 float origin_z,
                                 float delta_x,
                                 float delta_z) -> SweepResult;

auto slide_body_to(const Engine::Core::Entity& entity,
                   Engine::Core::TransformComponent& transform,
                   float target_x,
                   float target_z,
                   bool respect_body_radius = false) -> SweepResult;

void unstick_body(const Engine::Core::Entity& entity,
                  Engine::Core::TransformComponent& transform,
                  float delta_time);

} // namespace Game::Systems::MovementCollision
