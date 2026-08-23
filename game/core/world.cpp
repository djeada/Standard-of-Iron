#include "world.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <string>
#include <string_view>
#include <typeinfo>

#if defined(__GNUC__) && __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define SOI_HAS_CXA_DEMANGLE 1
#endif
#include <type_traits>
#include <utility>
#include <vector>

#include "../../animation/action_manifest.h"
#include "../../animation/clip_manifest.h"
#include "../formation/unit_layout_state.h"
#include "../systems/builder_product_types.h"
#include "../systems/unit_activity.h"
#include "component.h"
#include "core/entity.h"
#include "core/system.h"
#include "movement_trace.h"

namespace Engine::Core {

namespace {

World::EntityDestroyedHook g_entity_destroyed_hook = nullptr;

} // namespace

void World::set_entity_destroyed_hook(EntityDestroyedHook hook) {
  g_entity_destroyed_hook = hook;
}

namespace {

constexpr float k_motion_displacement_epsilon_sq = 1.0e-6F;
constexpr float k_motion_velocity_epsilon_sq = 1.0e-4F;
constexpr float k_motion_stall_speed = 0.15F;
constexpr float k_motion_stall_grace_seconds = 0.4F;
constexpr float k_motion_no_progress_seconds = 0.6F;
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
  bool stalled{false};
  bool has_component_velocity{false};
  bool has_navigation_intent{false};
  bool direct_control_moving{false};
  bool builder_bypass{false};
  bool has_chase_intent{false};
  bool has_active_navigation_segment{false};
  bool is_running{false};
};

[[nodiscard]] auto resolve_motion_presentation_state(
    const MotionPresentationSample& sample) noexcept -> MotionPresentationState {
  bool const moving = sample.displaced || sample.direct_control_moving ||
                      sample.builder_bypass || sample.has_component_velocity ||
                      (sample.has_chase_intent && sample.has_active_navigation_segment);
  if (!moving || sample.stalled) {
    return MotionPresentationState::Idle;
  }
  return sample.is_running ? MotionPresentationState::Run
                           : MotionPresentationState::Walk;
}

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

// Emitted after presentation so one record carries the whole tick: the order
// state, the route the follower used, the correction steering asked for, what
// the motor actually accepted, and the gait the renderer will be told to play.
void publish_movement_trace_frame(World& world) {
  auto& trace = MovementTrace::instance();
  trace.configure_from_environment();
  if (!trace.enabled()) {
    return;
  }

  world.each<MovementFactsComponent, TransformComponent, UnitComponent>(
      [&world, &trace](EntityID id,
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
          sample.corridor_half_width = motion->traversal_available_half_width;
        }
        sample.command_sequence = facts.route.command_sequence;
        sample.route_revision = facts.route.route_revision;
        sample.topology_revision = facts.route.topology_revision;
        sample.requested_goal_x = facts.route.requested_goal_x;
        sample.requested_goal_z = facts.route.requested_goal_z;
        sample.resolved_goal_x = facts.route.resolved_goal_x;
        sample.resolved_goal_z = facts.route.resolved_goal_z;
        if (movement != nullptr) {
          sample.waypoint_index =
              static_cast<std::uint32_t>(movement->get_path_index());
          sample.waypoint_count =
              static_cast<std::uint32_t>(movement->get_path().size());
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
        sample.nearest_time_to_collision = facts.steering.nearest_time_to_collision;
        sample.passing_side = facts.steering.passing_side;
        sample.solver_result = facts.steering.solver_result;
        sample.queue_owner = facts.steering.queue_owner;
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
        sample.traversal_mode = facts.traversal.mode;
        sample.portal_id = facts.traversal.portal_id;
        sample.current_files = facts.traversal.current_files;
        sample.target_files = facts.traversal.target_files;
        sample.transition_progress = facts.traversal.transition_progress;
        sample.mode_dwell_seconds = facts.traversal.mode_dwell_seconds;
        sample.soldier_body_radius = facts.traversal.soldier_body_radius;
        sample.direction_source = facts.direction_source;
        trace.record(sample);
      });
}

void finalize_motion_presentation_frame(World& world, float delta_time) {
  const float safe_dt = std::max(delta_time, 1.0e-5F);
  world.each<MotionPresentationComponent, TransformComponent, UnitComponent>(
      [&world, delta_time, safe_dt](EntityID id,
                                    MotionPresentationComponent& motion_value,
                                    TransformComponent& transform_value,
                                    UnitComponent& unit_value) {
        Entity* entity_ptr = world.get_entity(id);
        if (entity_ptr == nullptr) {
          return;
        }
        Entity& entity = *entity_ptr;
        auto* motion = &motion_value;
        auto* transform = &transform_value;
        auto* unit = &unit_value;

        auto* movement = entity.get_component<MovementComponent>();
        auto* attack = entity.get_component<AttackComponent>();
        auto* attack_target = entity.get_component<AttackTargetComponent>();
        auto* commander = entity.get_component<CommanderComponent>();
        auto* builder_prod = entity.get_component<BuilderProductionComponent>();
        auto* stamina = entity.get_component<StaminaComponent>();

        float const displacement_x = transform->position.x - motion->previous_x;
        float const displacement_z = transform->position.z - motion->previous_z;
        float const displacement_sq =
            displacement_x * displacement_x + displacement_z * displacement_z;

        // Locomotion is asserted from what the motor accepted, never from what
        // the route follower wanted. A body pressed against a wall has a desired
        // velocity and no accepted one, and used to be told to walk anyway.
        auto* facts = entity.get_component<MovementFactsComponent>();
        bool const motor_published = facts != nullptr && facts->motor.valid;
        float const motion_vx = motor_published       ? facts->motor.accepted_vx
                                : movement != nullptr ? movement->get_vx()
                                                      : 0.0F;
        float const motion_vz = motor_published       ? facts->motor.accepted_vz
                                : movement != nullptr ? movement->get_vz()
                                                      : 0.0F;
        float const movement_speed_sq = motion_vx * motion_vx + motion_vz * motion_vz;
        bool const has_component_velocity =
            movement_speed_sq > k_motion_velocity_epsilon_sq;

        bool const has_active_navigation_segment =
            movement != nullptr &&
            (movement->get_has_target() || movement->has_waypoints());

        bool const melee_footwork = attack != nullptr && attack->in_melee_lock &&
                                    !has_active_navigation_segment &&
                                    !has_component_velocity;
        bool const displaced =
            displacement_sq > k_motion_displacement_epsilon_sq && !melee_footwork;
        bool const has_navigation_intent =
            has_active_navigation_segment || has_component_velocity;

        float const direct_control_speed_sq =
            commander != nullptr && commander->fpv_controlled
                ? (commander->fpv_motion_vx * commander->fpv_motion_vx) +
                      (commander->fpv_motion_vz * commander->fpv_motion_vz)
                : 0.0F;
        bool const direct_control_velocity =
            direct_control_speed_sq > k_motion_velocity_epsilon_sq;
        bool const direct_control_moving =
            commander != nullptr && commander->fpv_controlled &&
            (direct_control_velocity || has_component_velocity);
        bool const builder_bypass =
            builder_prod != nullptr && builder_prod->bypass_movement_active;

        motion->attack_target_in_range =
            attack_target_is_in_range(world, &entity, attack, attack_target, transform);
        motion->has_chase_intent =
            attack_target != nullptr && attack_target->target_id > 0 &&
            attack_target->should_chase && !motion->attack_target_in_range;

        bool const wants_locomotion =
            has_component_velocity || direct_control_moving || builder_bypass ||
            (motion->has_chase_intent && has_active_navigation_segment);
        bool const making_progress =
            displacement_sq >=
                (k_motion_stall_speed * safe_dt) * (k_motion_stall_speed * safe_dt) ||
            direct_control_speed_sq >= k_motion_stall_speed * k_motion_stall_speed;
        if (!wants_locomotion || making_progress) {
          motion->stalled_seconds = 0.0F;
        } else {
          motion->stalled_seconds += std::max(0.0F, delta_time);
        }
        float const no_ground_gained = (movement != nullptr && !direct_control_moving)
                                           ? movement->get_stuck_time()
                                           : 0.0F;

        MotionPresentationSample sample{};
        sample.displaced = displaced;
        sample.stalled = motion->stalled_seconds >= k_motion_stall_grace_seconds ||
                         no_ground_gained >= k_motion_no_progress_seconds;
        sample.has_component_velocity = has_component_velocity;
        sample.has_navigation_intent = has_navigation_intent;
        sample.direct_control_moving = direct_control_moving;
        sample.builder_bypass = builder_bypass;
        sample.has_chase_intent = motion->has_chase_intent;
        sample.has_active_navigation_segment = has_active_navigation_segment;
        sample.is_running = stamina != nullptr && stamina->is_running;

        const MotionPresentationState next_state =
            resolve_motion_presentation_state(sample);
        motion->set_state(next_state);
        motion->state_time = motion->state_changed
                                 ? 0.0F
                                 : motion->state_time + std::max(0.0F, delta_time);
        motion->has_velocity =
            displaced || has_component_velocity || direct_control_velocity;
        motion->has_navigation_intent =
            has_navigation_intent || builder_bypass || direct_control_moving;

        motion->displacement_x = displacement_x;
        motion->displacement_z = displacement_z;
        if (has_component_velocity) {
          motion->velocity_x = motion_vx;
          motion->velocity_z = motion_vz;
          motion->speed = std::sqrt(movement_speed_sq);

          if (displaced) {
            motion->speed =
                std::min(motion->speed, std::sqrt(displacement_sq) / safe_dt);
          }
        } else if (displaced) {
          motion->velocity_x = displacement_x / safe_dt;
          motion->velocity_z = displacement_z / safe_dt;
          motion->speed = std::sqrt(displacement_sq) / safe_dt;
        } else if (direct_control_velocity) {
          motion->velocity_x = commander->fpv_motion_vx;
          motion->velocity_z = commander->fpv_motion_vz;
          motion->speed = std::sqrt(direct_control_speed_sq);
        } else {
          motion->velocity_x = 0.0F;
          motion->velocity_z = 0.0F;
          motion->speed = next_state != MotionPresentationState::Idle
                              ? std::max(0.1F, unit->speed)
                              : 0.0F;
          if (next_state == MotionPresentationState::Run && stamina != nullptr) {
            motion->speed *= StaminaComponent::k_run_speed_multiplier;
          }
        }

        motion->has_movement_target = false;
        if (builder_bypass) {
          motion->movement_target_x = builder_prod->bypass_target_x;
          motion->movement_target_z = builder_prod->bypass_target_z;
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
        } else if (motion->has_chase_intent && attack_target != nullptr) {
          if (auto* target = world.get_entity(attack_target->target_id)) {
            if (auto* target_transform = target->get_component<TransformComponent>()) {
              motion->movement_target_x = target_transform->position.x;
              motion->movement_target_z = target_transform->position.z;
              motion->has_movement_target = true;
            }
          }
        }

        MovementDirectionSource direction_source = MovementDirectionSource::None;
        if (has_component_velocity) {
          motion->direction_x = motion_vx;
          motion->direction_z = motion_vz;
          direction_source = motor_published ? MovementDirectionSource::AcceptedVelocity
                                             : MovementDirectionSource::DesiredVelocity;
        } else if (displaced) {
          motion->direction_x = displacement_x;
          motion->direction_z = displacement_z;
        } else if (direct_control_velocity) {
          motion->direction_x = commander->fpv_motion_vx;
          motion->direction_z = commander->fpv_motion_vz;
          direction_source = MovementDirectionSource::AcceptedVelocity;
        } else if (motion->has_movement_target) {
          motion->direction_x = motion->movement_target_x - transform->position.x;
          motion->direction_z = motion->movement_target_z - transform->position.z;
          direction_source = MovementDirectionSource::RouteTangent;
        } else {
          auto [forward_x, forward_z] = forward_xz_from_yaw(transform->rotation.y);
          motion->direction_x = forward_x;
          motion->direction_z = forward_z;
          direction_source = MovementDirectionSource::BodyForward;
        }
        normalize_xz(motion->direction_x, motion->direction_z);
        if (facts != nullptr) {
          facts->direction_source = direction_source;
        }

        if (direct_control_moving) {
          motion->source = MotionPresentationSource::DirectControl;
        } else if (builder_bypass) {
          motion->source = MotionPresentationSource::BuilderBypass;
        } else if (motion->has_chase_intent) {
          motion->source = MotionPresentationSource::Chase;
        } else if (has_navigation_intent) {
          motion->source = MotionPresentationSource::Navigation;
        } else if (displaced) {
          motion->source = MotionPresentationSource::ForcedDisplacement;
        } else {
          motion->source = MotionPresentationSource::None;
        }

        motion->seconds_since_motion =
            motion->has_locomotion()
                ? 0.0F
                : motion->seconds_since_motion + std::max(0.0F, delta_time);
        motion->snapshot_valid = true;
      });
}

[[nodiscard]] constexpr auto
to_animation_phase(CombatAnimationState phase) noexcept -> Animation::CombatPhase {
  switch (phase) {
  case CombatAnimationState::Idle:
    return Animation::CombatPhase::Idle;
  case CombatAnimationState::Advance:
    return Animation::CombatPhase::Advance;
  case CombatAnimationState::WindUp:
    return Animation::CombatPhase::WindUp;
  case CombatAnimationState::Strike:
    return Animation::CombatPhase::Strike;
  case CombatAnimationState::Impact:
    return Animation::CombatPhase::Impact;
  case CombatAnimationState::Recover:
    return Animation::CombatPhase::Recover;
  case CombatAnimationState::Reposition:
    return Animation::CombatPhase::Reposition;
  }
  return Animation::CombatPhase::Idle;
}

[[nodiscard]] constexpr auto to_animation_family(CombatAttackFamily family) noexcept
    -> Animation::CombatAttackFamily {
  switch (family) {
  case CombatAttackFamily::Sword:
    return Animation::CombatAttackFamily::Sword;
  case CombatAttackFamily::Spear:
    return Animation::CombatAttackFamily::Spear;
  case CombatAttackFamily::Bow:
    return Animation::CombatAttackFamily::Bow;
  case CombatAttackFamily::None:
    return Animation::CombatAttackFamily::None;
  }
  return Animation::CombatAttackFamily::None;
}

auto builder_work_job(std::string_view product_type) -> std::uint8_t {
  using Animation::HumanoidWorkJob;
  if (product_type == Game::Systems::k_builder_product_cut_tree) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Chop);
  }
  if (product_type == Game::Systems::k_builder_product_collect_stone ||
      product_type == Game::Systems::k_builder_product_collect_iron_ore) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Quarry);
  }
  if (product_type == Game::Systems::k_builder_product_harvest_grain) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Reap);
  }
  if (product_type == Game::Systems::k_builder_product_slaughter_sheep) {
    return static_cast<std::uint8_t>(HumanoidWorkJob::Butcher);
  }
  return static_cast<std::uint8_t>(HumanoidWorkJob::Build);
}

