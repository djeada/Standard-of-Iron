#include "animation_inputs.h"

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/core/world.h"
#include "../../../../game/systems/combat_actions/combat_action_definition.h"
#include "../../../creature/animation_state_components.h"
#include "../../../creature/movement_animation.h"
#include "../../../entity/registry.h"
#include "../../../profiling/combat_animation_diagnostics.h"
#include "../../../profiling/frame_profile.h"
#include "../humanoid_constants.h"
#include "animation/action_manifest.h"
#include "animation/state_manifest.h"

namespace Render::GL {

namespace {

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

[[nodiscard]] constexpr auto to_animation_combat_phase(
    Engine::Core::CombatAnimationState phase) noexcept -> Animation::CombatPhase {
  switch (phase) {
  case Engine::Core::CombatAnimationState::Idle:
    return Animation::CombatPhase::Idle;
  case Engine::Core::CombatAnimationState::Advance:
    return Animation::CombatPhase::Advance;
  case Engine::Core::CombatAnimationState::WindUp:
    return Animation::CombatPhase::WindUp;
  case Engine::Core::CombatAnimationState::Strike:
    return Animation::CombatPhase::Strike;
  case Engine::Core::CombatAnimationState::Impact:
    return Animation::CombatPhase::Impact;
  case Engine::Core::CombatAnimationState::Recover:
    return Animation::CombatPhase::Recover;
  case Engine::Core::CombatAnimationState::Reposition:
    return Animation::CombatPhase::Reposition;
  }
  return Animation::CombatPhase::Idle;
}

[[nodiscard]] auto synthesize_visual_movement_from_inputs(
    const DrawContext&, const AnimationInputs& anim) -> VisualMovementState {
  VisualMovementState state{};
  state.is_authoritative = true;
  state.movement_state = anim.movement_state;

  state.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  state.has_navigation_intent =
      Render::Creature::is_moving_animation(state.movement_state);

  if (!Render::Creature::is_moving_animation(state.movement_state)) {
    return state;
  }

  state.speed_hint = Render::Creature::is_running_animation(state.movement_state)
                         ? k_reference_run_speed
                         : k_reference_walk_speed;

  return state;
}

[[nodiscard]] auto visual_movement_from_motion_snapshot(
    const DrawContext&,
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
    state.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    state.locomotion_direction.normalize();
  }
  state.speed_hint = motion.speed;
  if (Render::Creature::is_moving_animation(state.movement_state) &&
      state.speed_hint <= 0.0F) {
    state.speed_hint = Render::Creature::is_running_animation(state.movement_state)
                           ? k_reference_run_speed
                           : k_reference_walk_speed;
  }
  return state;
}

[[nodiscard]] auto humanoid_action_flags(const AnimationInputs& anim) noexcept
    -> Animation::HumanoidActionBlockInputs {
  return {
      .is_attacking = anim.is_attacking,
      .is_melee = anim.is_melee,
      .preserves_hold_pose = anim.hold_attack_preserves_pose,
      .is_hit_reacting = anim.is_hit_reacting,
      .is_constructing = anim.is_constructing,
      .is_healing = anim.is_healing,
      .is_dying = anim.is_dying,
      .is_dead = anim.is_dead,
  };
}

[[nodiscard]] auto resolve_stance_targets(
    const Engine::Core::CreaturePresentationComponent& presentation,
    const Render::Creature::HumanoidAnimationStateComponent* humanoid_state,
    const AnimationInputs& anim) noexcept -> Animation::HumanoidStanceTargets {
  return Animation::resolve_humanoid_stance_targets({
      .hold_requested = presentation.hold_requested,
      .hold_exit_requested = presentation.hold_exit_requested,
      .raw_guard_requested = presentation.guard_requested,
      .hold_entry_progress = presentation.hold_entry_progress,
      .hold_exit_progress = presentation.hold_exit_progress,
      .previous_hold_pose_progress =
          humanoid_state != nullptr ? humanoid_state->hold_pose_progress : 0.0F,
      .action = humanoid_action_flags(anim),
  });
}

[[nodiscard]] auto commander_sword_attack_animation(
    std::uint8_t authored_action_id) noexcept -> Animation::SwordAttackAnimation {
  switch (
      static_cast<Game::Systems::CombatActions::CombatActionId>(authored_action_id)) {
  case Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft:
    return Animation::SwordAttackAnimation::RpgSlashLeft;
  case Game::Systems::CombatActions::CombatActionId::RpgSwordSlashRight:
    return Animation::SwordAttackAnimation::RpgSlashRight;
  case Game::Systems::CombatActions::CombatActionId::RpgSwordOverhead:
    return Animation::SwordAttackAnimation::RpgOverhead;
  case Game::Systems::CombatActions::CombatActionId::RpgSwordThrust:
    return Animation::SwordAttackAnimation::RpgThrust;
  case Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher:
    return Animation::SwordAttackAnimation::RpgFinisher;
  case Game::Systems::CombatActions::CombatActionId::MountedSwordSlash:
    return Animation::SwordAttackAnimation::RpgSlashRight;
  case Game::Systems::CombatActions::CombatActionId::RpgSpearThrust:
  case Game::Systems::CombatActions::CombatActionId::RpgSpearSweep:
  case Game::Systems::CombatActions::CombatActionId::RpgBowShot:
  case Game::Systems::CombatActions::CombatActionId::MountedSpearThrust:
  case Game::Systems::CombatActions::CombatActionId::MountedChargeImpact:
  case Game::Systems::CombatActions::CombatActionId::RtsSpearThrust:
  case Game::Systems::CombatActions::CombatActionId::RtsBowShot:
  case Game::Systems::CombatActions::CombatActionId::None:
    break;
  case Game::Systems::CombatActions::CombatActionId::RtsSwordStrike:
    return Animation::SwordAttackAnimation::InfantrySlashA;
  }
  return Animation::SwordAttackAnimation::InfantrySlashA;
}

void apply_presentation_sample(
    AnimationInputs& anim,
    const Engine::Core::CreaturePresentationComponent& presentation) noexcept {
  anim.is_attacking = presentation.is_attacking;
  anim.is_melee = presentation.is_melee;
  anim.is_in_melee_lock = presentation.is_in_melee_lock;
  anim.combat_phase = presentation.combat_phase;
  anim.combat_phase_progress = presentation.combat_phase_progress;
  anim.attack_family = presentation.attack_family;
  anim.attack_variant = presentation.attack_variant;
  anim.finisher_attack = presentation.finisher_attack;
  anim.attack_offset = presentation.attack_offset;
  anim.has_attack_offset = presentation.has_attack_offset;
  anim.is_casting = presentation.is_casting;
  anim.cast_kind = presentation.cast == Engine::Core::CreatureCastPresentation::Fireball
                       ? CastVisualKind::Fireball
                       : CastVisualKind::None;
  anim.is_hit_reacting = presentation.is_hit_reacting;
  anim.hit_reaction_intensity = presentation.hit_reaction_intensity;
  anim.hit_recoil_x = presentation.hit_recoil_x;
  anim.hit_recoil_z = presentation.hit_recoil_z;
  anim.is_healing = presentation.is_healing;
  anim.healing_target_dx = presentation.healing_target_dx;
  anim.healing_target_dz = presentation.healing_target_dz;
  anim.is_constructing = presentation.is_constructing;
  anim.construction_progress = presentation.construction_progress;
  anim.is_dying = presentation.is_dying;
  anim.is_dead = presentation.is_dead;
  anim.death_progress = presentation.death_progress;
  anim.death_variant = presentation.death_variant;
}

void apply_authored_action_sample(
    AnimationInputs& anim,
    const Engine::Core::RpgCommanderActionComponent* action) noexcept;

void apply_authored_action_presentation(
    AnimationInputs& anim,
    const Engine::Core::CreaturePresentationComponent& presentation) noexcept {
  Engine::Core::RpgCommanderActionComponent authored;
  authored.combat_action_id = presentation.authored_action_id;
  authored.action_running = presentation.authored_action_running;
  authored.action_completed = presentation.authored_action_completed;
  authored.normalized_action_time = presentation.authored_action_phase;
  apply_authored_action_sample(anim, &authored);
}

[[nodiscard]] auto action_event_time(
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    Game::Systems::CombatActions::CombatActionEventType type,
    float fallback) noexcept -> float {
  for (auto const& event : definition.events) {
    if (event.type == type) {
      return event.normalized_time;
    }
  }
  return fallback;
}

void apply_authored_action_sample(
    AnimationInputs& anim,
    const Engine::Core::RpgCommanderActionComponent* action) noexcept {
  if (action == nullptr || action->combat_action_id == 0U ||
      (!action->action_running && !action->action_completed)) {
    return;
  }
  auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
      action->combat_action_id);
  auto const* definition =
      Game::Systems::CombatActions::find_combat_action_definition(action_id);
  if (definition == nullptr) {
    return;
  }

