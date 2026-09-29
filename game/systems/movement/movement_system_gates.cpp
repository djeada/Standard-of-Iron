#include "movement_system_gates.h"

#include <algorithm>
#include <cmath>

#include "core/component_economy.h"
#include "movement_system_heading.h"
#include "movement_system_motor.h"
#include "order_service.h"
#include "route_follow_system.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "units/spawn_type.h"
#include "util/planar_math.h"

namespace Game::Systems {

using Engine::Core::MovementOrderState;

void MovementSystem::Gates::hold_direct_control(Mover& mover) {
  auto& movement = mover.movement;
  if (auto const* commander =
          mover.world.try_get<Engine::Core::CommanderComponent>(mover.entity.get_id());
      commander != nullptr && commander->fpv_controlled && !commander->jump_active) {
    movement.has_target = false;
    movement.clear_path();
    OrderService::clear_player_order_intent(&mover.entity);
    movement.vx = 0.0F;
    movement.vz = 0.0F;
    mover.facts.progress.state = MovementOrderState::Cancelled;
  }
}

void MovementSystem::Gates::step_hold_mode(Mover& mover) {
  auto& movement = mover.movement;
  auto* hold_mode =
      mover.world.try_get<Engine::Core::HoldModeComponent>(mover.entity.get_id());
  float const delta_time = mover.delta_time;

  bool in_hold_mode = false;
  if (hold_mode != nullptr) {
    if (hold_mode->exit_cooldown > 0.0F) {
      hold_mode->exit_cooldown = std::max(0.0F, hold_mode->exit_cooldown - delta_time);
    }

    if (hold_mode->active) {
      movement.has_target = false;
      movement.clear_path();
      OrderService::clear_player_order_intent(&mover.entity);
      movement.vx = 0.0F;
      movement.vz = 0.0F;
      in_hold_mode = true;

      if (hold_mode->kneel_duration > 0.0F && hold_mode->kneel_entry_progress < 1.0F) {
        hold_mode->kneel_entry_progress = std::min(
            1.0F,
            hold_mode->kneel_entry_progress + delta_time / hold_mode->kneel_duration);
      }
    } else {
      hold_mode->kneel_entry_progress = 0.0F;
    }

    if (hold_mode->exit_cooldown > 0.0F && !in_hold_mode) {
      movement.vx = 0.0F;
      movement.vz = 0.0F;
      return;
    }
  }

  if (in_hold_mode) {
    mover.facts.progress.state = MovementOrderState::Idle;
    if (!mover.world.has<Engine::Core::BuildingComponent>(mover.entity.get_id())) {
      MovementHeading::apply_desired_yaw(
          &mover.transform,
          delta_time,
          MovementHeading::formation_turn_speed_degrees(
              mover.entity,
              mover.unit,
              std::min(MovementHeading::k_hold_mode_turn_speed_degrees,
                       Game::Units::body_turn_speed_degrees(mover.unit.spawn_type))));
    }
  }
}

void MovementSystem::Gates::step_melee_lock(Mover& mover,
                                            const DuelFootwork& footwork) {
  auto& movement = mover.movement;
  auto& transform = mover.transform;
  auto* atk = mover.world.try_get<Engine::Core::AttackComponent>(mover.entity.get_id());
  movement.has_target = false;
  OrderService::clear_player_order_intent(&mover.entity);
  movement.vx = 0.0F;
  movement.vz = 0.0F;
  movement.clear_path();
  mover.facts.progress.state = MovementOrderState::Idle;
  if (atk != nullptr && !footwork.apply(mover, *atk) &&
      !MovementHeading::face_locked_opponent(
          &mover.world, mover.entity, transform, *atk, &mover.unit, mover.delta_time)) {
    transform.desired_yaw = transform.rotation.y;
    transform.has_desired_yaw = false;
  }

  MovementHeading::clamp_to_map_bounds(transform);
  Motor::publish_displacement(
      mover.facts, transform, mover.previous_x, mover.previous_z, mover.delta_time);
}

void MovementSystem::Gates::step_builder_bypass(Mover& mover) {
  auto& movement = mover.movement;
  auto& transform = mover.transform;
  auto& facts = mover.facts;
  float const delta_time = mover.delta_time;
  auto* builder_prod = mover.world.try_get<Engine::Core::BuilderProductionComponent>(
      mover.entity.get_id());
  float const dx = builder_prod->bypass_target_x - transform.position.x;
  float const dz = builder_prod->bypass_target_z - transform.position.z;
  float const dist_sq = dx * dx + dz * dz;

  float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
  float const bypass_step =
      std::max(max_navigation_speed(mover.unit, nullptr) * delta_time, 0.01F);

  auto const* bypass_ground = NavGrid::get_pathfinder();
  if (dist <= bypass_step &&
      (bypass_ground == nullptr ||
       bypass_ground->is_terrain_segment_walkable(
           QVector3D(transform.position.x, 0.0F, transform.position.z),
           QVector3D(
               builder_prod->bypass_target_x, 0.0F, builder_prod->bypass_target_z)))) {
    transform.position.x = builder_prod->bypass_target_x;
    transform.position.z = builder_prod->bypass_target_z;
    builder_prod->bypass_movement_active = false;
    movement.vx = 0.0F;
    movement.vz = 0.0F;
    movement.has_target = false;
    movement.clear_path();
    OrderService::clear_player_order_intent(&mover.entity);
    facts.progress.state = MovementOrderState::Arrived;
  } else {
    float const nx = dx / dist;
    float const nz = dz / dist;
    float const base_speed = max_navigation_speed(mover.unit, nullptr);
    movement.vx = nx * base_speed;
    movement.vz = nz * base_speed;

    QVector3D const here(transform.position.x, 0.0F, transform.position.z);
    QVector3D const next(transform.position.x + movement.vx * delta_time,
                         0.0F,
                         transform.position.z + movement.vz * delta_time);
    if (auto const* ground = NavGrid::get_pathfinder();
        ground != nullptr && !ground->is_terrain_segment_walkable(here, next)) {
      builder_prod->bypass_movement_active = false;
      movement.vx = 0.0F;
      movement.vz = 0.0F;
      movement.has_target = false;
      movement.clear_path();
      facts.progress.state = MovementOrderState::Arrived;
      Motor::publish_displacement(
          facts, transform, mover.previous_x, mover.previous_z, delta_time);
      return;
    }

    transform.position.x += movement.vx * delta_time;
    transform.position.z += movement.vz * delta_time;

    float const target_yaw =
        Game::Systems::yaw_degrees_from_direction(movement.vx, movement.vz);
    float const turn_speed = MovementHeading::formation_turn_speed_degrees(
        mover.entity,
        mover.unit,
        Game::Units::body_turn_speed_degrees(mover.unit.spawn_type));
    transform.rotation.y = Game::Systems::turn_yaw_toward(
        transform.rotation.y, target_yaw, turn_speed * delta_time);
    facts.progress.state = MovementOrderState::Following;
  }

  Motor::publish_displacement(
      facts, transform, mover.previous_x, mover.previous_z, delta_time);
}

} // namespace Game::Systems