auto publish_creature_presentation_entity(Entity* entity, World* world)
    -> CreaturePresentationComponent* {
  if (entity == nullptr) {
    return nullptr;
  }
  auto const* unit = entity->get_component<UnitComponent>();

  auto* presentation = get_or_add_component<CreaturePresentationComponent>(entity);
  if (presentation == nullptr) {
    return nullptr;
  }
  CreaturePresentationComponent next;
  next.snapshot_valid = true;

  auto const* target_ref = entity->get_component<AttackTargetComponent>();
  next.target_id = target_ref != nullptr ? target_ref->target_id : 0U;
  if (next.target_id != 0U && world != nullptr) {
    auto* target = world->get_entity(next.target_id);
    auto const* target_unit =
        target != nullptr ? target->get_component<UnitComponent>() : nullptr;
    next.target_alive = target_unit != nullptr && target_unit->health > 0 &&
                        !target->has_component<PendingRemovalComponent>() &&
                        !target->has_component<DeathAnimationComponent>();
  }

  auto const* death = entity->get_component<DeathAnimationComponent>();
  auto const* builder = entity->get_component<BuilderProductionComponent>();
  auto const* combat = entity->get_component<CombatStateComponent>();
  auto const* attack = entity->get_component<AttackComponent>();
  auto const* hit = entity->get_component<HitFeedbackComponent>();
  auto const* special = entity->get_component<SpecialAttackComponent>();
  bool const uses_rpg_rules =
      (entity->get_component<CommanderComponent>() != nullptr &&
       entity->get_component<CommanderComponent>()->fpv_controlled) ||
      (entity->get_component<RpgHealthComponent>() != nullptr &&
       entity->get_component<RpgHealthComponent>()->active);

  Animation::HumanoidActionSampleInputs action_inputs{};
  if (death != nullptr) {
    action_inputs.death = {
        .active = true,
        .dying = death->state == DeathSequenceState::Dying,
        .state_time = death->state_time,
        .state_duration = death->state_duration,
        .variant = death->sequence_variant,
    };
  }
  std::uint8_t construction_job = 0;
  if (builder != nullptr && builder->in_progress) {
    action_inputs.construction = {
        .active = true,
        .build_time = builder->build_time,
        .time_remaining = builder->time_remaining,
    };
    construction_job = builder_work_job(builder->product_type);
  } else if (auto const* resident =
                 entity->get_component<SettlementResidentComponent>();

             death == nullptr && resident != nullptr && resident->is_labouring()) {

    action_inputs.construction = {
        .active = true,
        .build_time = resident->work_elapsed,
        .time_remaining = 0.0F,
        .cycles_per_second = k_settlement_labour_cycles_per_second,
    };
  }
  if (combat != nullptr) {
    action_inputs.combat = {
        .has_state = true,
        .phase = to_animation_phase(combat->animation_state),
        .phase_time = combat->state_time,
        .phase_duration = combat->state_duration,
        .attack_family = to_animation_family(combat->attack_family),
        .attack_variant = combat->attack_variant,
        .finisher_attack = combat->finisher_attack,
        .attack_offset = combat->attack_offset,
        .fallback_mode_is_melee =
            attack != nullptr &&
            attack->current_mode == AttackComponent::CombatMode::Melee,
    };
  }
  if (attack != nullptr) {
    action_inputs.melee_lock = {
        .in_lock = attack->in_melee_lock,
        .participates = !uses_rpg_rules,
        .fallback_attack_family =
            unit != nullptr ? to_animation_family(resolve_combat_attack_family(
                                  unit->spawn_type, AttackComponent::CombatMode::Melee))
                            : Animation::CombatAttackFamily::None,
    };
  }
  if (hit != nullptr && hit->is_reacting) {
    action_inputs.hit_reaction = {
        .active = true,
        .reaction_time = hit->reaction_time,
        .reaction_duration = hit->reaction_duration > 0.0F
                                 ? hit->reaction_duration
                                 : HitFeedbackComponent::k_reaction_duration,
        .intensity = hit->reaction_intensity,
        .knockback_x = hit->knockback_x,
        .knockback_z = hit->knockback_z,
    };
  }
  if (special != nullptr && special->use_projectile_system &&
      Game::Systems::is_cast_projectile_kind(special->projectile_kind)) {
    action_inputs.cast = {
        .has_projectile_cast = true,
        .projectile_is_fireball =
            special->projectile_kind == Game::Systems::ProjectileKind::Fireball,
    };
  }
  auto const action = Animation::resolve_humanoid_action_sample(action_inputs);
  next.is_attacking = action.is_attacking;
  next.is_melee = action.is_melee;
  next.is_in_melee_lock = action.is_in_melee_lock;
  next.combat_phase =
      combat != nullptr ? combat->animation_state : CombatAnimationState::Idle;
  next.combat_phase_progress = action.combat_phase_progress;
  if (combat != nullptr) {
    next.melee_intent = combat->intent;
    next.melee_intent_valid = true;
  }
  if (auto const* body = entity->get_component<CommanderBodyControlComponent>()) {
    next.melee_rest_x = body->rest_dir_x;
    next.melee_rest_y = body->rest_dir_y;
    next.melee_rest_valid = body->rest_valid;
  }
  next.attack_family =
      combat != nullptr ? combat->attack_family : CombatAttackFamily::None;
  if (action.attack_family != Animation::CombatAttackFamily::None &&
      next.attack_family == CombatAttackFamily::None && unit != nullptr) {
    next.attack_family = resolve_combat_attack_family(
        unit->spawn_type, AttackComponent::CombatMode::Melee);
  }
  next.attack_variant = action.attack_variant;
  next.finisher_attack = action.finisher_attack;
  next.attack_offset = action.attack_offset;
  next.has_attack_offset = action.has_attack_offset;
  next.attack_from_combat_state = action.attack_from_combat_state;
  next.attack_from_melee_lock = action.attack_from_melee_lock;
  next.is_casting = action.is_casting;
  next.cast = action.cast_kind == Animation::CastVisualKind::Fireball
                  ? CreatureCastPresentation::Fireball
                  : CreatureCastPresentation::None;
  next.is_hit_reacting = action.is_hit_reacting;
  next.hit_reaction_intensity = action.hit_reaction_intensity;
  next.hit_reaction_progress = action.hit_reaction_progress;
  next.hit_reaction_kind =
      hit != nullptr ? hit->reaction_kind : HitReactionKind::Flinch;
  next.hit_recoil_x = action.hit_recoil_x;
  next.hit_recoil_z = action.hit_recoil_z;
  next.is_constructing = action.is_constructing;
  next.construction_progress = action.construction_progress;
  next.construction_job = construction_job;

  if (action.is_in_melee_lock) {
    next.is_constructing = false;
    next.construction_progress = 0.0F;
    next.construction_job = 0;
  }
  next.is_dying = action.is_dying;
  next.is_dead = action.is_dead;
  next.death_progress = action.death_progress;
  next.death_variant = action.death_variant;

  auto const* formation = entity->get_component<FormationPresentationComponent>();
  next.allow_full_body_hit_reaction =
      formation == nullptr || formation->allow_full_body_hit_reaction;
  if (!next.allow_full_body_hit_reaction) {
    next.is_hit_reacting = false;
    next.hit_reaction_intensity = 0.0F;
    next.hit_reaction_progress = 0.0F;
    next.hit_recoil_x = 0.0F;
    next.hit_recoil_z = 0.0F;
  }

  auto const* transform = entity->get_component<TransformComponent>();
  auto const* healer = entity->get_component<HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr &&
      !action.is_in_melee_lock) {
    next.is_healing = true;
    next.healing_target_dx = healer->healing_target_x - transform->position.x;
    next.healing_target_dz = healer->healing_target_z - transform->position.z;
  }
  auto const* commander = entity->get_component<CommanderComponent>();
  next.has_commander = commander != nullptr;
  next.fpv_controlled = commander != nullptr && commander->fpv_controlled;
  if (commander != nullptr) {
    next.jump_active = commander->jump_active;
    next.jump_phase = commander->jump_phase;
    next.jump_height_offset = commander->jump_height_offset;
    next.flag_rally_planting = commander->is_flag_rally_planting();
    next.flag_rally_animation_timer = commander->flag_rally_animation_timer;
    next.flag_rally_cost = commander->flag_rally_cost;
  }
  auto const* commander_guard = entity->get_component<CommanderGuardComponent>();
  auto const* formation_mode = entity->get_component<FormationModeComponent>();
  auto const* guard_mode = entity->get_component<GuardModeComponent>();
  auto const* brace = entity->get_component<SpearBraceComponent>();
  next.formation_guard_active = (formation_mode != nullptr && formation_mode->active) ||
                                (guard_mode != nullptr && guard_mode->active);

  auto const* unit_layout = entity->get_component<UnitLayoutStateComponent>();
  next.defensive_layout_locked =
      unit_layout != nullptr &&
      unit_layout->state ==
          static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Defensive) &&
      unit_layout->is_formed();
  next.guard_requested =
      (next.fpv_controlled && commander_guard != nullptr && commander_guard->active) ||
      (unit != nullptr && unit->spawn_type == Game::Units::SpawnType::Knight &&
       next.formation_guard_active) ||
      (brace != nullptr && (brace->requested || brace->active));
  next.activity = Game::Systems::classify_unit_activity(*entity);

  auto const* hold = entity->get_component<HoldModeComponent>();
  if (hold != nullptr) {
    next.hold_requested = hold->active;
    next.hold_exit_requested = !hold->active && hold->exit_cooldown > 0.0F;
    next.hold_entry_progress = hold->kneel_entry_progress;
    next.hold_exit_progress =
        1.0F - hold->exit_cooldown / std::max(hold->stand_up_duration, 1.0e-4F);
    next.hold_enter_duration = hold->kneel_duration;
    next.hold_exit_duration = hold->stand_up_duration;
  }
  auto const* showcase = entity->get_component<ShowcaseRoutineComponent>();
  if (showcase != nullptr) {
    next.showcase_active = showcase->active;
    next.showcase_move = showcase->current_move;
    next.showcase_phase = showcase->phase;
  }
  auto const* authored = entity->get_component<RpgCommanderActionComponent>();
  if (authored != nullptr) {
    next.authored_action_id = authored->combat_action_id;
    next.authored_action_running = authored->action_running;
    next.authored_action_completed = authored->action_completed;
    next.authored_action_phase = authored->normalized_action_time;
    next.authored_action_exchange_outcome = authored->exchange_outcome;
  }
  next.combat_active = next.is_attacking || next.is_hit_reacting || next.is_dying ||
                       next.is_dead || next.target_id != 0U;

  next.revision = presentation->revision;
  bool const changed =
      presentation->snapshot_valid != next.snapshot_valid ||
      presentation->target_id != next.target_id ||
      presentation->target_alive != next.target_alive ||
      presentation->combat_active != next.combat_active ||
      presentation->is_attacking != next.is_attacking ||
      presentation->is_melee != next.is_melee ||
      presentation->combat_phase != next.combat_phase ||
      presentation->is_hit_reacting != next.is_hit_reacting ||
      presentation->hit_reaction_kind != next.hit_reaction_kind ||
      presentation->construction_job != next.construction_job ||
      presentation->is_dying != next.is_dying ||
      presentation->is_dead != next.is_dead ||
      presentation->guard_requested != next.guard_requested ||
      presentation->defensive_layout_locked != next.defensive_layout_locked ||
      presentation->hold_requested != next.hold_requested ||
      presentation->showcase_active != next.showcase_active ||
      presentation->showcase_move != next.showcase_move ||
      presentation->showcase_phase != next.showcase_phase;
  if (changed) {
    ++next.revision;
  }
  *presentation = next;
  return presentation;
}

