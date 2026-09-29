#include "movement_system_motor.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "movement_system_heading.h"
#include "route_follow_system.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/navigation/walkability.h"
#include "units/spawn_type.h"
#include "util/planar_math.h"

namespace Game::Systems {

using MovementCollision::MotorCollision;
using MovementCollision::SweepResult;

namespace {

constexpr float k_motor_ramp_seconds = 0.12F;
constexpr float k_heading_locked_deceleration = 2.6F;
constexpr float k_heading_locked_pivot_degrees = 65.0F;
constexpr float k_heading_locked_idle_speed = 0.05F;

} // namespace

auto MovementSystem::Motor::motor_limits(const Engine::Core::Entity& entity,
                                         const Engine::Core::UnitComponent& unit,
                                         const Engine::Core::StaminaComponent* stamina)
    -> MotorLimits {
  MotorLimits limits;
  limits.max_speed = formation_navigation_speed(entity, unit, stamina);
  limits.acceleration = limits.max_speed * 4.0F;
  limits.slew = limits.max_speed / k_motor_ramp_seconds;
  return limits;
}

void MovementSystem::Motor::drive_along_heading(
    Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const Engine::Core::UnitComponent& unit,
    float target_vx,
    float target_vz,
    float delta_time) {
  float const dt = std::max(0.0F, delta_time);
  float yaw = transform.rotation.y;
  float const forward_x0 = std::sin(yaw * std::numbers::pi_v<float> / 180.0F);
  float const forward_z0 = std::cos(yaw * std::numbers::pi_v<float> / 180.0F);
  float speed = std::max(
      0.0F, (movement.get_vx() * forward_x0) + (movement.get_vz() * forward_z0));

  float const wanted_speed = std::hypot(target_vx, target_vz);
  float target_speed = 0.0F;
  if (wanted_speed > k_heading_locked_idle_speed) {
    float const desired_yaw =
        Game::Systems::yaw_degrees_from_direction(target_vx, target_vz);
    float const radius = Game::Units::min_turn_radius(unit.spawn_type);
    float const arc_rate = radius > 0.0F
                               ? (speed / radius) * 180.0F / std::numbers::pi_v<float>
                               : Game::Units::body_turn_speed_degrees(unit.spawn_type);
    float const yaw_rate = std::min(
        Game::Units::body_turn_speed_degrees(unit.spawn_type),
        std::max(Game::Units::turn_in_place_speed_degrees(unit.spawn_type), arc_rate));
    yaw = Game::Systems::turn_yaw_toward(yaw, desired_yaw, yaw_rate * dt);
    transform.rotation.y = yaw;
    float const error = std::abs(Game::Systems::signed_yaw_delta(yaw, desired_yaw));
    if (error < k_heading_locked_pivot_degrees) {
      target_speed =
          wanted_speed *
          std::max(0.0F, std::cos(error * std::numbers::pi_v<float> / 180.0F));
    }
  }

  float const accelerate =
      std::max(0.1F, Game::Units::body_acceleration(unit.spawn_type));
  if (speed < target_speed) {
    speed = std::min(target_speed, speed + accelerate * dt);
  } else {
    speed = std::max(target_speed, speed - k_heading_locked_deceleration * dt);
  }
  float const yaw_radians = yaw * std::numbers::pi_v<float> / 180.0F;
  movement.set_manual_velocity(std::sin(yaw_radians) * speed,
                               std::cos(yaw_radians) * speed);
}

auto MovementSystem::Motor::wanted_velocity(const Mover& mover,
                                            const MotorCollision& collision,
                                            float old_x,
                                            float old_z) -> WantedVelocity {
  const auto& facts = mover.facts;
  WantedVelocity wanted;
  if (facts.desired.valid && facts.steering.valid) {
    wanted = {facts.steering.velocity_x, facts.steering.velocity_z, true};
  } else if (facts.desired.valid) {
    wanted = {facts.desired.velocity_x, facts.desired.velocity_z, true};
  }

  if (wanted.present && facts.motor.has_contact) {
    float const nx = facts.motor.contact_nx;
    float const nz = facts.motor.contact_nz;
    float const into = (wanted.vx * nx) + (wanted.vz * nz);
    if (into < 0.0F) {
      float const speed = std::hypot(wanted.vx, wanted.vz);
      float const along = std::hypot(wanted.vx - (nx * into), wanted.vz - (nz * into));
      if (along > 1.0e-4F && speed > 1.0e-4F) {
        float const slide_x = (wanted.vx - (nx * into)) * (speed / along);
        float const slide_z = (wanted.vz - (nz * into)) * (speed / along);
        if (collision.step_allowed(old_x,
                                   old_z,
                                   old_x + (slide_x * mover.delta_time),
                                   old_z + (slide_z * mover.delta_time))) {
          wanted.vx = slide_x;
          wanted.vz = slide_z;
        }
      }
    }
  }
  return wanted;
}

void MovementSystem::Motor::accelerate_toward(Mover& mover,
                                              const MotorLimits& limits,
                                              const WantedVelocity& wanted,
                                              bool heading_locked) {
  auto& movement = mover.movement;
  float const delta_time = mover.delta_time;
  if (heading_locked) {
    drive_along_heading(mover.transform,
                        movement,
                        mover.unit,
                        wanted.present ? wanted.vx : 0.0F,
                        wanted.present ? wanted.vz : 0.0F,
                        delta_time);
  } else if (!wanted.present) {
    movement.vx *= std::max(0.0F, 1.0F - limits.damping * delta_time);
    movement.vz *= std::max(0.0F, 1.0F - limits.damping * delta_time);
  } else if (movement.get_issuer_retargets() &&
             !movement.get_following_formation_slot()) {
    movement.vx += (wanted.vx - movement.vx) * limits.acceleration * delta_time;
    movement.vz += (wanted.vz - movement.vz) * limits.acceleration * delta_time;
    movement.vx *= std::max(0.0F, 1.0F - 0.5F * limits.damping * delta_time);
    movement.vz *= std::max(0.0F, 1.0F - 0.5F * limits.damping * delta_time);
  } else {
    float delta_vx = wanted.vx - movement.vx;
    float delta_vz = wanted.vz - movement.vz;
    float const delta_speed = std::hypot(delta_vx, delta_vz);
    float const max_delta = limits.slew * delta_time;
    if (delta_speed > max_delta && delta_speed > 1.0e-6F) {
      delta_vx *= max_delta / delta_speed;
      delta_vz *= max_delta / delta_speed;
    }
    movement.vx += delta_vx;
    movement.vz += delta_vz;
  }
}
auto MovementSystem::Motor::sweep_step(Mover& mover,
                                       const MotorCollision& collision,
                                       float old_x,
                                       float old_z,
                                       bool heading_locked) -> SweepResult {
  auto& movement = mover.movement;
  auto& transform = mover.transform;
  float const delta_time = mover.delta_time;

  float translated_vx = movement.vx;
  float translated_vz = movement.vz;

  float const body_acceleration = Game::Units::body_acceleration(mover.unit.spawn_type);
  if (body_acceleration > 0.0F) {
    float const speed_ceiling =
        mover.facts.last_accepted_speed + body_acceleration * delta_time;
    float const translated_speed = std::hypot(translated_vx, translated_vz);
    if (translated_speed > speed_ceiling && translated_speed > 1.0e-5F) {
      float const scale = speed_ceiling / translated_speed;
      translated_vx *= scale;
      translated_vz *= scale;
    }
  }

  auto const sweep = MovementCollision::sweep_through(
      collision, old_x, old_z, translated_vx * delta_time, translated_vz * delta_time);

  transform.position.x = old_x + sweep.accepted_dx;
  transform.position.z = old_z + sweep.accepted_dz;

  if (sweep.blocked) {
    movement.vx = 0.0F;
    movement.vz = 0.0F;
  } else if (sweep.contact) {
    float const into = movement.vx * sweep.normal_x + movement.vz * sweep.normal_z;
    if (into < 0.0F) {
      movement.vx -= sweep.normal_x * into;
      movement.vz -= sweep.normal_z * into;
    }
    if (heading_locked) {
      float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
      float const forward_x = std::sin(yaw);
      float const forward_z = std::cos(yaw);
      float const along =
          std::max(0.0F, (movement.vx * forward_x) + (movement.vz * forward_z));
      movement.vx = forward_x * along;
      movement.vz = forward_z * along;
    }
  }
  return sweep;
}

void MovementSystem::Motor::record_motion(Mover& mover,
                                          float old_x,
                                          float old_z,
                                          const SweepResult& sweep) {
  auto& movement = mover.movement;
  auto& transform = mover.transform;
  auto& facts = mover.facts;

  float const stepped_x = transform.position.x - old_x;
  float const stepped_z = transform.position.z - old_z;
  movement.travelled += std::sqrt((stepped_x * stepped_x) + (stepped_z * stepped_z));

  constexpr float k_travelled_wrap = 4096.0F;
  if (movement.travelled >= k_travelled_wrap) {
    movement.travelled -= k_travelled_wrap;
  }

  publish_displacement(facts, transform, old_x, old_z, mover.delta_time);
  facts.last_accepted_speed =
      std::hypot(facts.motor.accepted_vx, facts.motor.accepted_vz);
  facts.motor.rejected_dx = sweep.rejected_dx;
  facts.motor.rejected_dz = sweep.rejected_dz;
  facts.motor.accepted_fraction = sweep.accepted_fraction;
  facts.motor.blocked = sweep.blocked;
  facts.motor.has_contact = sweep.contact;
  facts.motor.contact_nx = sweep.normal_x;
  facts.motor.contact_nz = sweep.normal_z;

  QVector3D const settled_pos(transform.position.x, 0.0F, transform.position.z);
  facts.motor.penetration_depth =
      is_movement_point_allowed(settled_pos, mover.entity)
          ? 0.0F
          : Walkability::penetration(settled_pos,
                                     MotorCollision::body_profile(mover.entity));

  if (sweep.blocked) {
    ++facts.progress.blocked_steps;
  } else if (facts.motor.accepted_fraction > 0.5F) {
    facts.progress.blocked_steps = 0;
  }
}
void MovementSystem::Motor::publish_displacement(
    Engine::Core::MovementFactsComponent& facts,
    const Engine::Core::TransformComponent& transform,
    float previous_x,
    float previous_z,
    float delta_time) {
  float const seconds = std::max(1.0e-5F, delta_time);
  facts.motor.valid = true;
  if (delta_time <= 0.0F) {

    return;
  }
  facts.motor.accepted_dx = transform.position.x - previous_x;
  facts.motor.accepted_dz = transform.position.z - previous_z;
  facts.motor.accepted_vx = facts.motor.accepted_dx / seconds;
  facts.motor.accepted_vz = facts.motor.accepted_dz / seconds;
}

void MovementSystem::Motor::drive(Mover& mover) {
  auto& movement = mover.movement;
  auto& transform = mover.transform;
  float const old_x = transform.position.x;
  float const old_z = transform.position.z;
  MotorCollision const collision(
      mover.entity, old_x, old_z, false, movement.get_escape_active());

  auto* stamina =
      mover.world.try_get<Engine::Core::StaminaComponent>(mover.entity.get_id());
  MotorLimits const limits = motor_limits(mover.entity, mover.unit, stamina);

  WantedVelocity const wanted = wanted_velocity(mover, collision, old_x, old_z);
  bool const heading_locked =
      Game::Units::heading_locked_locomotion(mover.unit.spawn_type);
  accelerate_toward(mover, limits, wanted, heading_locked);

  auto const sweep = sweep_step(mover, collision, old_x, old_z, heading_locked);
  record_motion(mover, old_x, old_z, sweep);

  if (MovementHeading::finalize_orientation(mover.world,
                                            &mover.entity,
                                            &mover.transform,
                                            &mover.movement,
                                            &mover.facts,
                                            mover.delta_time)) {
    movement.vx = -movement.vx;
    movement.vz = -movement.vz;
  }
}

} // namespace Game::Systems
