#include "animation_inputs.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../../game/systems/combat_rules.h"
#include "../../../creature/animation_state_components.h"
#include "../../../creature/movement_animation.h"
#include "../../../entity/registry.h"
#include "../../../profiling/combat_animation_diagnostics.h"
#include "../../../profiling/frame_profile.h"
#include "../humanoid_constants.h"

namespace Render::GL {

namespace {

constexpr float k_builder_construct_cycles_per_second = 1.75F;

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
}

void reset_humanoid_animation_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.idle_duration = 0.0F;
  state.last_sample_time = 0.0F;
  state.initialized = false;
  state.guard_pose_progress = 0.0F;
  state.hold_pose_progress = 0.0F;
  state.combat_visual = {};
  reset_humanoid_locomotion_state(state);
}

class HumanoidAnimationModePolicy {
public:
  virtual ~HumanoidAnimationModePolicy() = default;
  [[nodiscard]] virtual auto
  is_guarding(const Engine::Core::CommanderGuardComponent*) const -> bool {
    return false;
  }
};

class StandardHumanoidAnimationPolicy final : public HumanoidAnimationModePolicy {};

class CommanderHumanoidAnimationPolicy final : public HumanoidAnimationModePolicy {
public:
  explicit CommanderHumanoidAnimationPolicy(
      const Engine::Core::CommanderComponent* commander)
      : m_commander(commander) {}

  [[nodiscard]] auto is_guarding(
      const Engine::Core::CommanderGuardComponent* guard) const -> bool override {
    return m_commander != nullptr && m_commander->fpv_controlled && guard != nullptr &&
           guard->active;
  }

private:
  const Engine::Core::CommanderComponent* m_commander = nullptr;
};

[[nodiscard]] auto is_infantry_formation_candidate(
    const Engine::Core::UnitComponent* unit,
    const Engine::Core::FormationModeComponent* formation_mode,
    bool is_mounted,
    const Engine::Core::GuardModeComponent* guard_mode = nullptr) noexcept -> bool {
  if (unit == nullptr || is_mounted) {
    return false;
  }

  bool const formation_active =
      (formation_mode != nullptr) && formation_mode->active;
  bool const guard_active = (guard_mode != nullptr) && guard_mode->active;
  if (!formation_active && !guard_active) {
    return false;
  }

  if (unit->spawn_type != Game::Units::SpawnType::Knight) {
    return false;
  }

  return unit->nation_id == Game::Systems::NationID::RomanRepublic ||
         unit->nation_id == Game::Systems::NationID::Carthage;
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

[[nodiscard]] constexpr auto motion_presentation_to_animation_state(
    Engine::Core::MotionPresentationState state) noexcept
    -> Render::Creature::MovementAnimationState {
  switch (state) {
  case Engine::Core::MotionPresentationState::Run:
    return Render::Creature::MovementAnimationState::Run;
  case Engine::Core::MotionPresentationState::Walk:
    return Render::Creature::MovementAnimationState::Walk;
  case Engine::Core::MotionPresentationState::Idle:
    return Render::Creature::MovementAnimationState::Idle;
  }
  return Render::Creature::MovementAnimationState::Idle;
}

[[nodiscard]] auto synthesize_visual_movement_from_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;
  state.movement_state = anim.movement_state;

  auto* transform = (ctx.entity != nullptr)
                        ? ctx.entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  QVector3D const forward = forward_from_transform(transform);
  state.locomotion_direction = forward;
  state.has_navigation_intent =
      Render::Creature::is_moving_animation(state.movement_state);

  if (!Render::Creature::is_moving_animation(state.movement_state)) {
    return state;
  }

  auto* unit = (ctx.entity != nullptr)
                   ? ctx.entity->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
  if (unit != nullptr) {
    state.speed_hint = std::max(0.1F, unit->speed);
    if (Render::Creature::is_running_animation(state.movement_state)) {
      state.speed_hint *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
    }
  } else {
    state.speed_hint = Render::Creature::is_running_animation(state.movement_state)
                           ? k_reference_run_speed
                           : k_reference_walk_speed;
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
  state.forced_displacement =
      motion.source == Engine::Core::MotionPresentationSource::ForcedDisplacement;
  state.attack_target_in_range = motion.attack_target_in_range;
  state.movement_state = motion_presentation_to_animation_state(motion.state);
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
  if (Render::Creature::is_moving_animation(state.movement_state) &&
      state.speed_hint <= 0.0F && ctx.entity != nullptr) {
    auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    state.speed_hint = (unit != nullptr) ? std::max(0.1F, unit->speed) : 0.0F;
  }
  return state;
}

struct StanceTargets {
  bool hold{false};
  bool exiting_hold{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  bool guard{false};
};

[[nodiscard]] auto
action_blocks_hold_pose(const AnimationInputs& anim) noexcept -> bool {
  return (anim.is_attacking && anim.is_melee) || anim.is_hit_reacting ||
         anim.is_constructing || anim.is_healing || anim.is_dying || anim.is_dead;
}

[[nodiscard]] auto
action_allows_guard_pose(const AnimationInputs& anim) noexcept -> bool {
  return !anim.is_attacking && !anim.is_hit_reacting && !anim.is_constructing &&
         !anim.is_healing && !anim.is_dying && !anim.is_dead;
}

[[nodiscard]] auto resolve_stance_targets(
    const Engine::Core::HoldModeComponent* hold_mode,
    bool raw_guard_requested,
    const Render::Creature::HumanoidAnimationStateComponent* humanoid_state,
    const AnimationInputs& anim) noexcept -> StanceTargets {
  StanceTargets targets{};

  bool const hold_requested = hold_mode != nullptr && hold_mode->active;
  bool const hold_exit_requested =
      hold_mode != nullptr && !hold_mode->active && hold_mode->exit_cooldown > 0.0F;
  bool const hold_blocked = action_blocks_hold_pose(anim);
  targets.hold = hold_requested && !hold_blocked;
  targets.exiting_hold = hold_blocked ? (humanoid_state != nullptr &&
                                         humanoid_state->hold_pose_progress > 1.0e-4F)
                                      : hold_exit_requested;
  if (targets.hold && hold_mode != nullptr) {
    targets.hold_entry_progress =
        std::clamp(hold_mode->kneel_entry_progress, 0.0F, 1.0F);
  }
  if (targets.exiting_hold && !hold_blocked && hold_mode != nullptr) {
    targets.hold_exit_progress =
        1.0F -
        (hold_mode->exit_cooldown / std::max(hold_mode->stand_up_duration, 1.0e-4F));
  }

  targets.guard = raw_guard_requested && action_allows_guard_pose(anim);
  return targets;
}

} // namespace

auto resolve_visual_movement_state(const DrawContext& ctx) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;
  if (ctx.entity == nullptr) {
    return state;
  }

  if (auto* motion =
          ctx.entity->get_component<Engine::Core::MotionPresentationComponent>();
      motion != nullptr) {
    return visual_movement_from_motion_snapshot(ctx, *motion);
  }

  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  state.locomotion_direction = forward_from_transform(transform);
  return state;
}