void publish_creature_presentation_frame(World& world) {
  auto const unit_ids = world.entities_with<UnitComponent>();
  for (EntityID const id : unit_ids) {
    publish_creature_presentation_entity(world.get_entity(id), &world);
  }
}

template <typename ComponentType>
void copy_snapshot_component(const Entity& source, Entity& destination) {
  auto const* component = source.get_component<ComponentType>();
  if (component == nullptr) {
    if (destination.get_component<ComponentType>() != nullptr) {
      destination.remove_component<ComponentType>();
    }
    return;
  }
  static_assert(std::is_copy_constructible_v<ComponentType>);
  static_assert(std::is_copy_assignable_v<ComponentType>);
  if (auto* existing = destination.get_component<ComponentType>()) {
    *existing = *component;
    return;
  }
  destination.add_component<ComponentType>(*component);
}

} // namespace

void copy_authoritative_snapshot_components(const Entity& source, Entity& destination) {
  copy_snapshot_component<TransformComponent>(source, destination);
  copy_snapshot_component<UnitComponent>(source, destination);
  copy_snapshot_component<RenderableComponent>(source, destination);
  copy_snapshot_component<MovementComponent>(source, destination);
  copy_snapshot_component<MovementFactsComponent>(source, destination);
  copy_snapshot_component<BuildingComponent>(source, destination);
  copy_snapshot_component<PendingRemovalComponent>(source, destination);
  copy_snapshot_component<AttackComponent>(source, destination);
  copy_snapshot_component<AttackTargetComponent>(source, destination);
  copy_snapshot_component<CombatStateComponent>(source, destination);
  copy_snapshot_component<FormationContactComponent>(source, destination);
  copy_snapshot_component<WildlifeComponent>(source, destination);
  copy_snapshot_component<BuilderProductionComponent>(source, destination);
  copy_snapshot_component<ProductionComponent>(source, destination);
  copy_snapshot_component<CaptureComponent>(source, destination);
  copy_snapshot_component<CommanderComponent>(source, destination);
  copy_snapshot_component<CommanderAuraBuffComponent>(source, destination);
  copy_snapshot_component<RpgCommanderActionComponent>(source, destination);
  copy_snapshot_component<RpgCommanderTargetComponent>(source, destination);
  copy_snapshot_component<HealerComponent>(source, destination);
  copy_snapshot_component<PatrolComponent>(source, destination);
  copy_snapshot_component<GuardModeComponent>(source, destination);
  copy_snapshot_component<HoldModeComponent>(source, destination);
  copy_snapshot_component<FormationModeComponent>(source, destination);
  copy_snapshot_component<UnitLayoutStateComponent>(source, destination);
  copy_snapshot_component<SpearBraceComponent>(source, destination);
  copy_snapshot_component<StaminaComponent>(source, destination);
  copy_snapshot_component<MoraleComponent>(source, destination);
  copy_snapshot_component<BurningStatusComponent>(source, destination);
  copy_snapshot_component<StaggerComponent>(source, destination);
  copy_snapshot_component<HitFeedbackComponent>(source, destination);
  copy_snapshot_component<WallConstructionSiteComponent>(source, destination);
  copy_snapshot_component<FirePatchComponent>(source, destination);
  copy_snapshot_component<StructureFireComponent>(source, destination);
  copy_snapshot_component<ElephantComponent>(source, destination);
  copy_snapshot_component<ElephantStompImpactComponent>(source, destination);
  copy_snapshot_component<CatapultLoadingComponent>(source, destination);
  copy_snapshot_component<GateComponent>(source, destination);
  copy_snapshot_component<ResourceCarryComponent>(source, destination);
  copy_snapshot_component<FarmComponent>(source, destination);
}

