#include "animation_inputs.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../../game/systems/combat_rules.h"
#include "../../../creature/animation_state_components.h"
#include "../../../entity/registry.h"
#include "../humanoid_constants.h"

namespace Render::GL {

namespace {

constexpr float k_builder_construct_cycles_per_second = 1.75F;
constexpr float k_direct_control_move_speed_sq = 0.01F;
constexpr float k_visual_move_speed_sq = 0.01F;
constexpr float k_visual_move_goal_distance_sq = 0.35F * 0.35F;
constexpr float k_visual_path_request_window = 0.35F;

void reset_humanoid_locomotion_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.locomotion_last_sample_time = 0.0F;
  state.locomotion_phase_bias = 0.0F;
  state.locomotion_cycle_time = 0.0F;
  state.locomotion_phase = 0.0F;
  state.filtered_speed = 0.0F;
  state.filtered_acceleration = 0.0F;
  state.filtered_turn = 0.0F;
  state.locomotion_blend = 0.0F;
  state.run_blend = 0.0F;
  state.locomotion_state = HumanoidMotionState::Idle;
  state.locomotion_initialized = false;
  state.locomotion_was_moving = false;
}

void reset_humanoid_animation_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.idle_duration = 0.0F;
  state.last_sample_time = 0.0F;
  state.initialized = false;
  reset_humanoid_locomotion_state(state);
}

[[nodiscard]] auto
has_direct_control_motion(const Engine::Core::CommanderComponent* commander,
                          const Engine::Core::MovementComponent* movement) -> bool {
  if (commander == nullptr || !commander->fpv_controlled) {
    return false;
  }

  if (movement != nullptr) {
    float const movement_speed_sq =
        movement->vx * movement->vx + movement->vz * movement->vz;
    if (movement_speed_sq > k_direct_control_move_speed_sq) {
      return true;
    }
  }

  float const fpv_speed_sq = commander->fpv_motion_vx * commander->fpv_motion_vx +
                             commander->fpv_motion_vz * commander->fpv_motion_vz;
  return fpv_speed_sq > k_direct_control_move_speed_sq;
}

auto map_combat_state_to_phase(Engine::Core::CombatAnimationState state)
    -> CombatAnimPhase {
  switch (state) {
  case Engine::Core::CombatAnimationState::Advance:
    return CombatAnimPhase::Advance;
  case Engine::Core::CombatAnimationState::WindUp:
    return CombatAnimPhase::WindUp;
  case Engine::Core::CombatAnimationState::Strike:
    return CombatAnimPhase::Strike;
  case Engine::Core::CombatAnimationState::Impact:
    return CombatAnimPhase::Impact;
  case Engine::Core::CombatAnimationState::Recover:
    return CombatAnimPhase::Recover;
  case Engine::Core::CombatAnimationState::Reposition:
    return CombatAnimPhase::Reposition;
  case Engine::Core::CombatAnimationState::Idle:
  default:
    return CombatAnimPhase::Idle;
  }
}

[[nodiscard]] auto combat_state_keeps_attack_pose_while_moving(
    Engine::Core::CombatAnimationState state) noexcept -> bool {
  switch (state) {
  case Engine::Core::CombatAnimationState::WindUp:
  case Engine::Core::CombatAnimationState::Strike:
  case Engine::Core::CombatAnimationState::Impact:
    return true;
  case Engine::Core::CombatAnimationState::Advance:
  case Engine::Core::CombatAnimationState::Recover:
  case Engine::Core::CombatAnimationState::Reposition:
  case Engine::Core::CombatAnimationState::Idle:
  default:
    return false;
  }
}

[[nodiscard]] auto
movement_speed_sq(const Engine::Core::MovementComponent* movement) noexcept -> float {
  if (movement == nullptr) {
    return 0.0F;
  }
  return movement->vx * movement->vx + movement->vz * movement->vz;
}

[[nodiscard]] auto forward_from_transform(
    const Engine::Core::TransformComponent* transform) noexcept -> QVector3D {
  if (transform == nullptr) {
    return {0.0F, 0.0F, 1.0F};
  }

  float const yaw_rad = qDegreesToRadians(transform->rotation.y);
  QVector3D const forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  if (forward.lengthSquared() <= 1.0e-6F) {
    return {0.0F, 0.0F, 1.0F};
  }
  return forward.normalized();
}

