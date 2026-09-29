#include "world_motion_presentation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

#include "component.h"
#include "core/entity.h"
#include "movement_trace.h"
#include "world.h"

namespace Engine::Core {

namespace {

constexpr float k_motion_velocity_epsilon_sq = 1.0e-4F;
constexpr float k_motion_stall_speed = 0.15F;

constexpr float k_motion_stall_gait_seconds = 0.4F;
[[nodiscard]] auto
forward_xz_from_yaw(float yaw_degrees) noexcept -> std::pair<float, float> {
  float const yaw_rad = yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  return {std::sin(yaw_rad), std::cos(yaw_rad)};
}

void normalize_xz(float& x, float& z) noexcept {
  float const len_sq = x * x + z * z;
  if (len_sq <= 1.0e-8F) {
    x = 0.0F;
    z = 1.0F;
    return;
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  x *= inv_len;
  z *= inv_len;
}

[[nodiscard]] auto
attack_target_is_in_range(World& world,
                          const Entity* attacker_entity,
                          const AttackComponent* attack,
                          const AttackTargetComponent* attack_target,
                          const TransformComponent* transform) -> bool {
  if (attack == nullptr || attack_target == nullptr || attack_target->target_id == 0 ||
      transform == nullptr) {
    return false;
  }

  auto* target = world.get_entity(attack_target->target_id);
  if (target == nullptr) {
    return false;
  }
  if (attack->current_mode == AttackComponent::CombatMode::Melee &&
      attacker_entity != nullptr) {

    auto const* contact = attacker_entity->get_component<FormationContactComponent>();
    if (contact != nullptr) {
      return contact->target_id == attack_target->target_id && contact->in_contact;
    }
  }
  auto* target_transform = target->get_component<TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  float const dx = target_transform->position.x - transform->position.x;
  float const dz = target_transform->position.z - transform->position.z;
  float const dist_sq = dx * dx + dz * dz;
  float target_radius =
      std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
  if (auto* elephant = target->get_component<ElephantComponent>()) {
    target_radius = std::max(target_radius, elephant->trample_radius);
  }
  float const effective_range = attack->range + target_radius + 0.25F;
  return dist_sq <= effective_range * effective_range;
}

struct MotionPresentationSample {
  bool displaced{false};
  bool has_component_velocity{false};

  bool direct_control_velocity{false};

  bool wants_locomotion{false};
  bool is_running{false};
  bool forced_displacement{false};
  MovementOrderState order_state{MovementOrderState::Idle};
};

[[nodiscard]] auto resolve_motion_presentation_state(
    const MotionPresentationSample& sample) noexcept -> MotionPresentationState {
  if (!sample.displaced && !sample.has_component_velocity &&
      !sample.direct_control_velocity) {
    switch (sample.order_state) {
    case MovementOrderState::Turning:
      return MotionPresentationState::Turning;
    case MovementOrderState::LocallyBlocked:
    case MovementOrderState::Yielding:
      return MotionPresentationState::Yielding;
    case MovementOrderState::Repathing:
    case MovementOrderState::Recovering:
      return MotionPresentationState::Recovering;
    default:

      if (!sample.wants_locomotion) {
        return MotionPresentationState::Idle;
      }
      break;
    }
  }
  if (sample.forced_displacement) {
    return MotionPresentationState::ForcedDisplacement;
  }
  return sample.is_running ? MotionPresentationState::Run
                           : MotionPresentationState::Walk;
}

struct MotionParts {
  Entity* entity{nullptr};
  TransformComponent* transform{nullptr};
  MotionPresentationComponent* motion{nullptr};
  MovementComponent* movement{nullptr};
  AttackComponent* attack{nullptr};
  AttackTargetComponent* attack_target{nullptr};
  CommanderComponent* commander{nullptr};
  BuilderProductionComponent* builder_prod{nullptr};
  StaminaComponent* stamina{nullptr};
  MovementFactsComponent* facts{nullptr};
};

struct MotionObservation {
  bool motor_published{false};
  float displacement_x{0.0F};
  float displacement_z{0.0F};
  float displacement_sq{0.0F};
  float motion_vx{0.0F};
  float motion_vz{0.0F};
  float movement_speed_sq{0.0F};
  bool has_component_velocity{false};
  bool has_active_navigation_segment{false};
  bool displaced{false};
  bool has_navigation_intent{false};
  float direct_control_speed_sq{0.0F};
  bool direct_control_velocity{false};
  bool direct_control_moving{false};
  bool builder_bypass{false};
};

[[nodiscard]] auto
gather_motion_parts(Entity& entity,
                    TransformComponent& transform,
                    MotionPresentationComponent& motion) -> MotionParts {
  MotionParts parts;
  parts.entity = &entity;
  parts.transform = &transform;
  parts.motion = &motion;
  parts.movement = entity.get_component<MovementComponent>();
  parts.attack = entity.get_component<AttackComponent>();
  parts.attack_target = entity.get_component<AttackTargetComponent>();
  parts.commander = entity.get_component<CommanderComponent>();
  parts.builder_prod = entity.get_component<BuilderProductionComponent>();
  parts.stamina = entity.get_component<StaminaComponent>();
  parts.facts = entity.get_component<MovementFactsComponent>();
  return parts;
}

[[nodiscard]] auto observe_motion(const MotionParts& parts,
                                  float safe_dt) -> MotionObservation {
  MotionObservation obs;
  auto const* motion = parts.motion;
  auto const* transform = parts.transform;
  auto const* facts = parts.facts;
  auto const* commander = parts.commander;
  auto const* movement = parts.movement;

  float const observed_displacement_x = transform->position.x - motion->previous_x;
  float const observed_displacement_z = transform->position.z - motion->previous_z;

  obs.motor_published = facts != nullptr && facts->motor.valid;
  obs.displacement_x =
      obs.motor_published ? facts->motor.accepted_dx : observed_displacement_x;
  obs.displacement_z =
      obs.motor_published ? facts->motor.accepted_dz : observed_displacement_z;
  obs.displacement_sq =
      obs.displacement_x * obs.displacement_x + obs.displacement_z * obs.displacement_z;
  obs.motion_vx = obs.motor_published ? facts->motor.accepted_vx : 0.0F;
  obs.motion_vz = obs.motor_published ? facts->motor.accepted_vz : 0.0F;
  obs.movement_speed_sq = obs.motion_vx * obs.motion_vx + obs.motion_vz * obs.motion_vz;
  bool const direct_control_commander =
      commander != nullptr && commander->fpv_controlled;
  obs.has_component_velocity =
      direct_control_commander
          ? obs.movement_speed_sq >=
                CommanderComponent::k_direct_control_gait_floor_speed *
                    CommanderComponent::k_direct_control_gait_floor_speed
          : obs.movement_speed_sq > k_motion_velocity_epsilon_sq;

  obs.has_active_navigation_segment =
      movement != nullptr && (movement->get_has_target() || movement->has_waypoints());

  bool const melee_footwork = parts.attack != nullptr && parts.attack->in_melee_lock &&
                              !obs.has_active_navigation_segment &&
                              !obs.has_component_velocity;

  float const gait_displacement_floor =
      (direct_control_commander ? CommanderComponent::k_direct_control_gait_floor_speed
                                : k_motion_stall_speed) *
      safe_dt;
  obs.displaced =
      obs.displacement_sq > (gait_displacement_floor * gait_displacement_floor) &&
      !melee_footwork;
  obs.has_navigation_intent =
      obs.has_active_navigation_segment || obs.has_component_velocity;

  obs.direct_control_speed_sq =
      commander != nullptr && commander->fpv_controlled
          ? (commander->fpv_motion_vx * commander->fpv_motion_vx) +
                (commander->fpv_motion_vz * commander->fpv_motion_vz)
          : 0.0F;
  obs.direct_control_velocity =
      obs.direct_control_speed_sq > k_motion_velocity_epsilon_sq;
  bool const direct_control_gait =
      obs.direct_control_speed_sq >=
      CommanderComponent::k_direct_control_gait_floor_speed *
          CommanderComponent::k_direct_control_gait_floor_speed;
  obs.direct_control_moving = commander != nullptr && commander->fpv_controlled &&
                              (direct_control_gait || obs.has_component_velocity ||
                               commander->fpv_motion_requested);
  obs.builder_bypass =
      parts.builder_prod != nullptr && parts.builder_prod->bypass_movement_active;
  return obs;
}

void update_chase_intent(World& world, const MotionParts& parts) {
  auto* motion = parts.motion;
  motion->attack_target_in_range = attack_target_is_in_range(
      world, parts.entity, parts.attack, parts.attack_target, parts.transform);
  motion->has_chase_intent =
      parts.attack_target != nullptr && parts.attack_target->target_id > 0 &&
      parts.attack_target->should_chase && !motion->attack_target_in_range;
}

[[nodiscard]] auto classify_motion_state(const MotionParts& parts,
                                         const MotionObservation& obs,
                                         float delta_time,
                                         float safe_dt) -> MotionPresentationState {
  auto* motion = parts.motion;
  bool const wants_locomotion =
      obs.has_component_velocity || obs.direct_control_moving || obs.builder_bypass ||
      (motion->has_chase_intent && obs.has_active_navigation_segment);
  bool const making_progress =
      obs.displacement_sq >=
          (k_motion_stall_speed * safe_dt) * (k_motion_stall_speed * safe_dt) ||
      obs.direct_control_speed_sq >= k_motion_stall_speed * k_motion_stall_speed;
  if (!wants_locomotion || making_progress) {
    motion->stalled_seconds = 0.0F;
  } else {
    motion->stalled_seconds += std::max(0.0F, delta_time);
  }
  MotionPresentationSample sample{};
  sample.displaced = obs.displaced;
  sample.has_component_velocity = obs.has_component_velocity;

  sample.direct_control_velocity =
      obs.direct_control_moving && obs.direct_control_velocity;
  sample.wants_locomotion = wants_locomotion;
  sample.is_running = parts.stamina != nullptr && parts.stamina->is_running;
  sample.forced_displacement = obs.displaced && !obs.motor_published;
  sample.order_state =
      parts.facts != nullptr ? parts.facts->progress.state : MovementOrderState::Idle;

  MotionPresentationState next_state = resolve_motion_presentation_state(sample);
  if (motion->stalled_seconds > k_motion_stall_gait_seconds &&
      (next_state == MotionPresentationState::Walk ||
       next_state == MotionPresentationState::Run)) {
    next_state = MotionPresentationState::Yielding;
  }
  return next_state;
}

void apply_motion_state(const MotionParts& parts,
                        const MotionObservation& obs,
                        MotionPresentationState next_state,
                        float delta_time) {
  auto* motion = parts.motion;
  motion->set_state(next_state);
  motion->state_time =
      motion->state_changed ? 0.0F : motion->state_time + std::max(0.0F, delta_time);
  motion->has_velocity =
      obs.displaced || obs.has_component_velocity || obs.direct_control_velocity;
  motion->has_navigation_intent =
      obs.has_navigation_intent || obs.builder_bypass || obs.direct_control_moving;
}

void apply_motion_velocity(const MotionParts& parts,
                           const MotionObservation& obs,
                           float safe_dt) {
  auto* motion = parts.motion;
  motion->displacement_x = obs.displacement_x;
  motion->displacement_z = obs.displacement_z;
  if (obs.has_component_velocity) {
    motion->velocity_x = obs.motion_vx;
    motion->velocity_z = obs.motion_vz;
    motion->speed = std::sqrt(obs.movement_speed_sq);

    if (obs.displaced) {
      motion->speed = std::min(motion->speed, std::sqrt(obs.displacement_sq) / safe_dt);
    }
  } else if (obs.displaced) {
    motion->velocity_x = obs.displacement_x / safe_dt;
    motion->velocity_z = obs.displacement_z / safe_dt;
    motion->speed = std::sqrt(obs.displacement_sq) / safe_dt;
  } else if (obs.direct_control_velocity) {
    motion->velocity_x = parts.commander->fpv_motion_vx;
    motion->velocity_z = parts.commander->fpv_motion_vz;
    motion->speed = std::sqrt(obs.direct_control_speed_sq);
  } else {
    motion->velocity_x = 0.0F;
    motion->velocity_z = 0.0F;
    motion->speed = 0.0F;
  }
}

void apply_motion_target(World& world,
                         const MotionParts& parts,
                         const MotionObservation& obs) {
  auto* motion = parts.motion;
  auto const* movement = parts.movement;
  motion->has_movement_target = false;
  if (obs.builder_bypass) {
    motion->movement_target_x = parts.builder_prod->bypass_target_x;
    motion->movement_target_z = parts.builder_prod->bypass_target_z;
    motion->has_movement_target = true;
  } else if (movement != nullptr && movement->has_waypoints()) {
    auto const& waypoint = movement->current_waypoint();
    motion->movement_target_x = waypoint.first;
    motion->movement_target_z = waypoint.second;
    motion->has_movement_target = true;
  } else if (movement != nullptr && movement->get_has_target()) {
    motion->movement_target_x = movement->get_target_x();
    motion->movement_target_z = movement->get_target_y();
    motion->has_movement_target = true;
  } else if (motion->has_chase_intent && parts.attack_target != nullptr) {
    if (auto* target = world.get_entity(parts.attack_target->target_id)) {
      if (auto* target_transform = target->get_component<TransformComponent>()) {
        motion->movement_target_x = target_transform->position.x;
        motion->movement_target_z = target_transform->position.z;
        motion->has_movement_target = true;
      }
    }
  }
}

void apply_motion_direction(const MotionParts& parts,
                            const MotionObservation& obs,
                            MotionPresentationState next_state) {
  auto* motion = parts.motion;
  auto* facts = parts.facts;
  MovementDirectionSource direction_source = MovementDirectionSource::None;
  if (obs.has_component_velocity) {
    motion->direction_x = obs.motion_vx;
    motion->direction_z = obs.motion_vz;
    direction_source = obs.motor_published ? MovementDirectionSource::AcceptedVelocity
                                           : MovementDirectionSource::DesiredVelocity;
  } else if (obs.displaced) {
    motion->direction_x = obs.displacement_x;
    motion->direction_z = obs.displacement_z;
  } else if (obs.direct_control_velocity) {
    motion->direction_x = parts.commander->fpv_motion_vx;
    motion->direction_z = parts.commander->fpv_motion_vz;
    direction_source = MovementDirectionSource::AcceptedVelocity;
  } else if (next_state == MotionPresentationState::Turning && facts != nullptr &&
             facts->desired.valid) {
    motion->direction_x = facts->desired.tangent_x;
    motion->direction_z = facts->desired.tangent_z;
    direction_source = MovementDirectionSource::RouteTangent;
  } else {
    auto [forward_x, forward_z] = forward_xz_from_yaw(parts.transform->rotation.y);
    motion->direction_x = forward_x;
    motion->direction_z = forward_z;
    direction_source = MovementDirectionSource::BodyForward;
  }
  normalize_xz(motion->direction_x, motion->direction_z);
  if (facts != nullptr) {
    facts->direction_source = direction_source;
  }
}

void apply_motion_source(const MotionParts& parts, const MotionObservation& obs) {
  auto* motion = parts.motion;
  if (obs.direct_control_moving) {
    motion->source = MotionPresentationSource::DirectControl;
  } else if (obs.builder_bypass) {
    motion->source = MotionPresentationSource::BuilderBypass;
  } else if (obs.displaced && !obs.motor_published) {
    motion->source = MotionPresentationSource::ForcedDisplacement;
  } else if (motion->has_chase_intent) {
    motion->source = MotionPresentationSource::Chase;
  } else if (obs.has_navigation_intent) {
    motion->source = MotionPresentationSource::Navigation;
  } else {
    motion->source = MotionPresentationSource::None;
  }
}

void finalize_motion_entity(World& world,
                            Entity& entity,
                            TransformComponent& transform,
                            MotionPresentationComponent& motion,
                            float delta_time,
                            float safe_dt) {
  if (delta_time <= 0.0F && motion.classified) {
    motion.state_changed = false;
    motion.snapshot_valid = true;
    return;
  }
  MotionParts const parts = gather_motion_parts(entity, transform, motion);
  MotionObservation const obs = observe_motion(parts, safe_dt);
  update_chase_intent(world, parts);
  MotionPresentationState const next_state =
      classify_motion_state(parts, obs, delta_time, safe_dt);
  apply_motion_state(parts, obs, next_state, delta_time);
  apply_motion_velocity(parts, obs, safe_dt);
  apply_motion_target(world, parts, obs);
  apply_motion_direction(parts, obs, next_state);
  apply_motion_source(parts, obs);

  motion.seconds_since_motion =
      motion.has_locomotion()
          ? 0.0F
          : motion.seconds_since_motion + std::max(0.0F, delta_time);
  motion.classified = motion.classified || delta_time > 0.0F;
  motion.snapshot_valid = true;
}

void publish_movement_trace_sample(World& world,
                                   MovementTrace& trace,
                                   EntityID id,
                                   MovementFactsComponent& facts,
                                   TransformComponent& transform,
                                   UnitComponent& unit) {
  Entity* entity = world.get_entity(id);
  if (entity == nullptr) {
    return;
  }
  auto const* movement = entity->get_component<MovementComponent>();
  auto const* motion = entity->get_component<MotionPresentationComponent>();

  MovementTroopSample sample;
  sample.session_id = world.instance_id();
  sample.tick = world.tick_id();
  sample.entity_id = id;
  sample.owner_id = unit.owner_id;
  sample.troop_type = static_cast<std::uint8_t>(unit.spawn_type);
  sample.state = facts.progress.state;
  sample.root_x = transform.position.x;
  sample.root_z = transform.position.z;
  sample.root_yaw = transform.rotation.y;
  if (facts.previous_root.valid) {
    sample.previous_root_x = facts.previous_root.x;
    sample.previous_root_z = facts.previous_root.z;
    sample.previous_root_yaw = facts.previous_root.yaw;
  } else if (motion != nullptr) {
    sample.previous_root_x = motion->previous_x;
    sample.previous_root_z = motion->previous_z;
    sample.previous_root_yaw = motion->previous_rotation_y;
  }
  if (motion != nullptr) {
    sample.presentation_valid = world.presentation_enabled();
    sample.presentation_state = static_cast<std::uint8_t>(motion->state);
    sample.presentation_speed = motion->speed;
    sample.presentation_dir_x = motion->direction_x;
    sample.presentation_dir_z = motion->direction_z;
  }
  sample.command_sequence = facts.route.command_sequence;
  sample.route_id = facts.route.route_id;
  sample.route_revision = facts.route.route_revision;
  sample.topology_revision = facts.route.topology_revision;
  sample.lane_offset = facts.route.lane_offset;
  sample.lane_scale = facts.route.lane_scale;
  sample.cohesion_pace = facts.route.cohesion_pace;
  sample.requested_goal_x = facts.route.requested_goal_x;
  sample.requested_goal_z = facts.route.requested_goal_z;
  sample.resolved_goal_x = facts.route.resolved_goal_x;
  sample.resolved_goal_z = facts.route.resolved_goal_z;
  if (movement != nullptr) {
    sample.waypoint_index = static_cast<std::uint32_t>(movement->get_path_index());
    sample.waypoint_count = static_cast<std::uint32_t>(movement->get_path().size());
    sample.waypoint_x = movement->get_target_x();
    sample.waypoint_z = movement->get_target_y();
    sample.envelope_radius = movement->get_navigation_clearance();
  }
  sample.lookahead_x = facts.desired.lookahead_x;
  sample.lookahead_z = facts.desired.lookahead_z;
  sample.tangent_x = facts.desired.tangent_x;
  sample.tangent_z = facts.desired.tangent_z;
  sample.desired_vx = facts.desired.velocity_x;
  sample.desired_vz = facts.desired.velocity_z;
  sample.avoidance_dx = facts.steering.correction_x;
  sample.avoidance_dz = facts.steering.correction_z;
  sample.steered_vx = facts.steering.velocity_x;
  sample.steered_vz = facts.steering.velocity_z;
  sample.neighbor_count = facts.steering.neighbor_count;
  sample.body_overlap = facts.steering.body_overlap;
  sample.solver_result = static_cast<std::uint8_t>(facts.steering.result);
  sample.has_contact = facts.motor.has_contact;
  sample.contact_nx = facts.motor.contact_nx;
  sample.contact_nz = facts.motor.contact_nz;
  sample.accepted_dx = facts.motor.accepted_dx;
  sample.accepted_dz = facts.motor.accepted_dz;
  sample.accepted_vx = facts.motor.accepted_vx;
  sample.accepted_vz = facts.motor.accepted_vz;
  sample.rejected_dx = facts.motor.rejected_dx;
  sample.rejected_dz = facts.motor.rejected_dz;
  sample.penetration_depth = facts.motor.penetration_depth;
  sample.remaining_arclength = facts.progress.remaining_arclength;
  sample.route_advance = facts.progress.route_advance;
  sample.lateral_route_error = facts.progress.lateral_route_error;
  sample.no_progress_seconds = facts.progress.no_progress_seconds;
  sample.order_seconds = facts.progress.order_seconds;
  sample.blocked_steps = facts.progress.blocked_steps;
  sample.repath_count = facts.progress.repath_count;
  sample.repath_reason = facts.progress.repath_reason;
  sample.stalled_seconds = facts.progress.stall.stalled_seconds;
  sample.recovery_rung = facts.progress.stall.rung;
  sample.recovery_attempts = facts.progress.stall.recovery_attempts;
  sample.abandon_count = facts.progress.stall.abandon_count;
  sample.objective_abandoned = facts.progress.stall.objective_abandoned;
  sample.traversal_mode = facts.traversal.mode;
  sample.portal_id = facts.traversal.portal_id;
  sample.current_files = facts.traversal.current_files;
  sample.target_files = facts.traversal.target_files;
  sample.transition_progress = facts.traversal.transition_progress;
  sample.mode_dwell_seconds = facts.traversal.mode_dwell_seconds;
  sample.soldier_body_radius = facts.traversal.soldier_body_radius;
  sample.corridor_half_width = facts.traversal.corridor_half_width;
  sample.formation_half_width = facts.traversal.desired_half_width;
  sample.file_spacing = facts.traversal.file_spacing;
  sample.lateral_scale = facts.traversal.lateral_scale;
  sample.about_faced = facts.traversal.about_faced;
  sample.normal_files = facts.traversal.normal_files;
  sample.direction_source = facts.direction_source;
  trace.record(sample);
}

} // namespace

void begin_motion_presentation_frame(World& world, float delta_time) {
  auto const unit_ids = world.entities_with<UnitComponent>();
  for (EntityID const id : unit_ids) {
    Entity* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* transform = entity->get_component<TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    auto* motion = get_or_add_component<MotionPresentationComponent>(*entity);
    if (motion == nullptr) {
      continue;
    }
    motion->previous_x = transform->position.x;
    motion->previous_y = transform->position.y;
    motion->previous_z = transform->position.z;
    motion->previous_rotation_y = transform->rotation.y;
    motion->tick_delta_time = std::max(0.0F, delta_time);
    motion->snapshot_valid = false;
    motion->initialized = true;
  }
}

void publish_movement_trace_frame(World& world, float delta_time) {
  auto& trace = MovementTrace::instance();
  trace.configure_from_environment();
  if (!trace.enabled()) {
    return;
  }
  trace.set_fixed_step_seconds(delta_time);

  world.each<MovementFactsComponent, TransformComponent, UnitComponent>(
      [&world, &trace](EntityID id,
                       MovementFactsComponent& facts,
                       TransformComponent& transform,
                       UnitComponent& unit) {
        publish_movement_trace_sample(world, trace, id, facts, transform, unit);
      });
}

void finalize_motion_presentation_frame(World& world, float delta_time) {
  const float safe_dt = std::max(delta_time, 1.0e-5F);
  world.each<MotionPresentationComponent, TransformComponent, UnitComponent>(
      [&world, delta_time, safe_dt](EntityID id,
                                    MotionPresentationComponent& motion,
                                    TransformComponent& transform,
                                    UnitComponent&) {
        Entity* entity = world.get_entity(id);
        if (entity == nullptr) {
          return;
        }
        finalize_motion_entity(world, *entity, transform, motion, delta_time, safe_dt);
      });
}

} // namespace Engine::Core