void copy_presentation_snapshot_components(const Entity& source, Entity& destination) {
  copy_snapshot_component<MotionPresentationComponent>(source, destination);
  copy_snapshot_component<CreaturePresentationComponent>(source, destination);
  copy_snapshot_component<FormationRosterPresentationComponent>(source, destination);
  copy_snapshot_component<FormationPresentationComponent>(source, destination);
  copy_snapshot_component<FormationHitPresentationComponent>(source, destination);
  copy_snapshot_component<SoldierCasualtyAnimationComponent>(source, destination);
  copy_snapshot_component<DeathAnimationComponent>(source, destination);
  copy_snapshot_component<ConstructionPreviewComponent>(source, destination);
  copy_snapshot_component<StructureDamagePresentationComponent>(source, destination);
  copy_snapshot_component<RpgContactPresentationComponent>(source, destination);
  copy_snapshot_component<BloodStainComponent>(source, destination);
  copy_snapshot_component<StockpileComponent>(source, destination);
}

void copy_render_components(const Entity& source, Entity& destination) {
  copy_authoritative_snapshot_components(source, destination);
  copy_presentation_snapshot_components(source, destination);

  auto const* motion = source.get_component<MotionPresentationComponent>();
  auto const* formation = source.get_component<FormationPresentationComponent>();
  auto* transform = destination.get_component<TransformComponent>();
  bool const formation_handles_squeeze =
      formation != nullptr && formation->soldiers.size() > 1U;
  if (transform != nullptr && motion != nullptr && motion->traversal_squeeze_active &&
      !formation_handles_squeeze) {
    transform->scale.x *= std::clamp(motion->traversal_lateral_scale, 0.1F, 1.0F);
  }
}