[[nodiscard]] auto attack_target_is_in_range(
    const DrawContext& ctx,
    const Engine::Core::AttackComponent* attack,
    const Engine::Core::AttackTargetComponent* attack_target,
    const Engine::Core::TransformComponent* transform) noexcept -> bool {
  if (ctx.world == nullptr || attack == nullptr || attack_target == nullptr ||
      attack_target->target_id == 0 || transform == nullptr) {
    return false;
  }

  auto* target = ctx.world->get_entity(attack_target->target_id);
  if (target == nullptr) {
    return false;
  }

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  float const dx = target_transform->position.x - transform->position.x;
  float const dz = target_transform->position.z - transform->position.z;
  float const dist_squared = dx * dx + dz * dz;
  float target_radius =
      std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F;
  auto* elephant = target->get_component<Engine::Core::ElephantComponent>();
  if (elephant != nullptr) {
    target_radius = std::max(target_radius, elephant->trample_radius);
  }

  float const effective_range = attack->range + target_radius + 0.25F;
  return dist_squared <= effective_range * effective_range;
}

[[nodiscard]] auto movement_goal_is_active(
    const Engine::Core::MovementComponent* movement,
    const Engine::Core::TransformComponent* transform) noexcept -> bool {
  if (movement == nullptr || transform == nullptr) {
    return false;
  }

  QVector3D const current(transform->position.x, 0.0F, transform->position.z);
  QVector3D const goal(movement->goal_x, 0.0F, movement->goal_y);
  bool const goal_is_meaningful =
      (goal - current).lengthSquared() > k_visual_move_goal_distance_sq;
  if (!goal_is_meaningful) {
    return false;
  }

  bool const has_recent_request =
      movement->time_since_last_path_request < k_visual_path_request_window &&
      (movement->last_goal_x != 0.0F || movement->last_goal_y != 0.0F);
  return movement->repath_cooldown > 0.0F || has_recent_request ||
         movement->time_stuck > 0.0F || movement->unstuck_cooldown > 0.0F;
}

[[nodiscard]] auto synthesize_visual_movement_from_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;

  auto* transform = (ctx.entity != nullptr)
                        ? ctx.entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  QVector3D const forward = forward_from_transform(transform);
  state.locomotion_direction = forward;
  state.is_moving = anim.is_moving;
  state.has_navigation_intent = anim.is_moving;

  if (!anim.is_moving) {
    return state;
  }

  auto* unit = (ctx.entity != nullptr)
                   ? ctx.entity->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
  if (unit != nullptr) {
    state.speed_hint = std::max(0.1F, unit->speed);
    if (anim.is_running) {
      state.speed_hint *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
    }
  } else {
    state.speed_hint = anim.is_running ? k_reference_run_speed : k_reference_walk_speed;
  }

  return state;
}

[[nodiscard]] auto visual_movement_from_motion_snapshot(
    const DrawContext& ctx,
    const Engine::Core::MotionPresentationComponent& motion) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;
  state.has_velocity = motion.has_velocity;
  state.has_navigation_intent = motion.has_navigation_intent;
  state.has_chase_intent = motion.has_chase_intent;
  state.attack_target_in_range = motion.attack_target_in_range;
  state.is_moving = motion.is_moving;
  state.has_movement_target = motion.has_movement_target;
  state.movement_target =
      motion.has_movement_target
          ? QVector3D(motion.movement_target_x, 0.0F, motion.movement_target_z)
          : QVector3D(0.0F, 0.0F, 0.0F);
  state.locomotion_direction = QVector3D(motion.direction_x, 0.0F, motion.direction_z);
  if (state.locomotion_direction.lengthSquared() <= 1.0e-6F) {
    auto* transform =
        (ctx.entity != nullptr)
            ? ctx.entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    state.locomotion_direction = forward_from_transform(transform);
  } else {
    state.locomotion_direction.normalize();
  }
  state.speed_hint = motion.speed;
  if (state.is_moving && state.speed_hint <= 0.0F && ctx.entity != nullptr) {
    auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    state.speed_hint = (unit != nullptr) ? std::max(0.1F, unit->speed) : 0.0F;
  }
  return state;
}

} // namespace

