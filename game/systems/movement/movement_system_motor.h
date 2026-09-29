#pragma once

#include "core/component_combat.h"
#include "movement_system.h"
#include "movement_system_collision.h"
#include "movement_system_mover.h"

namespace Game::Systems {

class MovementSystem::Motor {
public:
  static void drive(Mover& mover);

  static void publish_displacement(Engine::Core::MovementFactsComponent& facts,
                                   const Engine::Core::TransformComponent& transform,
                                   float previous_x,
                                   float previous_z,
                                   float delta_time);

private:
  struct MotorLimits {
    float max_speed{0.0F};
    float acceleration{0.0F};
    float slew{0.0F};
    float damping{6.0F};
  };

  struct WantedVelocity {
    float vx{0.0F};
    float vz{0.0F};
    bool present{false};
  };

  [[nodiscard]] static auto
  motor_limits(const Engine::Core::Entity& entity,
               const Engine::Core::UnitComponent& unit,
               const Engine::Core::StaminaComponent* stamina) -> MotorLimits;
  [[nodiscard]] static auto
  wanted_velocity(const Mover& mover,
                  const MovementCollision::MotorCollision& collision,
                  float old_x,
                  float old_z) -> WantedVelocity;
  static void drive_along_heading(Engine::Core::TransformComponent& transform,
                                  Engine::Core::MovementComponent& movement,
                                  const Engine::Core::UnitComponent& unit,
                                  float target_vx,
                                  float target_vz,
                                  float delta_time);
  static void accelerate_toward(Mover& mover,
                                const MotorLimits& limits,
                                const WantedVelocity& wanted,
                                bool heading_locked);

  [[nodiscard]] static auto
  sweep_step(Mover& mover,
             const MovementCollision::MotorCollision& collision,
             float old_x,
             float old_z,
             bool heading_locked) -> MovementCollision::SweepResult;
  static void record_motion(Mover& mover,
                            float old_x,
                            float old_z,
                            const MovementCollision::SweepResult& sweep);
};

} // namespace Game::Systems