namespace {

void render_hash_combine(std::uint64_t& seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void render_hash_float(std::uint64_t& seed, float value) {
  render_hash_combine(seed, std::bit_cast<std::uint32_t>(value));
}

auto render_entity_is_stable(const Entity& entity) -> bool {
  auto const* movement = entity.get_component<MovementComponent>();
  auto const* motion = entity.get_component<MotionPresentationComponent>();
  auto const* creature = entity.get_component<CreaturePresentationComponent>();
  auto const* target = entity.get_component<AttackTargetComponent>();
  auto const* combat = entity.get_component<CombatStateComponent>();
  auto const* contact = entity.get_component<FormationContactComponent>();
  auto const* casualties = entity.get_component<SoldierCasualtyAnimationComponent>();
  bool const moving = (movement != nullptr &&
                       (movement->get_has_target() || movement->has_waypoints() ||
                        std::hypot(movement->get_vx(), movement->get_vz()) > 0.001F)) ||
                      (motion != nullptr &&
                       (motion->has_locomotion() || motion->traversal_squeeze_active));
  bool const active_creature =
      creature != nullptr &&
      (creature->combat_active || creature->is_constructing || creature->is_healing ||
       creature->is_dying || creature->is_dead || creature->jump_active ||
       creature->flag_rally_planting || creature->authored_action_running);
  bool const active_combat =
      (target != nullptr && target->target_id != 0) ||
      (combat != nullptr && combat->animation_state != CombatAnimationState::Idle) ||
      (contact != nullptr && (contact->in_contact || !contact->fronts.empty())) ||
      (casualties != nullptr && !casualties->entries.empty());
  bool const transient = entity.has_component<PendingRemovalComponent>() ||
                         entity.has_component<DeathAnimationComponent>() ||
                         entity.has_component<BuilderProductionComponent>() ||
                         entity.has_component<ProductionComponent>() ||
                         entity.has_component<CaptureComponent>() ||
                         entity.has_component<CommanderComponent>() ||
                         entity.has_component<CommanderAuraBuffComponent>() ||
                         entity.has_component<RpgCommanderActionComponent>() ||
                         entity.has_component<RpgCommanderTargetComponent>() ||
                         entity.has_component<HealerComponent>() ||
                         entity.has_component<BurningStatusComponent>() ||
                         entity.has_component<StaggerComponent>() ||
                         entity.has_component<HitFeedbackComponent>() ||
                         entity.has_component<FormationHitPresentationComponent>() ||
                         entity.has_component<ConstructionPreviewComponent>() ||
                         entity.has_component<WallConstructionSiteComponent>() ||
                         entity.has_component<StructureDamagePresentationComponent>() ||
                         entity.has_component<RpgContactPresentationComponent>() ||
                         entity.has_component<BloodStainComponent>() ||
                         entity.has_component<FirePatchComponent>() ||
                         entity.has_component<StructureFireComponent>() ||
                         entity.has_component<ElephantStompImpactComponent>() ||
                         entity.has_component<CatapultLoadingComponent>();
  return !moving && !active_creature && !active_combat && !transient;
}

constexpr std::uint64_t k_render_signature_unstable = 0ULL;

auto render_entity_signature(const Entity& entity) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  if (auto const* transform = entity.get_component<TransformComponent>()) {
    render_hash_float(signature, transform->position.x);
    render_hash_float(signature, transform->position.y);
    render_hash_float(signature, transform->position.z);
    render_hash_float(signature, transform->rotation.x);
    render_hash_float(signature, transform->rotation.y);
    render_hash_float(signature, transform->rotation.z);
    render_hash_float(signature, transform->scale.x);
    render_hash_float(signature, transform->scale.y);
    render_hash_float(signature, transform->scale.z);
  }
  if (auto const* unit = entity.get_component<UnitComponent>()) {
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->health));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->max_health));
    render_hash_float(signature, unit->speed);
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->owner_id));
    render_hash_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
    render_hash_combine(
        signature,
        static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
  }
  if (auto const* farm = entity.get_component<FarmComponent>()) {
    render_hash_combine(signature, static_cast<std::uint64_t>(farm->growth_stage()));
  }
  if (auto const* gate = entity.get_component<GateComponent>()) {

    render_hash_float(signature, gate->open_amount);
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->state));
    render_hash_combine(signature, static_cast<std::uint64_t>(gate->manual_mode));
  }
  if (auto const* renderable = entity.get_component<RenderableComponent>()) {
    render_hash_combine(signature, std::hash<std::string>{}(renderable->renderer_id));
    render_hash_combine(signature, renderable->visible ? 1U : 0U);
  }
  if (auto const* layout = entity.get_component<UnitLayoutStateComponent>()) {
    render_hash_combine(signature, layout->state);
    render_hash_combine(signature, layout->phase);
    render_hash_combine(signature, layout->layout_id);
    render_hash_float(signature, layout->transition_progress);
  }
  if (auto const* morale = entity.get_component<MoraleComponent>()) {
    render_hash_float(signature, morale->morale);
    render_hash_float(signature, morale->commander_aura_bonus);
    render_hash_combine(signature, morale->wavering ? 1U : 0U);
    render_hash_combine(signature, morale->routing ? 1U : 0U);
  }
  if (auto const* creature = entity.get_component<CreaturePresentationComponent>()) {
    render_hash_combine(signature, creature->revision);
  }
  if (auto const* formation = entity.get_component<FormationPresentationComponent>()) {
    render_hash_combine(signature, formation->revision);
  }
  if (auto const* roster =
          entity.get_component<FormationRosterPresentationComponent>()) {
    render_hash_combine(signature, roster->revision);
  }
  render_hash_combine(signature, entity.has_component<UnitComponent>() ? 1U : 0U);
  render_hash_combine(signature, entity.has_component<BuildingComponent>() ? 1U : 0U);
  return signature;
}

} // namespace