  float const time = std::clamp(action->normalized_action_time, 0.0F, 1.0F);
  float const windup = action_event_time(
      *definition,
      Game::Systems::CombatActions::CombatActionEventType::WindupStart,
      0.08F);
  float const active_start = action_event_time(
      *definition,
      Game::Systems::CombatActions::CombatActionEventType::ActiveStart,
      0.35F);
  float const active = action_event_time(
      *definition,
      Game::Systems::CombatActions::CombatActionEventType::WeaponTraceStart,
      active_start);
  float const recovery = action_event_time(
      *definition,
      Game::Systems::CombatActions::CombatActionEventType::RecoveryStart,
      0.75F);
  float const exit =
      action_event_time(*definition,
                        Game::Systems::CombatActions::CombatActionEventType::ExitSafe,
                        0.92F);

  auto set_phase = [&](CombatAnimPhase phase, float start, float end) {
    anim.combat_phase = phase;
    anim.combat_phase_progress =
        std::clamp((time - start) / std::max(1.0e-4F, end - start), 0.0F, 1.0F);
  };
  if (time < windup) {
    set_phase(CombatAnimPhase::Advance, 0.0F, windup);
  } else if (time < active) {
    set_phase(CombatAnimPhase::WindUp, windup, active);
  } else if (time < recovery) {
    set_phase(CombatAnimPhase::Strike, active, recovery);
  } else if (time < exit) {
    set_phase(CombatAnimPhase::Recover, recovery, exit);
  } else {
    set_phase(CombatAnimPhase::Reposition, exit, 1.0F);
  }