auto visual_movement_for_animation_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState {
  if (anim.visual_movement.is_authoritative) {
    return anim.visual_movement;
  }
  return synthesize_visual_movement_from_inputs(ctx, anim);
}

auto approximate_attack_phase(const AnimationInputs& anim) noexcept -> float {
  if (!anim.is_attacking) {
    return 0.0F;
  }
  if (anim.combat_phase == CombatAnimPhase::Idle) {
    float const attack_offset = anim.has_attack_offset ? anim.attack_offset : 0.0F;
    float const phase = std::fmod(anim.time + attack_offset, 1.0F);
    return phase < 0.0F ? phase + 1.0F : phase;
  }

  auto const window = Render::Profiling::attack_phase_window(
      anim.combat_phase, false, anim.finisher_attack);
  return window.start + (window.end - window.start) *
                            std::clamp(anim.combat_phase_progress, 0.0F, 1.0F);
}

auto sample_anim_state(const DrawContext& ctx) -> AnimationInputs {
  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const scope(
      ctx.template_prewarm ? nullptr : &profile.animation_input_sampling_us);

  bool attack_from_combat_state = false;
  bool attack_from_melee_lock = false;
  auto finalize_sample = [&](const AnimationInputs& sampled_anim) {
    if (!ctx.template_prewarm && ctx.entity != nullptr) {
      Render::Profiling::UnitAnimationDebugSample debug_sample{};
      debug_sample.entity_id = ctx.entity->get_id();
      debug_sample.sample_time = sampled_anim.time;
      debug_sample.combat_phase = sampled_anim.combat_phase;
      debug_sample.combat_phase_progress = sampled_anim.combat_phase_progress;
      debug_sample.attack_phase = approximate_attack_phase(sampled_anim);
      debug_sample.attack_variant = sampled_anim.attack_variant;
      debug_sample.is_attacking = sampled_anim.is_attacking;
      debug_sample.is_in_melee_lock = sampled_anim.is_in_melee_lock;
      debug_sample.locomotion_state = sampled_anim.movement_state;
      debug_sample.attack_from_combat_state = attack_from_combat_state;
      debug_sample.attack_from_melee_lock = attack_from_melee_lock;
      if (auto* attack_target =
              ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
          attack_target != nullptr) {
        debug_sample.attack_target_id = attack_target->target_id;
      }
      Render::Profiling::CombatAnimationDiagnostics::instance().record_unit_sample(
          debug_sample);
    }
    return sampled_anim;
  };

  if (ctx.animation_override != nullptr) {
    AnimationInputs anim = *ctx.animation_override;
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    anim.movement_state = anim.visual_movement.movement_state;
    return finalize_sample(anim);
  }
  AnimationInputs anim{};
  anim.time = ctx.animation_time;
  anim.movement_state = Render::Creature::MovementAnimationState::Idle;
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
  anim.is_in_melee_lock = false;
  anim.is_casting = false;
  anim.cast_kind = CastVisualKind::None;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.is_dying = false;
  anim.is_dead = false;
  anim.death_progress = 0.0F;
  anim.death_variant = 0;

  if (ctx.entity == nullptr) {
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    anim.movement_state = anim.visual_movement.movement_state;
    return finalize_sample(anim);
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
    anim.movement_state = anim.visual_movement.movement_state;
    return finalize_sample(anim);
  }

  auto* attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  auto* hold_mode = ctx.entity->get_component<Engine::Core::HoldModeComponent>();
  auto* formation_mode =
      ctx.entity->get_component<Engine::Core::FormationModeComponent>();
  auto* combat_state = ctx.entity->get_component<Engine::Core::CombatStateComponent>();
  auto* hit_feedback = ctx.entity->get_component<Engine::Core::HitFeedbackComponent>();
  auto* commander = ctx.entity->get_component<Engine::Core::CommanderComponent>();
  auto* commander_guard =
      ctx.entity->get_component<Engine::Core::CommanderGuardComponent>();
  auto* special_attack =
      ctx.entity->get_component<Engine::Core::SpecialAttackComponent>();
  StandardHumanoidAnimationPolicy const standard_policy;
  const CommanderHumanoidAnimationPolicy commander_policy(commander);
  const HumanoidAnimationModePolicy& animation_policy =
      commander != nullptr
          ? static_cast<const HumanoidAnimationModePolicy&>(commander_policy)
          : static_cast<const HumanoidAnimationModePolicy&>(standard_policy);
  VisualMovementState const movement_state = resolve_visual_movement_state(ctx);

  if (death_anim != nullptr) {
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
    anim.movement_state = Render::Creature::MovementAnimationState::Idle;
    return finalize_sample(anim);
  }

  anim.visual_movement = movement_state;
  anim.movement_state = anim.visual_movement.movement_state;
  auto* guard_mode =
      ctx.entity->get_component<Engine::Core::GuardModeComponent>();
  bool const commander_guarding = animation_policy.is_guarding(commander_guard);
  bool const infantry_formation_guarding =
      is_infantry_formation_candidate(unit, formation_mode, false, guard_mode);
  bool const raw_target_guarding = commander_guarding || infantry_formation_guarding;

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

    if (combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
      anim.combat_phase = combat_state->animation_state;
      if (combat_state->state_duration > 0.0F) {
        anim.combat_phase_progress =
            combat_state->state_time / combat_state->state_duration;
      }
      anim.is_attacking = true;
      attack_from_combat_state = true;
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
    anim.is_in_melee_lock = true;
    attack_from_melee_lock = true;
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

  if (anim.is_attacking && !anim.is_melee && !anim.is_in_melee_lock &&
      special_attack != nullptr && special_attack->use_projectile_system &&
      Game::Systems::is_cast_projectile_kind(special_attack->projectile_kind)) {
    anim.is_casting = true;
    switch (special_attack->projectile_kind) {
    case Game::Systems::ProjectileKind::Fireball:
      anim.cast_kind = CastVisualKind::Fireball;
      break;
    case Game::Systems::ProjectileKind::Arrow:
    case Game::Systems::ProjectileKind::CursedArrow:
    case Game::Systems::ProjectileKind::Stone:
      break;
    }
  }

  StanceTargets const stance =
      resolve_stance_targets(hold_mode, raw_target_guarding, humanoid_state, anim);
  anim.is_in_hold_mode = stance.hold;
  anim.is_exiting_hold = stance.exiting_hold;
  anim.hold_entry_progress = stance.hold_entry_progress;
  anim.hold_exit_progress = stance.hold_exit_progress;

  bool const has_guard_pose =
      stance.guard ||
      (humanoid_state != nullptr && humanoid_state->guard_pose_progress > 1.0e-4F);
  bool const is_active = Render::Creature::is_moving_animation(anim.movement_state) ||
                         anim.is_attacking || anim.is_constructing || anim.is_healing ||
                         anim.is_hit_reacting || anim.is_dying || anim.is_dead ||
                         anim.is_in_hold_mode || anim.is_exiting_hold || has_guard_pose;

  if (humanoid_state == nullptr) {
    anim.is_guarding = stance.guard;
    anim.is_exiting_guard = false;
    anim.guard_pose_progress = stance.guard ? 1.0F : 0.0F;
    anim.idle_duration = 0.0F;
    return finalize_sample(anim);
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
  float guard_pose_progress = stance.guard ? 1.0F : 0.0F;
  float hold_pose_progress =
      stance.hold ? std::clamp(anim.hold_entry_progress, 0.0F, 1.0F) : 0.0F;
  bool is_exiting_guard = false;
  bool is_exiting_hold = anim.is_exiting_hold;
  if (!initialized) {
    idle_duration = 0.0F;
    guard_pose_progress = 0.0F;
    hold_pose_progress = 0.0F;
    if (should_persist) {
      humanoid_state->initialized = true;
      humanoid_state->idle_duration = 0.0F;
      humanoid_state->last_sample_time = anim.time;
      humanoid_state->guard_pose_progress = guard_pose_progress;
      humanoid_state->hold_pose_progress = hold_pose_progress;
    }
  } else {
    float const previous_guard_pose =
        std::clamp(humanoid_state->guard_pose_progress, 0.0F, 1.0F);
    float const previous_hold_pose =
        std::clamp(humanoid_state->hold_pose_progress, 0.0F, 1.0F);
    if (stance.guard) {
      float const enter_duration =
          std::max(Engine::Core::Defaults::k_guard_enter_duration, 1.0e-4F);
      guard_pose_progress =
          std::min(1.0F, previous_guard_pose + delta_time / enter_duration);
    } else if (previous_guard_pose > 0.0F) {
      float const exit_duration =
          std::max(Engine::Core::Defaults::k_guard_exit_duration, 1.0e-4F);
      guard_pose_progress =
          std::max(0.0F, previous_guard_pose - delta_time / exit_duration);
      is_exiting_guard = guard_pose_progress > 0.0F;
    } else {
      guard_pose_progress = 0.0F;
    }

    if (stance.hold) {
      float const enter_duration = std::max(
          (hold_mode != nullptr) ? hold_mode->kneel_duration
                                 : Engine::Core::Defaults::k_hold_kneel_duration,
          1.0e-4F);
      float const gameplay_progress = std::clamp(anim.hold_entry_progress, 0.0F, 1.0F);
      float const max_visual_progress =
          std::min(1.0F, previous_hold_pose + delta_time / enter_duration);
      hold_pose_progress = std::min(std::max(previous_hold_pose, gameplay_progress),
                                    max_visual_progress);
    } else if (previous_hold_pose > 0.0F) {
      float const exit_duration = std::max(
          (hold_mode != nullptr) ? hold_mode->stand_up_duration
                                 : Engine::Core::Defaults::k_hold_stand_up_duration,
          1.0e-4F);
      hold_pose_progress =
          std::max(0.0F, previous_hold_pose - delta_time / exit_duration);
      is_exiting_hold = hold_pose_progress > 0.0F;
    } else {
      hold_pose_progress = 0.0F;
      is_exiting_hold = false;
    }

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
    humanoid_state->guard_pose_progress = guard_pose_progress;
    humanoid_state->hold_pose_progress = hold_pose_progress;
  }

  anim.is_guarding = stance.guard;
  anim.is_exiting_guard = is_exiting_guard;
  anim.guard_pose_progress = guard_pose_progress;
  anim.is_in_hold_mode = stance.hold;
  anim.is_exiting_hold = is_exiting_hold;
  if (stance.hold) {
    anim.hold_entry_progress = hold_pose_progress;
    anim.hold_exit_progress = 0.0F;
  } else if (is_exiting_hold) {
    anim.hold_entry_progress = 0.0F;
    anim.hold_exit_progress = 1.0F - hold_pose_progress;
  } else {
    anim.hold_entry_progress = 0.0F;
    anim.hold_exit_progress = 0.0F;
  }
  anim.idle_duration = idle_duration;
  return finalize_sample(anim);
}

} // namespace Render::GL