auto publish_creature_presentation(Entity* entity,
                                   World* world) -> CreaturePresentationComponent* {
  return publish_creature_presentation_entity(entity, world);
}

auto World::HandleTable::bind(EntityID entity_id, Registry* registry) -> Entity* {
  const std::uint32_t index = Handle::index_of(entity_id);
  const std::size_t page_index = index / k_page_size;
  while (m_pages.size() <= page_index) {
    m_pages.push_back(std::make_unique<Page>());
  }
  Entity& handle = (*m_pages[page_index])[index % k_page_size];
  handle = Entity(entity_id, registry);
  return &handle;
}

auto World::HandleTable::find(std::uint32_t index) const -> Entity* {
  const std::size_t page_index = index / k_page_size;
  if (page_index >= m_pages.size()) {
    return nullptr;
  }
  return &(*m_pages[page_index])[index % k_page_size];
}

World::World()
    : World(true, false) {
}

namespace {
std::atomic<std::uint64_t> g_next_world_instance_id{1};
} // namespace

World::World(bool presentation_enabled, bool render_snapshot)
    : m_instance_id(g_next_world_instance_id.fetch_add(1, std::memory_order_relaxed))
    , m_presentation_enabled(presentation_enabled)
    , m_is_render_snapshot(render_snapshot) {
  m_registry.set_component_change_callback([this](EntityID entity_id,
                                                  ComponentTypeId type_id,
                                                  std::type_index component_type,
                                                  bool added) {
    this->on_component_changed(entity_id, type_id, component_type, added);
  });
  if (!m_is_render_snapshot) {
    std::atomic_store_explicit(&m_render_snapshot,
                               std::shared_ptr<World>(new World(false, true)),
                               std::memory_order_release);
  }
}