auto resolve_visual_movement_state(const DrawContext& ctx) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;
  if (ctx.entity == nullptr) {
    return state;
  }

  // Use the motion snapshot whenever the component has been initialized by at
  // least one begin_motion_presentation_frame call.  snapshot_valid is cleared
  // at the start of each world tick and restored at the end, so it would be
  // false during the race window in which the render thread holds entity_mutex
  // but finalize_motion_presentation_frame has not yet run.  The snapshot
  // fields (is_moving, speed, direction, …) are written only by finalize, which
  // also holds entity_mutex, so they are always safe to read here and always
  // reflect the correct last-finalized state — no data race, no stale idle.
  // The legacy path reads MovementComponent fields directly; those can be
  // written by the movement system without holding entity_mutex, producing a
  // data race that manifests as infantry silently sliding without walk animation.
  if (auto* motion =
          ctx.entity->get_component<Engine::Core::MotionPresentationComponent>();
      motion != nullptr && motion->initialized) {
    return visual_movement_from_motion_snapshot(ctx, *motion);
  }

  auto* movement = ctx.entity->get_component<Engine::Core::MovementComponent>();
  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  auto* attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  auto* attack_target =
      ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
  auto* commander = ctx.entity->get_component<Engine::Core::CommanderComponent>();
  auto* builder_prod =
      ctx.entity->get_component<Engine::Core::BuilderProductionComponent>();
  auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
  auto* stamina = ctx.entity->get_component<Engine::Core::StaminaComponent>();

  QVector3D const forward = forward_from_transform(transform);
  QVector3D target_position = forward;
  bool has_target_position = false;

  float const speed_sq = movement_speed_sq(movement);
  state.has_velocity = speed_sq > k_visual_move_speed_sq;
  state.speed_hint = state.has_velocity ? std::sqrt(speed_sq) : 0.0F;

  if (movement != nullptr) {
    if (movement->has_waypoints()) {
      auto const& waypoint = movement->current_waypoint();
      target_position = QVector3D(waypoint.first, 0.0F, waypoint.second);
      has_target_position = true;
    } else if (movement->has_target) {
      target_position = QVector3D(movement->target_x, 0.0F, movement->target_y);
      has_target_position = true;
    } else if (movement_goal_is_active(movement, transform)) {
      target_position = QVector3D(movement->goal_x, 0.0F, movement->goal_y);
      has_target_position = true;
    }
  }

  if (state.has_velocity && movement != nullptr) {
    state.locomotion_direction =
        QVector3D(movement->vx, 0.0F, movement->vz).normalized();
  } else if (has_target_position && transform != nullptr) {
    QVector3D const current(transform->position.x, 0.0F, transform->position.z);
    QVector3D const delta = target_position - current;
    if (delta.lengthSquared() > 1.0e-6F) {
      state.locomotion_direction = delta.normalized();
    } else {
      state.locomotion_direction = forward;
    }
  } else {
    state.locomotion_direction = forward;
  }

  state.has_movement_target = has_target_position;
  state.movement_target =
      has_target_position ? target_position : QVector3D(0.0F, 0.0F, 0.0F);

  if (builder_prod != nullptr && builder_prod->bypass_movement_active) {
    state.is_moving = true;
    state.has_navigation_intent = true;
    state.has_movement_target = true;
    state.movement_target =
        QVector3D(builder_prod->bypass_target_x, 0.0F, builder_prod->bypass_target_z);
    if (transform != nullptr) {
      QVector3D const current(transform->position.x, 0.0F, transform->position.z);
      QVector3D const delta = state.movement_target - current;
      if (delta.lengthSquared() > 1.0e-6F) {
        state.locomotion_direction = delta.normalized();
      }
    }
    if (state.speed_hint <= 0.0F) {
      state.speed_hint = (unit != nullptr) ? std::max(0.1F, unit->speed) : 0.0F;
    }
    return state;
  }

  state.attack_target_in_range =
      attack_target_is_in_range(ctx, attack, attack_target, transform);
  state.has_chase_intent = (attack_target != nullptr) && attack_target->target_id > 0 &&
                           attack_target->should_chase && !state.attack_target_in_range;

  bool const has_active_navigation_segment =
      (movement != nullptr) &&
      (movement->has_target || movement->has_waypoints() || movement->path_pending ||
       movement->pending_request_id != 0);
  bool const has_navigation_signal = state.has_velocity ||
                                     has_active_navigation_segment ||
                                     movement_goal_is_active(movement, transform);
  state.has_navigation_intent = has_navigation_signal;

  bool const direct_control_moving = has_direct_control_motion(commander, movement);
  bool const suppress_for_attack_range =
      !direct_control_moving && !state.has_velocity && state.attack_target_in_range &&
      !has_active_navigation_segment;

  state.is_moving = direct_control_moving ||
                    (!suppress_for_attack_range &&
                     (state.has_navigation_intent || state.has_chase_intent));

  if (state.has_chase_intent && !state.has_movement_target && ctx.world != nullptr &&
      attack_target != nullptr) {
    auto* target = ctx.world->get_entity(attack_target->target_id);
    auto* target_transform =
        (target != nullptr) ? target->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
    if (target_transform != nullptr) {
      state.has_movement_target = true;
      state.movement_target =
          QVector3D(target_transform->position.x, 0.0F, target_transform->position.z);
      if (transform != nullptr) {
        QVector3D const current(transform->position.x, 0.0F, transform->position.z);
        QVector3D const delta = state.movement_target - current;
        if (delta.lengthSquared() > 1.0e-6F) {
          state.locomotion_direction = delta.normalized();
        }
      }
    }
  }

  if (state.is_moving && state.speed_hint <= 0.0F) {
    state.speed_hint = (unit != nullptr) ? std::max(0.1F, unit->speed) : 0.0F;
    if (stamina != nullptr && stamina->is_running) {
      state.speed_hint *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
    }
  }

  return state;
}