  anim.is_attacking = action->action_running;
  anim.is_melee =
      definition->weapon_family != Game::Systems::CombatActions::WeaponFamily::Bow;
  anim.attack_family = definition->attack_family;
  anim.finisher_attack =
      action_id == Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher;
}

void apply_persistent_stance(
    const DrawContext& ctx,
    Render::Creature::HumanoidAnimationStateComponent* humanoid_state,
    const Animation::HumanoidStanceTargets& stance,
    float hold_enter_duration,
    float hold_exit_duration,
    AnimationInputs& anim) {
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
    return;
  }

  bool const should_persist = should_persist_animation_state(ctx);
  bool previous_initialized = humanoid_state->initialized;
  float previous_sample_time = humanoid_state->last_sample_time;
  float previous_idle_duration = humanoid_state->idle_duration;
  float previous_guard_pose_progress = humanoid_state->guard_pose_progress;
  float previous_hold_pose_progress = humanoid_state->hold_pose_progress;
  if (previous_initialized && anim.time < previous_sample_time) {
    if (should_persist) {
      reset_humanoid_animation_state(*humanoid_state);
      previous_initialized = false;
      previous_sample_time = 0.0F;
      previous_idle_duration = 0.0F;
      previous_guard_pose_progress = 0.0F;
      previous_hold_pose_progress = 0.0F;
    } else {
      previous_sample_time = anim.time;
    }
  }

  auto const persistent_stance = Animation::resolve_humanoid_persistent_stance_state({
      .sample_time = anim.time,
      .previous_initialized = previous_initialized,
      .previous_sample_time = previous_sample_time,
      .previous_idle_duration = previous_idle_duration,
      .previous_guard_pose_progress = previous_guard_pose_progress,
      .previous_hold_pose_progress = previous_hold_pose_progress,
      .is_active = is_active,
      .stance = stance,
      .guard_enter_duration = Engine::Core::Defaults::k_guard_enter_duration,
      .guard_exit_duration = Engine::Core::Defaults::k_guard_exit_duration,
      .hold_enter_duration = hold_enter_duration,
      .hold_exit_duration = hold_exit_duration,
  });

  if (should_persist) {
    humanoid_state->initialized = persistent_stance.initialized;
    humanoid_state->last_sample_time = persistent_stance.last_sample_time;
    humanoid_state->idle_duration = persistent_stance.idle_duration;
    humanoid_state->guard_pose_progress = persistent_stance.guard_pose_progress;
    humanoid_state->hold_pose_progress = persistent_stance.hold_pose_progress;
  }

  anim.is_guarding = persistent_stance.is_guarding;
  anim.is_exiting_guard = persistent_stance.is_exiting_guard;
  anim.guard_pose_progress = persistent_stance.guard_pose_progress;
  anim.is_in_hold_mode = persistent_stance.is_in_hold_mode;
  anim.is_exiting_hold = persistent_stance.is_exiting_hold;
  anim.hold_entry_progress = persistent_stance.hold_entry_progress;
  anim.hold_exit_progress = persistent_stance.hold_exit_progress;
  anim.idle_duration = persistent_stance.idle_duration;
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

  state.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
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
  return Animation::approximate_humanoid_attack_phase({
      .is_attacking = anim.is_attacking,
      .combat_phase = to_animation_combat_phase(anim.combat_phase),
      .combat_phase_progress = anim.combat_phase_progress,
      .finisher_attack = anim.finisher_attack,
      .sample_time = anim.time,
      .attack_offset = anim.attack_offset,
      .has_attack_offset = anim.has_attack_offset,
  });
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
      if (auto const* presentation =
              ctx.entity->get_component<Engine::Core::CreaturePresentationComponent>();
          presentation != nullptr && presentation->snapshot_valid) {
        debug_sample.attack_target_id = presentation->target_id;
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
  anim.has_sword_attack_animation = false;
  anim.sword_attack_animation = Animation::SwordAttackAnimation::InfantrySlashA;
  anim.attack_offset = 0.0F;
  anim.has_attack_offset = false;
  anim.is_in_melee_lock = false;
  anim.is_casting = false;
  anim.cast_kind = CastVisualKind::None;
  anim.is_hit_reacting = false;
  anim.hit_reaction_intensity = 0.0F;
  anim.hit_recoil_x = 0.0F;
  anim.hit_recoil_z = 0.0F;
  anim.is_dying = false;
  anim.is_dead = false;
  anim.death_progress = 0.0F;
  anim.death_variant = 0;

  if (ctx.entity == nullptr) {
    anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
    anim.movement_state = anim.visual_movement.movement_state;
    return finalize_sample(anim);
  }

  if (auto const* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
      unit != nullptr) {
    anim.hold_attack_preserves_pose =
        unit->spawn_type == Game::Units::SpawnType::Spearman ||
        unit->spawn_type == Game::Units::SpawnType::Archer;
  }

  auto* humanoid_state = Engine::Core::get_or_add_component<
      Render::Creature::HumanoidAnimationStateComponent>(ctx.entity);
  auto* presentation =
      ctx.entity->get_component<Engine::Core::CreaturePresentationComponent>();
  if (ctx.world == nullptr || presentation == nullptr ||
      !presentation->snapshot_valid) {

    Engine::Core::publish_creature_presentation(ctx.entity, ctx.world);
    presentation =
        ctx.entity->get_component<Engine::Core::CreaturePresentationComponent>();
  }
  if (presentation != nullptr && presentation->snapshot_valid) {
    anim.visual_movement = resolve_visual_movement_state(ctx);
    anim.movement_state = anim.visual_movement.movement_state;
    apply_presentation_sample(anim, *presentation);
    if (anim.hold_attack_preserves_pose && presentation->hold_requested) {
      anim.is_hit_reacting = false;
      anim.hit_reaction_intensity = 0.0F;
      anim.hit_recoil_x = 0.0F;
      anim.hit_recoil_z = 0.0F;
    }
    attack_from_combat_state = presentation->attack_from_combat_state;
    attack_from_melee_lock = presentation->attack_from_melee_lock;

    if (anim.is_dying || anim.is_dead) {
      if (humanoid_state != nullptr && should_persist_animation_state(ctx)) {
        reset_humanoid_animation_state(*humanoid_state);
      }
      anim.movement_state = Render::Creature::MovementAnimationState::Idle;
      return finalize_sample(anim);
    }

    apply_authored_action_presentation(anim, *presentation);
    if (presentation->fpv_controlled && anim.is_attacking && anim.is_melee &&
        anim.attack_family == Engine::Core::CombatAttackFamily::Sword) {
      anim.has_sword_attack_animation = true;
      anim.sword_attack_animation =
          commander_sword_attack_animation(presentation->authored_action_id);
    }
    if (presentation->authored_action_id != 0U) {
      auto const action_id = static_cast<Game::Systems::CombatActions::CombatActionId>(
          presentation->authored_action_id);
      auto const* definition =
          Game::Systems::CombatActions::find_combat_action_definition(action_id);
      if (definition != nullptr &&
          definition->rider_clip_id != Animation::k_unmapped_clip) {
        anim.has_authored_action_clip = true;
        anim.authored_action_clip = definition->rider_clip_id;
        anim.authored_action_phase =
            std::clamp(presentation->authored_action_phase, 0.0F, 1.0F);
      }
    }

    auto const stance = resolve_stance_targets(*presentation, humanoid_state, anim);
    apply_persistent_stance(ctx,
                            humanoid_state,
                            stance,
                            presentation->hold_enter_duration,
                            presentation->hold_exit_duration,
                            anim);
    return finalize_sample(anim);
  }
  anim.visual_movement = visual_movement_for_animation_inputs(ctx, anim);
  anim.movement_state = anim.visual_movement.movement_state;
  return finalize_sample(anim);
}

} // namespace Render::GL