World::~World() = default;

auto World::resolve(EntityID entity_id) const -> Entity* {
  if (!m_registry.is_alive(entity_id)) {
    return nullptr;
  }
  Entity* handle = m_handles.find(Handle::index_of(entity_id));
  if (handle == nullptr || handle->get_id() != entity_id) {
    return nullptr;
  }
  return handle;
}

void World::on_component_changed(EntityID entity_id,
                                 ComponentTypeId type_id,
                                 std::type_index component_type,
                                 bool added) {
  (void)type_id;
  if (m_component_observers.empty()) {
    return;
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

auto World::collect_entities_with_type(ComponentTypeId type_id,
                                       const std::source_location& where)
    -> std::vector<Entity*> {
  const EntityLock lock(*this);
  ++m_query_counters.collects;
  const std::span<const EntityID> dense = m_registry.entities_with(type_id);
  if (dense.empty()) {
    m_system_profiler.note_collect_call_site(where.file_name(), where.line(), 0);
    return {};
  }

  std::vector<Entity*> result;
  result.reserve(dense.size());
  for (const EntityID id : dense) {
    if (auto* entity = resolve(id)) {
      result.push_back(entity);
    }
  }
  m_query_counters.collected_entities += result.size();
  m_system_profiler.note_collect_call_site(
      where.file_name(), where.line(), result.size());
  return result;
}

auto World::entities_with(ComponentTypeId type_id) const -> std::span<const EntityID> {
  const EntityLock lock(*this);
  return m_registry.entities_with(type_id);
}

void World::resolve_entities_into(std::span<const EntityID> ids,
                                  std::vector<Entity*>& output) const {
  const EntityLock lock(*this);
  output.clear();
  if (output.capacity() < ids.size()) {
    output.reserve(ids.size());
  }
  for (const EntityID id : ids) {
    if (Entity* entity = resolve(id)) {
      output.push_back(entity);
    }
  }
}

auto World::create_entity() -> Entity* {
  const EntityLock lock(*this);
  const EntityID id = m_registry.create_entity();
  return m_handles.bind(id, &m_registry);
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const EntityLock lock(*this);
  const EntityID id = m_registry.create_entity_with_id(entity_id);
  if (id == NULL_ENTITY) {
    return nullptr;
  }
  return m_handles.bind(id, &m_registry);
}

void World::destroy_entity(EntityID entity_id) {
  const EntityLock lock(*this);

  if (m_registry.is_alive(entity_id)) {
    if (!m_is_render_snapshot && g_entity_destroyed_hook != nullptr) {
      g_entity_destroyed_hook(entity_id);
    }
    m_registry.destroy_entity(entity_id);
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const EntityLock lock(*this);

  m_registry.clear();
  m_deferred.clear();
  m_spatial_index.clear();

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const EntityLock lock(*this);
  return resolve(entity_id);
}

auto World::is_alive(EntityID entity_id) const -> bool {
  const EntityLock lock(*this);
  return m_registry.is_alive(entity_id);
}

auto World::entity_count() const -> std::size_t {
  const EntityLock lock(*this);
  return m_registry.entity_count();
}

void World::add_system(std::unique_ptr<System> system) {
  const SystemPhase phase = system != nullptr ? system->phase() : SystemPhase::Combat;
  add_system(std::move(system), phase);
}

void World::add_system(std::unique_ptr<System> system, SystemPhase phase) {
  m_systems.push_back(std::move(system));
  m_system_phases.push_back(phase);
}

auto World::plan_phase_schedule(SystemPhase phase) const
    -> std::vector<std::vector<std::size_t>> {
  std::vector<SystemAccess> declared;
  std::vector<std::size_t> phase_slots;
  for (std::size_t slot = 0; slot < m_systems.size(); ++slot) {
    if (m_system_phases[slot] != phase || m_systems[slot] == nullptr) {
      continue;
    }
    declared.push_back(m_systems[slot]->access());
    phase_slots.push_back(slot);
  }

  auto batches = plan_phase_batches(declared);
  for (auto& batch : batches) {
    for (std::size_t& index : batch) {
      index = phase_slots[index];
    }
  }
  return batches;
}

namespace {

auto demangled_system_name(const std::type_info& type) -> const std::string& {
  static std::map<std::string, std::string> cache;
  const std::string key = type.name();
  auto existing = cache.find(key);
  if (existing != cache.end()) {
    return existing->second;
  }

  std::string readable = key;
#if defined(SOI_HAS_CXA_DEMANGLE)
  int status = 0;
  char* raw = abi::__cxa_demangle(key.c_str(), nullptr, nullptr, &status);
  if (status == 0 && raw != nullptr) {
    readable = raw;
  }
  std::free(raw);
#endif

  const std::size_t last_scope = readable.rfind("::");
  if (last_scope != std::string::npos) {
    readable = readable.substr(last_scope + 2);
  }
  return cache.emplace(key, std::move(readable)).first->second;
}

} // namespace

auto World::system_display_name(const System& system) -> const char* {
  return demangled_system_name(typeid(system)).c_str();
}

auto World::current_query_counters() const -> SystemProfiler::QueryCounters {
  SystemProfiler::QueryCounters counters = m_query_counters;
  const WorldSpatialIndex::Stats& spatial = m_spatial_index.stats();
  counters.spatial_queries = spatial.queries;
  counters.spatial_candidates = spatial.candidates_examined;
  return counters;
}

void World::update(float delta_time) {
  const EntityLock lock(*this);
  ++m_tick_id;
  if (m_presentation_enabled) {
    begin_motion_presentation_frame(*this, delta_time);
  }

  const bool profiling = m_system_profiler.enabled();
  const auto tick_started = std::chrono::steady_clock::now();
  if (profiling) {
    m_system_profiler.begin_tick(m_tick_id, m_registry.entity_count());
  }

  SystemPhase current_phase =
      m_system_phases.empty() ? SystemPhase::Input : m_system_phases.front();

  for (std::size_t slot = 0; slot < m_systems.size(); ++slot) {
    System& system = *m_systems[slot];

    const SystemPhase slot_phase = m_system_phases[slot];
    if (slot_phase != current_phase) {
      m_deferred.apply(*this);
      current_phase = slot_phase;
    }

    if (!profiling) {
      system.update(this, delta_time);
      continue;
    }

    const SystemProfiler::QueryCounters queries_before = current_query_counters();
    const auto started = std::chrono::steady_clock::now();
    system.update(this, delta_time);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    m_system_profiler.record_system(
        slot,
        system_display_name(system),
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()),
        current_query_counters() - queries_before);
  }

  m_deferred.apply(*this);

  if (profiling) {
    m_system_profiler.end_tick(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - tick_started)
            .count()));
  }
  if (m_presentation_enabled) {
    finalize_motion_presentation_frame(*this, delta_time);
    publish_creature_presentation_frame(*this);
  }
  publish_movement_trace_frame(*this);
  if (!m_is_render_snapshot &&
      m_render_snapshots_requested.load(std::memory_order_acquire)) {
    publish_render_snapshot();
  }
}

