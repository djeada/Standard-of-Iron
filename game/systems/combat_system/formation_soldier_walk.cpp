#include "formation_soldier_walk.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "../../map/scatter/world_prop_clearance_index.h"
#include "../../util/planar_math.h"
#include "../navigation/nav_grid.h"
#include "formation_local_frame.h"
#include "formation_soldier_gait.h"
#include "formation_terrain_constraint.h"

namespace Game::Systems::Combat {
namespace {

using Soldier = Engine::Core::FormationSoldierPresentation;

constexpr float k_foreign_push_speed = 1.1F;
constexpr float k_crowd_offset_relax_seconds = 2.5F;
constexpr float k_crowd_offset_max_spacing = 0.9F;
constexpr float k_crowd_offset_settled = 0.005F;
constexpr float k_mounted_crowd_width = 0.75F;
constexpr float k_mounted_crowd_length_ratio = 0.55F;
constexpr float k_authored_velocity_smoothing_seconds = 0.10F;

constexpr float k_slot_settle_distance = 0.12F;
constexpr float k_squared_distance_clear_margin = 1.001F;
constexpr float k_obstacle_catch_up_ratio = 1.25F;
constexpr float k_min_prop_clearance = 0.22F;

constexpr float k_mounted_turn_radius = 1.8F;
constexpr float k_mounted_pivot_degrees = 130.0F;
constexpr float k_mounted_turn_cap_degrees = 150.0F;
constexpr float k_mounted_catch_up_headroom = 1.3F;
constexpr float k_mounted_sidestep_speed = 0.45F;
constexpr float k_mounted_slot_behind_degrees = 100.0F;
constexpr float k_mounted_slot_behind_distance = 2.5F;
constexpr float k_mounted_settle_slot_speed = 0.25F;

constexpr float k_stranded_distance = 2.0F;
constexpr float k_lost_distance = 12.0F;

[[nodiscard]] auto personal_space_of(float spacing) -> float {
  return std::max(0.28F, spacing * 0.72F);
}

struct WalkTiming {
  float dt{0.0F};
  float variation{0.0F};
  float max_speed{0.0F};
  float root_travel{0.0F};
  float run_speed{0.0F};
  float desired_facing{0.0F};
  bool reset{false};
};

struct SlotAim {
  QVector3D destination;
  bool obstructed{false};
  float heading_change{0.0F};
  float target_x{0.0F};
  float target_z{0.0F};
  float slot_velocity_x{0.0F};
  float slot_velocity_z{0.0F};
  float dx{0.0F};
  float dz{0.0F};
  float distance{0.0F};
};

struct CatchDirection {
  float x{0.0F};
  float z{0.0F};
  float crowd_speed{1.0F};
};

struct Steering {
  float desired_x{0.0F};
  float desired_z{0.0F};
  float desired_magnitude{0.0F};
  float travel_yaw{0.0F};
  float facing_target{0.0F};
  float acceleration_step{0.0F};
  bool responding{false};
};

struct StepPlan {
  float x{0.0F};
  float z{0.0F};
  float wanted{0.0F};
};

auto measure_timing(const SlotWalk& walk,
                    const Soldier* previous,
                    const Soldier& soldier) -> WalkTiming {
  WalkTiming timing;
  timing.dt = std::max(0.0F, walk.delta_time);
  timing.desired_facing = walk.actor.rotation.y + soldier.local_yaw;
  timing.variation = hash_unit_float(walk.seed, soldier.slot_index * 97U + 43U);
  timing.max_speed =
      std::max(1.4F,
               std::max(walk.march_speed, walk.squad_speed) *
                   (walk.mounted ? k_mounted_catch_up_headroom : 1.15F)) *
      (0.94F + timing.variation * 0.12F);
  timing.root_travel = std::hypot(walk.actor.position.x - walk.formation.motion_root_x,
                                  walk.actor.position.z - walk.formation.motion_root_z);
  timing.reset =
      previous == nullptr || !previous->alive || !previous->world_motion_valid ||
      !walk.formation.motion_root_valid ||
      timing.root_travel > std::max(12.0F, timing.max_speed * timing.dt * 4.0F);
  timing.run_speed = gait_run_speed(walk.march_speed);
  return timing;
}

void snap_to_slot(const SlotWalk& walk,
                  const Soldier* previous,
                  Soldier& soldier,
                  const QVector3D& destination,
                  const WalkTiming& timing) {
  soldier.world_x = destination.x();
  soldier.world_z = destination.z();
  soldier.world_yaw = timing.desired_facing;
  soldier.world_motion_valid = true;
  if (!timing.reset && timing.dt > 0.0F) {
    soldier.world_velocity_x = (soldier.world_x - previous->world_x) / timing.dt;
    soldier.world_velocity_z = (soldier.world_z - previous->world_z) / timing.dt;
  }
  settle_soldier_gait(
      timing.reset ? nullptr : previous, soldier, timing.dt, timing.run_speed);
}

void inherit_previous_motion(const Soldier& previous, Soldier& soldier) {
  soldier.world_motion_valid = true;
  soldier.world_x = previous.world_x;
  soldier.world_z = previous.world_z;
  soldier.world_yaw = previous.world_yaw;
  soldier.world_velocity_x = previous.world_velocity_x;
  soldier.world_velocity_z = previous.world_velocity_z;
  soldier.turning = previous.turning;
  soldier.turn_response_remaining = previous.turn_response_remaining;
}

auto foreign_push_direction(const SlotWalk& walk,
                            const Soldier& soldier,
                            float personal_space) -> std::pair<float, float> {
  float const crowd_radius =
      walk.mounted ? std::max(k_mounted_crowd_width, personal_space) : personal_space;
  float const along_scale = walk.mounted ? k_mounted_crowd_length_ratio : 1.0F;
  float const heading = soldier.world_yaw * std::numbers::pi_v<float> / 180.0F;
  float const heading_x = std::sin(heading);
  float const heading_z = std::cos(heading);
  float const crowd_radius_sq_clear =
      crowd_radius * crowd_radius * k_squared_distance_clear_margin;
  float push_x = 0.0F;
  float push_z = 0.0F;
  for (const auto& neighbor : walk.foreign_neighbors) {
    float away_x = soldier.world_x - neighbor.x;
    float away_z = soldier.world_z - neighbor.z;
    float const along = (away_x * heading_x) + (away_z * heading_z);
    float const across = (away_x * heading_z) - (away_z * heading_x);
    float const scaled_along = along * along_scale;
    if ((across * across) + (scaled_along * scaled_along) >= crowd_radius_sq_clear) {
      continue;
    }
    float const separation = std::hypot(across, scaled_along);
    if (separation >= crowd_radius) {
      continue;
    }
    if (separation < 0.001F) {
      float const angle = hash_unit_float(walk.seed, soldier.slot_index * 211U + 7U) *
                          2.0F * std::numbers::pi_v<float>;
      away_x = std::cos(angle);
      away_z = std::sin(angle);
    } else {
      float const length = std::hypot(away_x, away_z);
      away_x /= length;
      away_z /= length;
    }
    float const overlap = 1.0F - separation / crowd_radius;
    push_x += away_x * overlap;
    push_z += away_z * overlap;
  }
  float const push = std::hypot(push_x, push_z);
  if (push > 1.0F) {
    push_x /= push;
    push_z /= push;
  }
  return {push_x, push_z};
}

void relax_crowd_offset(const SlotWalk& walk,
                        const Soldier& previous,
                        Soldier& soldier,
                        float personal_space,
                        float dt) {
  auto const [foreign_push_x, foreign_push_z] =
      foreign_push_direction(walk, soldier, personal_space);
  float const relax = std::exp(-dt / k_crowd_offset_relax_seconds);
  float crowd_step_x = previous.crowd_offset_x * (relax - 1.0F) +
                       foreign_push_x * k_foreign_push_speed * dt;
  float crowd_step_z = previous.crowd_offset_z * (relax - 1.0F) +
                       foreign_push_z * k_foreign_push_speed * dt;

  float const crowd_step = std::hypot(crowd_step_x, crowd_step_z);
  float const budget = std::max(0.0F, walk.crowd_step_budget);
  if (crowd_step > budget && crowd_step > 1.0e-6F) {
    crowd_step_x *= budget / crowd_step;
    crowd_step_z *= budget / crowd_step;
  }
  soldier.crowd_offset_x = previous.crowd_offset_x + crowd_step_x;
  soldier.crowd_offset_z = previous.crowd_offset_z + crowd_step_z;
  float const crowd_offset = std::hypot(soldier.crowd_offset_x, soldier.crowd_offset_z);
  float const crowd_limit = walk.spacing * k_crowd_offset_max_spacing;
  if (crowd_offset > crowd_limit && crowd_offset > 0.0001F) {
    soldier.crowd_offset_x *= crowd_limit / crowd_offset;
    soldier.crowd_offset_z *= crowd_limit / crowd_offset;
  }
}

auto prop_clearance_of(const SlotWalk& walk) -> float {
  return std::max(walk.body_radius, k_min_prop_clearance);
}

auto aim_at_slot(const SlotWalk& walk,
                 const Soldier& previous,
                 const Soldier& soldier,
                 QVector3D destination,
                 const WalkTiming& timing,
                 const Pathfinding* pathfinder,
                 const Game::Map::WorldPropClearanceIndex& props) -> SlotAim {
  destination.setX(destination.x() + soldier.crowd_offset_x);
  destination.setZ(destination.z() + soldier.crowd_offset_z);
  QVector3D const slot_destination = destination;
  {
    float x = destination.x();
    float z = destination.z();
    if (props.push_out(x, z, prop_clearance_of(walk))) {
      destination.setX(x);
      destination.setZ(z);
    }
  }
  if (pathfinder != nullptr) {
    pull_onto_terrain(
        *pathfinder, walk.actor.position.x, walk.actor.position.z, destination);
  }

  SlotAim aim;
  aim.obstructed = destination != slot_destination || previous.relocation_blocked;
  aim.heading_change =
      signed_yaw_delta(walk.formation.motion_root_yaw, walk.actor.rotation.y);
  float const dt = timing.dt;
  aim.target_x = destination.x() - walk.actor.position.x;
  aim.target_z = destination.z() - walk.actor.position.z;
  float const heading_rate =
      dt > 0.0F ? aim.heading_change * std::numbers::pi_v<float> / 180.0F / dt : 0.0F;
  aim.slot_velocity_x =
      dt > 0.0F ? (walk.actor.position.x - walk.formation.motion_root_x) / dt +
                      heading_rate * aim.target_z
                : 0.0F;
  aim.slot_velocity_z =
      dt > 0.0F ? (walk.actor.position.z - walk.formation.motion_root_z) / dt -
                      heading_rate * aim.target_x
                : 0.0F;
  aim.dx = destination.x() - soldier.world_x;
  aim.dz = destination.z() - soldier.world_z;
  aim.distance = std::hypot(aim.dx, aim.dz);
  aim.destination = destination;
  return aim;
}

void begin_turn_response(const SlotWalk& walk,
                         Soldier& soldier,
                         const SlotAim& aim,
                         const WalkTiming& timing) {
  if (std::abs(aim.heading_change) > 0.05F && !soldier.turning) {
    float const rear_rank =
        walk.rows > 1 ? static_cast<float>(std::max(0, walk.rows - 1 - soldier.row)) /
                            static_cast<float>(walk.rows - 1)
                      : 0.0F;
    soldier.turn_response_remaining =
        walk.engaged ? 0.0F : 0.035F + timing.variation * 0.14F + rear_rank * 0.20F;
    soldier.turning = true;
  }
  soldier.turn_response_remaining =
      std::max(0.0F, soldier.turn_response_remaining - timing.dt);
}

auto catch_direction(const SlotWalk& walk,
                     const Soldier& soldier,
                     const SlotAim& aim,
                     float personal_space) -> CatchDirection {
  CatchDirection catching;
  catching.x = aim.distance > 0.0001F ? aim.dx / aim.distance : 0.0F;
  catching.z = aim.distance > 0.0001F ? aim.dz / aim.distance : 0.0F;

  float const radial_x = soldier.world_x - walk.actor.position.x;
  float const radial_z = soldier.world_z - walk.actor.position.z;
  float const radius = std::hypot(radial_x, radial_z);
  float const target_radius = std::hypot(aim.target_x, aim.target_z);
  if (!walk.engaged && std::abs(aim.heading_change) > 0.05F && radius > 0.5F &&
      target_radius > 0.5F) {
    float const angle = std::atan2(radial_z * aim.target_x - radial_x * aim.target_z,
                                   radial_x * aim.target_x + radial_z * aim.target_z);
    float const bend = std::clamp((std::abs(angle) - 0.20F) / 0.70F, 0.0F, 1.0F);
    float const sign = angle < 0.0F ? -1.0F : 1.0F;
    float const radial_correction =
        std::clamp((target_radius - radius) / radius, -0.4F, 0.4F);
    catching.x += (sign * radial_z / radius + radial_x / radius * radial_correction -
                   catching.x) *
                  bend;
    catching.z += (-sign * radial_x / radius + radial_z / radius * radial_correction -
                   catching.z) *
                  bend;
  }

  float const personal_space_sq_clear =
      personal_space * personal_space * k_squared_distance_clear_margin;
  for (const auto& neighbor : walk.neighbors) {
    if (!neighbor.alive || !neighbor.world_motion_valid ||
        neighbor.slot_index == soldier.slot_index) {
      continue;
    }
    float const away_x = soldier.world_x - neighbor.world_x;
    float const away_z = soldier.world_z - neighbor.world_z;
    if ((away_x * away_x) + (away_z * away_z) >= personal_space_sq_clear) {
      continue;
    }
    float const separation = std::hypot(away_x, away_z);
    if (separation > 0.001F && separation < personal_space) {
      bool const approaching = catching.x * away_x + catching.z * away_z < 0.0F;
      if (approaching) {
        catching.crowd_speed = std::min(
            catching.crowd_speed,
            std::clamp((separation - personal_space * 0.5F) / (personal_space * 0.5F),
                       0.0F,
                       1.0F));
      }
      float const pressure = std::max(0.0F, 1.0F - separation / personal_space) * 2.5F;
      catching.x += away_x / separation * pressure;
      catching.z += away_z / separation * pressure;

      if (approaching) {
        catching.x -= away_z / separation * pressure * 0.45F;
        catching.z += away_x / separation * pressure * 0.45F;
      }
    }
  }
  float const catch_length = std::hypot(catching.x, catching.z);
  if (catch_length > 0.0001F) {
    catching.x /= catch_length;
    catching.z /= catch_length;
  }
  return catching;
}

auto plan_steering(const SlotWalk& walk,
                   const Soldier& soldier,
                   const SlotAim& aim,
                   const CatchDirection& catching,
                   const WalkTiming& timing) -> Steering {
  float const catch_up_cap =
      std::max(1.0F, walk.march_speed * 0.6F) * (0.94F + timing.variation * 0.12F);
  float const catch_up_speed =
      std::min(catch_up_cap, aim.distance * 4.0F) * catching.crowd_speed;

  Steering steering;
  steering.desired_x = aim.slot_velocity_x + catching.x * catch_up_speed;
  steering.desired_z = aim.slot_velocity_z + catching.z * catch_up_speed;
  steering.desired_magnitude = std::hypot(steering.desired_x, steering.desired_z);
  if (steering.desired_magnitude > timing.max_speed &&
      steering.desired_magnitude > 0.0001F) {
    float const scale = timing.max_speed / steering.desired_magnitude;
    steering.desired_x *= scale;
    steering.desired_z *= scale;
    steering.desired_magnitude = timing.max_speed;
  }
  steering.travel_yaw = steering.desired_magnitude > 0.25F
                            ? std::atan2(steering.desired_x, steering.desired_z) *
                                  180.0F / std::numbers::pi_v<float>
                            : timing.desired_facing;
  steering.facing_target = !walk.engaged && steering.desired_magnitude > 0.25F
                               ? steering.travel_yaw
                               : timing.desired_facing;
  steering.responding = soldier.turn_response_remaining <= 0.0F;
  steering.acceleration_step = std::max(timing.max_speed, steering.desired_magnitude) /
                               (walk.mounted ? 0.45F : 0.28F) * timing.dt;
  return steering;
}

void steer_mounted(Soldier& soldier,
                   const SlotAim& aim,
                   const Steering& steering,
                   const WalkTiming& timing) {
  float const dt = timing.dt;
  if (steering.responding) {
    bool const slot_behind =
        steering.desired_magnitude > 0.25F &&
        std::abs(signed_yaw_delta(timing.desired_facing, steering.travel_yaw)) >
            k_mounted_slot_behind_degrees &&
        aim.distance < k_mounted_slot_behind_distance;
    float const heading_target =
        slot_behind ? timing.desired_facing : steering.facing_target;
    float const ground_speed =
        std::hypot(soldier.world_velocity_x, soldier.world_velocity_z);
    float const arc_rate =
        ground_speed / k_mounted_turn_radius * 180.0F / std::numbers::pi_v<float>;
    float const turn_rate = std::min(k_mounted_turn_cap_degrees,
                                     std::max(k_mounted_pivot_degrees, arc_rate)) *
                            (0.85F + timing.variation * 0.30F);
    soldier.world_yaw =
        turn_yaw_toward(soldier.world_yaw, heading_target, turn_rate * dt);
  }
  float const yaw_radians = soldier.world_yaw * std::numbers::pi_v<float> / 180.0F;
  float const forward_x = std::sin(yaw_radians);
  float const forward_z = std::cos(yaw_radians);
  float const side_x = forward_z;
  float const side_z = -forward_x;
  float forward_speed = std::max(0.0F,
                                 soldier.world_velocity_x * forward_x +
                                     soldier.world_velocity_z * forward_z);
  float side_speed =
      soldier.world_velocity_x * side_x + soldier.world_velocity_z * side_z;
  float const wanted_forward =
      steering.responding
          ? std::max(0.0F,
                     steering.desired_x * forward_x + steering.desired_z * forward_z)
          : forward_speed;
  float const wanted_side =
      steering.responding
          ? std::clamp(steering.desired_x * side_x + steering.desired_z * side_z,
                       -k_mounted_sidestep_speed,
                       k_mounted_sidestep_speed)
          : std::clamp(side_speed, -k_mounted_sidestep_speed, k_mounted_sidestep_speed);
  forward_speed += std::clamp(wanted_forward - forward_speed,
                              -steering.acceleration_step,
                              steering.acceleration_step);
  side_speed += std::clamp(wanted_side - side_speed,
                           -steering.acceleration_step,
                           steering.acceleration_step);
  soldier.world_velocity_x = forward_x * forward_speed + side_x * side_speed;
  soldier.world_velocity_z = forward_z * forward_speed + side_z * side_speed;
}

void steer_on_foot(const SlotWalk& walk,
                   Soldier& soldier,
                   Steering steering,
                   const WalkTiming& timing) {
  if (steering.responding) {
    soldier.world_yaw =
        turn_yaw_toward(soldier.world_yaw,
                        steering.facing_target,
                        (walk.mounted ? 125.0F : 185.0F) *
                            (0.85F + timing.variation * 0.30F) * timing.dt);
  }
  float const alignment =
      std::cos(signed_yaw_delta(soldier.world_yaw, steering.travel_yaw) *
               std::numbers::pi_v<float> / 180.0F);
  float const mobility =
      walk.engaged ? 1.0F : std::clamp((alignment - 0.15F) / 0.85F, 0.0F, 1.0F);
  if (steering.responding) {
    steering.desired_x *= mobility;
    steering.desired_z *= mobility;
  } else {
    steering.desired_x = soldier.world_velocity_x;
    steering.desired_z = soldier.world_velocity_z;
  }
  float const velocity_dx = steering.desired_x - soldier.world_velocity_x;
  float const velocity_dz = steering.desired_z - soldier.world_velocity_z;
  float const velocity_change = std::hypot(velocity_dx, velocity_dz);
  float const velocity_blend =
      velocity_change > 0.0001F
          ? std::min(1.0F, steering.acceleration_step / velocity_change)
          : 1.0F;
  soldier.world_velocity_x += velocity_dx * velocity_blend;
  soldier.world_velocity_z += velocity_dz * velocity_blend;
}

auto plan_step(const SlotWalk& walk,
               const Soldier& soldier,
               const SlotAim& aim,
               const WalkTiming& timing) -> StepPlan {
  StepPlan plan;
  plan.x = soldier.world_velocity_x * timing.dt;
  plan.z = soldier.world_velocity_z * timing.dt;
  if (walk.position_is_authored) {
    plan.x = aim.dx;
    plan.z = aim.dz;
    float const catch_up_limit =
        (aim.obstructed ? 0.0F : timing.root_travel) +
        (timing.max_speed * k_obstacle_catch_up_ratio * timing.dt);
    if (aim.distance > catch_up_limit) {
      plan.x *= catch_up_limit / aim.distance;
      plan.z *= catch_up_limit / aim.distance;
    }
  } else if (walk.foreign_neighbors.empty() &&
             std::hypot(soldier.crowd_offset_x, soldier.crowd_offset_z) <
                 k_crowd_offset_settled &&
             aim.distance > 0.0001F && aim.distance < k_slot_settle_distance &&
             (!walk.mounted || std::hypot(aim.slot_velocity_x, aim.slot_velocity_z) <
                                   k_mounted_settle_slot_speed)) {
    plan.x = aim.dx;
    plan.z = aim.dz;
  }
  plan.wanted = std::hypot(plan.x, plan.z);
  return plan;
}

void constrain_step(const SlotWalk& walk,
                    Soldier& soldier,
                    StepPlan& plan,
                    const Pathfinding* pathfinder,
                    const Game::Map::WorldPropClearanceIndex& props,
                    const WalkTiming& timing) {
  if (!walk.position_is_authored && pathfinder != nullptr &&
      constrain_step_to_ground(
          *pathfinder,
          QVector3D(soldier.world_x, walk.actor.position.y, soldier.world_z),
          plan.x,
          plan.z,
          walk.passability)) {
    soldier.relocation_blocked = true;
  }
  if (pathfinder != nullptr &&
      constrain_step_to_terrain(
          *pathfinder, soldier.world_x, soldier.world_z, plan.x, plan.z)) {
    soldier.relocation_blocked = true;
  }
  float landing_x = soldier.world_x + plan.x;
  float landing_z = soldier.world_z + plan.z;
  if (props.push_out(landing_x, landing_z, prop_clearance_of(walk))) {
    float const correction_x = landing_x - soldier.world_x - plan.x;
    float const correction_z = landing_z - soldier.world_z - plan.z;
    float const correction = std::hypot(correction_x, correction_z);
    float const correction_limit =
        timing.max_speed * k_obstacle_catch_up_ratio * timing.dt;
    float const keep = correction > correction_limit && correction > 0.0001F
                           ? correction_limit / correction
                           : 1.0F;

    bool const onto_slope =
        pathfinder != nullptr &&
        !terrain_walkable_at(*pathfinder,
                             soldier.world_x + plan.x + correction_x * keep,
                             soldier.world_z + plan.z + correction_z * keep);
    if (!onto_slope) {
      plan.x += correction_x * keep;
      plan.z += correction_z * keep;
    }
    soldier.relocation_blocked = true;
  }
}

void land_step(const SlotWalk& walk,
               const Soldier& previous,
               Soldier& soldier,
               StepPlan& plan,
               const SlotAim& aim,
               const Pathfinding* pathfinder,
               float dt) {
  bool const stopped_dead =
      plan.wanted > 1.0e-4F && std::hypot(plan.x, plan.z) < 1.0e-4F;
  bool const stranded = (stopped_dead && aim.distance > k_stranded_distance) ||
                        aim.distance > k_lost_distance;
  bool snapped = false;
  if (dt > 0.0F && stranded &&
      (pathfinder == nullptr ||
       terrain_walkable_at(*pathfinder, aim.destination.x(), aim.destination.z()))) {
    plan.x = aim.dx;
    plan.z = aim.dz;
    snapped = true;
  }
  soldier.world_x += plan.x;
  soldier.world_z += plan.z;
  if (dt > 0.0F) {
    soldier.world_velocity_x = snapped ? 0.0F : plan.x / dt;
    soldier.world_velocity_z = snapped ? 0.0F : plan.z / dt;
    if (walk.position_is_authored) {
      float const blend = std::min(1.0F, dt / k_authored_velocity_smoothing_seconds);
      soldier.world_velocity_x =
          previous.world_velocity_x +
          (soldier.world_velocity_x - previous.world_velocity_x) * blend;
      soldier.world_velocity_z =
          previous.world_velocity_z +
          (soldier.world_velocity_z - previous.world_velocity_z) * blend;
    }
    soldier.angular_speed =
        std::abs(signed_yaw_delta(previous.world_yaw, soldier.world_yaw)) / dt;
  }
}

void write_local_frame(const SlotWalk& walk,
                       const Soldier& previous,
                       Soldier& soldier) {
  auto const local = world_to_local(walk.actor, soldier.world_x, soldier.world_z);
  soldier.local_x = local.first;
  soldier.local_z = local.second;
  soldier.local_yaw = signed_yaw_delta(walk.actor.rotation.y, soldier.world_yaw);
  auto const previous_local =
      world_to_local(walk.actor, previous.world_x, previous.world_z);
  soldier.previous_local_x = previous_local.first;
  soldier.previous_local_z = previous_local.second;
  auto const [relocation_vx, relocation_vz] = world_vector_to_local(
      walk.actor, soldier.world_velocity_x, soldier.world_velocity_z);
  soldier.relocation_velocity_x = relocation_vx;
  soldier.relocation_velocity_z = relocation_vz;
}

void update_reforming_flags(Soldier& soldier,
                            const SlotAim& aim,
                            const StepPlan& plan,
                            const WalkTiming& timing) {
  float const remaining = std::hypot(aim.destination.x() - soldier.world_x,
                                     aim.destination.z() - soldier.world_z);
  float const facing_error =
      std::abs(signed_yaw_delta(soldier.world_yaw, timing.desired_facing));
  soldier.reforming =
      remaining > 0.06F || facing_error > 5.0F || std::hypot(plan.x, plan.z) > 0.001F;
  if (std::abs(aim.heading_change) < 0.05F && remaining < 0.08F &&
      facing_error < 3.0F) {
    soldier.turning = false;
  }
}

} // namespace

auto foreign_gather_radius(float spacing, bool mounted) -> float {
  float const personal_space = personal_space_of(spacing);
  return mounted ? std::max(k_mounted_crowd_width, personal_space) /
                       k_mounted_crowd_length_ratio
                 : personal_space;
}

void walk_formation_slot(const SlotWalk& walk,
                         const Soldier* previous,
                         Soldier& soldier) {
  auto const* pathfinder = NavGrid::get_pathfinder();
  QVector3D destination = local_to_world(walk.actor, soldier.local_x, soldier.local_z);
  if (pathfinder != nullptr) {
    pull_onto_terrain(
        *pathfinder, walk.actor.position.x, walk.actor.position.z, destination);
  }
  WalkTiming const timing = measure_timing(walk, previous, soldier);
  if (timing.reset || walk.external_reform) {
    snap_to_slot(walk, previous, soldier, destination, timing);
    return;
  }

  inherit_previous_motion(*previous, soldier);
  float const personal_space = personal_space_of(walk.spacing);
  relax_crowd_offset(walk, *previous, soldier, personal_space, timing.dt);

  auto const props = Game::Map::shared_world_prop_clearance_index();
  SlotAim const aim =
      aim_at_slot(walk, *previous, soldier, destination, timing, pathfinder, *props);
  begin_turn_response(walk, soldier, aim, timing);

  CatchDirection const catching = catch_direction(walk, soldier, aim, personal_space);
  Steering const steering = plan_steering(walk, soldier, aim, catching, timing);
  if (walk.mounted && !walk.engaged) {
    steer_mounted(soldier, aim, steering, timing);
  } else {
    steer_on_foot(walk, soldier, steering, timing);
  }

  StepPlan plan = plan_step(walk, soldier, aim, timing);
  constrain_step(walk, soldier, plan, pathfinder, *props, timing);
  land_step(walk, *previous, soldier, plan, aim, pathfinder, timing.dt);
  write_local_frame(walk, *previous, soldier);
  update_reforming_flags(soldier, aim, plan, timing);
  settle_soldier_gait(previous, soldier, timing.dt, timing.run_speed);
}

} // namespace Game::Systems::Combat