auto visual_movement_for_animation_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState {
  if (anim.visual_movement.is_authoritative) {
    return anim.visual_movement;
  }
  return synthesize_visual_movement_from_inputs(ctx, anim);
}

auto sample_anim_state(const DrawContext& ctx) -> AnimationInputs {
  if (ctx.animation_override != nullptr) {
    AnimationInputs anim = *ctx.animation_override;
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    anim.is_moving = anim.visual_movement.is_moving;
    anim.is_running = anim.is_moving && anim.is_running;
    return anim;
  }
  AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.is_moving = false;
  anim.is_running = false;
  anim.is_attacking = false;
  anim.is_melee = false;
  anim.is_in_hold_mode = false;
  anim.is_exiting_hold = false;
  anim.hold_exit_progress = 0.0F;
  anim.hold_entry_progress = 0.0F;
  anim.combat_phase = CombatAnimPhase::Idle;
  anim.combat_phase_progress = 0.0F;
  anim.attack_family = Engine::Core::CombatAttackFamily::None;
  anim.attack_variant = 0;
  anim.attack_offset = 0.0F;
  anim.has_attack_offset = false;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_dying = false;
  anim.is_dead = false;
  anim.death_progress = 0.0F;
  anim.death_variant = 0;

  if (ctx.entity == nullptr) {
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    return anim;
  }

  auto* death_anim = ctx.entity->get_component<Engine::Core::DeathAnimationComponent>();
  bool const has_active_death = (death_anim != nullptr);
  auto* humanoid_state = Engine::Core::get_or_add_component<
      Render::Creature::HumanoidAnimationStateComponent>(ctx.entity);
  if (ctx.entity->has_component<Engine::Core::PendingRemovalComponent>() &&
      !has_active_death) {
    if (humanoid_state != nullptr && should_persist_animation_state(ctx)) {
      reset_humanoid_animation_state(*humanoid_state);
    }
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    return anim;
  }

  auto* movement = ctx.entity->get_component<Engine::Core::MovementComponent>();
  auto* attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
  auto* attack_target =
      ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  auto* hold_mode = ctx.entity->get_component<Engine::Core::HoldModeComponent>();
  auto* combat_state = ctx.entity->get_component<Engine::Core::CombatStateComponent>();
  auto* hit_feedback = ctx.entity->get_component<Engine::Core::HitFeedbackComponent>();
  auto* commander = ctx.entity->get_component<Engine::Core::CommanderComponent>();
  auto* commander_guard =
      ctx.entity->get_component<Engine::Core::CommanderGuardComponent>();
  const auto* stamina = ctx.entity->get_component<Engine::Core::StaminaComponent>();
  const auto* motion =
      ctx.entity->get_component<Engine::Core::MotionPresentationComponent>();
  VisualMovementState const movement_state = resolve_visual_movement_state(ctx);

  if (death_anim != nullptr) {
    anim.is_moving = false;
    anim.is_running = false;
    anim.is_attacking = false;
    anim.is_melee = false;
    anim.is_hit_reacting = false;
    anim.hit_reaction_intensity = 0.0F;
    anim.death_variant = death_anim->sequence_variant;
    if (death_anim->state == Engine::Core::DeathSequenceState::Dying) {
      anim.is_dying = true;
      if (death_anim->state_duration > 0.0F) {
        anim.death_progress =
            std::clamp(death_anim->state_time / death_anim->state_duration, 0.0F, 1.0F);
      } else {
        anim.death_progress = 1.0F;
      }
    } else {
      anim.is_dead = true;
      anim.death_progress = 1.0F;
    }
    if (humanoid_state != nullptr && should_persist_animation_state(ctx)) {
      reset_humanoid_animation_state(*humanoid_state);
    }
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    return anim;
  }

  anim.is_in_hold_mode = ((hold_mode != nullptr) && hold_mode->active);
  if (anim.is_in_hold_mode && hold_mode != nullptr) {
    anim.hold_entry_progress = hold_mode->kneel_entry_progress;
  }
  if ((hold_mode != nullptr) && !hold_mode->active && hold_mode->exit_cooldown > 0.0F) {
    anim.is_exiting_hold = true;
    anim.hold_exit_progress =
        1.0F - (hold_mode->exit_cooldown / hold_mode->stand_up_duration);
  }
  anim.is_guarding = (commander != nullptr) && commander->fpv_controlled &&
                     (commander_guard != nullptr) && commander_guard->active;
  anim.guard_pose_progress = anim.is_guarding ? 1.0F : 0.0F;
  anim.visual_movement = movement_state;
  anim.is_moving = anim.visual_movement.is_moving;
  // Prefer the mutex-safe snapshot field; fall back to live stamina only for
  // entities that have never been through finalize (newly spawned, pre-first-tick).
  if (motion != nullptr && motion->initialized) {
    anim.is_running = anim.is_moving && motion->is_running;
  } else {
    anim.is_running = anim.is_moving && (stamina != nullptr) && stamina->is_running;
  }

  auto* healer = ctx.entity->get_component<Engine::Core::HealerComponent>();
  if (healer != nullptr && healer->is_healing_active && transform != nullptr) {
    anim.is_healing = true;
    anim.healing_target_dx = healer->healing_target_x - transform->position.x;
    anim.healing_target_dz = healer->healing_target_z - transform->position.z;
  }

  auto* builder_prod =
      ctx.entity->get_component<Engine::Core::BuilderProductionComponent>();
  if (builder_prod != nullptr) {
    if (builder_prod->in_progress) {
      anim.is_constructing = true;
      float const build_elapsed =
          std::max(0.0F, builder_prod->build_time - builder_prod->time_remaining);
      anim.construction_progress =
          std::fmod(build_elapsed * k_builder_construct_cycles_per_second, 1.0F);
      if (anim.construction_progress < 0.0F) {
        anim.construction_progress += 1.0F;
      }
    }
  }

  if (combat_state != nullptr) {
    anim.attack_variant = combat_state->attack_variant;
    anim.finisher_attack = combat_state->finisher_attack;
    anim.attack_offset = combat_state->attack_offset;
    anim.has_attack_offset = true;
    anim.attack_family = combat_state->attack_family;

    bool const combat_pose_active =
        combat_state->animation_state != Engine::Core::CombatAnimationState::Idle &&
        (!anim.is_moving ||
         combat_state_keeps_attack_pose_while_moving(combat_state->animation_state));
    if (combat_pose_active) {
      anim.combat_phase = map_combat_state_to_phase(combat_state->animation_state);
      if (combat_state->state_duration > 0.0F) {
        anim.combat_phase_progress =
            combat_state->state_time / combat_state->state_duration;
      }
      anim.is_attacking = true;
      anim.is_melee =
          (anim.attack_family == Engine::Core::CombatAttackFamily::None)
              ? ((attack != nullptr) &&
                 (attack->current_mode ==
                  Engine::Core::AttackComponent::CombatMode::Melee))
              : (anim.attack_family != Engine::Core::CombatAttackFamily::Bow);
    }
  }

  if (!anim.is_attacking && (attack != nullptr) && attack->in_melee_lock &&
      Game::Systems::CombatRules::participates_in_rts_melee_lock(ctx.entity)) {
    anim.is_attacking = true;
    anim.is_melee = true;
    if (anim.attack_family == Engine::Core::CombatAttackFamily::None &&
        unit != nullptr) {
      anim.attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Melee);
    }
  }

  if (hit_feedback != nullptr && hit_feedback->is_reacting) {
    anim.is_hit_reacting = true;
    float const progress = hit_feedback->reaction_time /
                           Engine::Core::HitFeedbackComponent::k_reaction_duration;
    anim.hit_reaction_intensity =
        hit_feedback->reaction_intensity * std::max(0.0F, 1.0F - progress);
  }

  if ((combat_state == nullptr) && (attack != nullptr) && (attack_target != nullptr) &&
      attack_target->target_id > 0 && (transform != nullptr)) {
    if (unit != nullptr) {
      anim.attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, attack->current_mode);
    }
    anim.is_melee =
        (attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee);

    bool const stationary = !anim.is_moving;
    float const current_cooldown =
        anim.is_melee ? attack->melee_cooldown : attack->cooldown;
    bool const recently_fired =
        attack->time_since_last < std::min(current_cooldown, 0.45F);
    anim.is_attacking =
        stationary && (movement_state.attack_target_in_range || recently_fired);
  }

  if (anim.is_attacking || anim.is_hit_reacting) {
    anim.is_constructing = false;
  }

  bool const is_active = anim.is_moving || anim.is_attacking || anim.is_constructing ||
                         anim.is_healing || anim.is_hit_reacting || anim.is_dying ||
                         anim.is_dead || anim.is_in_hold_mode || anim.is_exiting_hold ||
                         anim.is_guarding;

  if (humanoid_state == nullptr) {
    anim.idle_duration = 0.0F;
    return anim;
  }

  bool const should_persist = should_persist_animation_state(ctx);
  float delta_time = 0.0F;
  if (humanoid_state->initialized) {
    delta_time = std::max(0.0F, anim.time - humanoid_state->last_sample_time);
    if (anim.time < humanoid_state->last_sample_time) {
      if (should_persist) {
        reset_humanoid_animation_state(*humanoid_state);
      }
      delta_time = 0.0F;
    }
  }

  float idle_duration = humanoid_state->idle_duration;
  bool const initialized = humanoid_state->initialized;
  if (!initialized) {
    idle_duration = 0.0F;
    if (should_persist) {
      humanoid_state->initialized = true;
      humanoid_state->idle_duration = 0.0F;
      humanoid_state->last_sample_time = anim.time;
    }
  } else {
    if (is_active) {
      idle_duration = 0.0F;
    } else {
      idle_duration += delta_time;
    }
  }

  if (should_persist) {
    if (!humanoid_state->initialized) {
      humanoid_state->initialized = true;
    }
    humanoid_state->last_sample_time = anim.time;
    humanoid_state->idle_duration = idle_duration;
  }

  anim.idle_duration = idle_duration;
  return anim;
}

} // namespace Render::GL