auto World::acquire_render_snapshot() const -> std::shared_ptr<World> {
  return std::atomic_load_explicit(&m_render_snapshot, std::memory_order_acquire);
}

void World::ensure_render_snapshot() {
  if (m_is_render_snapshot) {
    return;
  }
  request_render_snapshots();
  if (m_render_publish_revision != 0) {
    return;
  }
  const EntityLock lock(*this);
  if (m_render_publish_revision == 0) {
    publish_render_snapshot();
  }
}

void World::publish_render_snapshot() {
  std::size_t const buffer_index = m_next_render_snapshot_buffer;
  m_next_render_snapshot_buffer =
      (m_next_render_snapshot_buffer + 1U) % m_render_snapshot_buffers.size();
  auto& buffer = m_render_snapshot_buffers[buffer_index];
  auto const published = acquire_render_snapshot();
  if (buffer == nullptr || buffer == published || buffer.use_count() > 1) {
    buffer = std::shared_ptr<World>(new World(false, true));
  }
  auto snapshot = buffer;
  snapshot->m_render_unit_ids.clear();
  snapshot->m_render_building_ids.clear();
  snapshot->m_render_other_ids.clear();
  std::size_t const slot_count = m_registry.slot_count();
  if (snapshot->m_render_entity_signatures.size() < slot_count) {
    snapshot->m_render_entity_signatures.resize(slot_count, 0U);
  }
  ++m_render_publish_revision;
  snapshot->m_render_unit_ids.reserve(entities_with<UnitComponent>().size());
  snapshot->m_render_building_ids.reserve(entities_with<BuildingComponent>().size());
  snapshot->m_render_other_ids.reserve(entities_with<RenderableComponent>().size());

  for (std::size_t index = 1; index < slot_count; ++index) {
    EntityID const source_id =
        m_registry.entity_at_index(static_cast<std::uint32_t>(index));
    EntityID const snapshot_id =
        snapshot->m_registry.entity_at_index(static_cast<std::uint32_t>(index));
    if (source_id == NULL_ENTITY) {
      if (snapshot_id != NULL_ENTITY) {
        snapshot->destroy_entity(snapshot_id);
      }
      snapshot->m_render_entity_signatures[index] = 0U;
      continue;
    }
    Entity const& source = *resolve(source_id);
    Entity* destination = snapshot->resolve(source_id);

    bool const stable = render_entity_is_stable(source);
    std::uint64_t signature = k_render_signature_unstable;
    bool reusable = false;
    if (destination != nullptr && stable) {
      signature = render_entity_signature(source);
      if (signature == k_render_signature_unstable) {
        signature = 1ULL;
      }
      reusable = snapshot->m_render_entity_signatures[index] == signature;
    }

    if (!reusable) {
      if (destination == nullptr) {
        if (snapshot_id != NULL_ENTITY) {
          snapshot->destroy_entity(snapshot_id);
        }
        destination = snapshot->create_entity_with_id(source_id);
        if (destination == nullptr) {
          continue;
        }
      }
      copy_render_components(source, *destination);
      snapshot->m_render_entity_signatures[index] = signature;
    }

    if (!destination->has_component<RenderableComponent>() ||
        destination->has_component<PendingRemovalComponent>()) {
      continue;
    }
    if (destination->has_component<UnitComponent>()) {
      snapshot->m_render_unit_ids.push_back(source_id);
    } else if (destination->has_component<BuildingComponent>()) {
      snapshot->m_render_building_ids.push_back(source_id);
    } else {
      snapshot->m_render_other_ids.push_back(source_id);
    }
  }
  std::atomic_store_explicit(
      &m_render_snapshot, std::move(snapshot), std::memory_order_release);
}

auto World::collect_units_matching(int owner_id,
                                   bool owned) const -> std::vector<Entity*> {
  const EntityLock lock(*this);
  const auto unit_ids = m_registry.entities_with<UnitComponent>();
  std::vector<Entity*> result;
  result.reserve(unit_ids.size());
  for (const EntityID id : unit_ids) {
    const auto* unit = m_registry.try_get<UnitComponent>(id);
    if (unit == nullptr || (unit->owner_id == owner_id) != owned) {
      continue;
    }
    if (Entity* entity = resolve(id)) {
      result.push_back(entity);
    }
  }
  return result;
}

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units_matching(owner_id, true);
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  return collect_units_matching(owner_id, false);
}

auto World::get_next_entity_id() const -> EntityID {
  const EntityLock lock(*this);
  return Handle::make(static_cast<std::uint32_t>(m_registry.slot_count()), 0);
}

void World::set_next_entity_id(EntityID next_id) {
  const EntityLock lock(*this);
  m_registry.reserve_indices_below(Handle::index_of(next_id));
}

auto World::add_component_observer(ComponentObserverCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_component_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_entity_destroyed_observer(EntityDestroyedCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_entity_destroyed_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_world_cleared_observer(WorldClearedCallback callback)
    -> ObserverHandle {
  const EntityLock lock(*this);
  const ObserverHandle handle = m_next_observer_handle++;
  m_world_cleared_observers.push_back({handle, std::move(callback)});
  return handle;
}

void World::remove_component_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_component_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_entity_destroyed_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_entity_destroyed_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_world_cleared_observer(ObserverHandle handle) {
  const EntityLock lock(*this);
  std::erase_if(m_world_cleared_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

} // namespace Engine::Core
