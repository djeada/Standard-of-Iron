#include "render/creature/humanoid/pipeline/humanoid_animation_selection_internal.h"

namespace Render::Creature::Pipeline {

using namespace humanoid_animation_selection_detail;

namespace {

auto apply_ambient_idle_crossfade(HumanoidAnimationSelection& selection,
                                  const Render::GL::HumanoidAnimationContext& anim,
                                  const UnitVisualSpec& spec,
                                  std::uint32_t seed,
                                  const Render::GL::HumanoidVariant* variant) noexcept
    -> bool {
  if (anim.ambient_idle_type == Render::GL::AmbientIdleType::None) {
    return false;
  }
  if (selection.state != Render::Creature::AnimationStateId::Idle &&
      selection.state != Render::Creature::AnimationStateId::RidingIdle) {
    return false;
  }
  float const blend = std::clamp(anim.ambient_idle_blend, 0.0F, 1.0F);
  if (blend >= 0.999F) {
    return true;
  }

  HumanoidAnimationSelection const ambient = selection;

  Render::GL::HumanoidAnimationContext resting = anim;
  resting.ambient_idle_type = Render::GL::AmbientIdleType::None;
  resting.ambient_idle_phase = 0.0F;
  resting.ambient_idle_blend = 0.0F;
  selection = build_selection_for_pose(
      spec, resting, Render::Creature::resolve_pose(resting.inputs), seed, variant);

  if (blend > 0.001F && ambient.clip_id.has_value()) {
    selection.full_body_blend = playback_layer_from_selection(
        ambient, blend, Render::Creature::PlaybackLayerMode::FullBodyBlend);
  }
  return true;
}

auto locomotion_pose_intent(Render::Creature::AnimationStateId state) noexcept
    -> Animation::PoseIntent {
  switch (state) {
  case Render::Creature::AnimationStateId::Walk:
    return Animation::PoseIntent::Walk;
  case Render::Creature::AnimationStateId::Run:
    return Animation::PoseIntent::Run;
  case Render::Creature::AnimationStateId::WalkStrafeLeft:
    return Animation::PoseIntent::WalkStrafeLeft;
  case Render::Creature::AnimationStateId::WalkStrafeRight:
    return Animation::PoseIntent::WalkStrafeRight;
  case Render::Creature::AnimationStateId::RunStrafeLeft:
    return Animation::PoseIntent::RunStrafeLeft;
  case Render::Creature::AnimationStateId::RunStrafeRight:
    return Animation::PoseIntent::RunStrafeRight;
  default:
    return Animation::PoseIntent::Idle;
  }
}

auto is_locomotion_state(Render::Creature::AnimationStateId state) noexcept -> bool {
  return state == Render::Creature::AnimationStateId::Idle ||
         state == Render::Creature::AnimationStateId::Walk ||
         state == Render::Creature::AnimationStateId::Run;
}

auto apply_locomotion_crossfade(HumanoidAnimationSelection& selection,
                                const Render::GL::HumanoidAnimationContext& anim,
                                const UnitVisualSpec& spec,
                                std::uint32_t seed,
                                const Render::GL::HumanoidVariant* variant) noexcept
    -> bool {

  bool const guard_stance_held =
      (anim.inputs.is_guarding || anim.inputs.is_exiting_guard) &&
      !steps_over_ground(anim);
  if (!is_locomotion_state(selection.state) || anim.inputs.is_attacking ||
      anim.inputs.is_casting || anim.inputs.is_mounted ||
      anim.inputs.has_showcase_clip || anim.inputs.is_in_hold_mode ||
      anim.inputs.is_exiting_hold || guard_stance_held || anim.inputs.is_hit_reacting ||
      anim.inputs.is_healing || anim.inputs.is_constructing || anim.inputs.is_dying ||
      anim.inputs.is_dead) {
    return false;
  }

  auto const crossfade = Animation::resolve_locomotion_crossfade({
      .resolved = selection.state,
      .locomotion_presence = anim.gait.locomotion_presence,
      .run_presence = anim.gait.run_presence,
      .lateral_share = anim.inputs.visual_movement.facing_independent_of_travel
                           ? anim.gait.turn_amount
                           : 0.0F,
  });
  if (!crossfade.active) {
    return false;
  }

  auto const build = [&](Render::Creature::AnimationStateId state) {
    return build_selection_for_pose(
        spec,
        anim,
        Render::Creature::resolve_pose_for_intent(locomotion_pose_intent(state)),
        seed,
        variant);
  };
  auto const primary = build(crossfade.primary);
  auto const secondary = build(crossfade.secondary);
  if (!primary.clip_id.has_value() || !secondary.clip_id.has_value() ||
      primary.clip_id == secondary.clip_id) {
    return false;
  }

  selection = primary;
  selection.full_body_blend =
      playback_layer_from_selection(secondary,
                                    crossfade.secondary_weight,
                                    Render::Creature::PlaybackLayerMode::FullBodyBlend);
  return true;
}

auto apply_hold_stance_crossfade(HumanoidAnimationSelection& selection,
                                 const Render::GL::HumanoidAnimationContext& anim,
                                 const UnitVisualSpec& spec,
                                 std::uint32_t seed,
                                 const Render::GL::HumanoidVariant* variant) noexcept
    -> bool {
  if (!anim.inputs.is_in_hold_mode && !anim.inputs.is_exiting_hold) {
    return false;
  }
  if (selection.state != Render::Creature::AnimationStateId::Hold) {
    return false;
  }

  float const blend =
      std::clamp(Render::GL::hold_transition_amount(anim.inputs), 0.0F, 1.0F);
  if (blend >= 0.999F) {
    return false;
  }

  HumanoidAnimationSelection const kneeling = selection;

  Render::GL::HumanoidAnimationContext standing = anim;
  standing.inputs.is_in_hold_mode = false;
  standing.inputs.is_exiting_hold = false;
  standing.inputs.hold_entry_progress = 0.0F;
  standing.inputs.hold_exit_progress = 0.0F;
  selection = build_selection_for_pose(
      spec, standing, Render::Creature::resolve_pose(standing.inputs), seed, variant);

  if (blend > 0.001F && kneeling.clip_id.has_value()) {
    selection.full_body_blend = playback_layer_from_selection(
        kneeling, blend, Render::Creature::PlaybackLayerMode::FullBodyBlend);
  }
  return true;
}

auto apply_construction_crossfade(HumanoidAnimationSelection& selection,
                                  const Render::GL::HumanoidAnimationContext& anim,
                                  const UnitVisualSpec& spec,
                                  std::uint32_t seed,
                                  const Render::GL::HumanoidVariant* variant) noexcept
    -> bool {
  float const blend = std::clamp(anim.construction_blend, 0.0F, 1.0F);
  if (anim.construction_role == Animation::HumanoidConstructionRole::None ||
      anim.inputs.is_attacking || anim.inputs.is_hit_reacting ||
      anim.inputs.is_mounted || anim.inputs.is_dying || anim.inputs.is_dead) {
    return false;
  }

  auto const build_rest_selection = [&]() {
    Render::GL::HumanoidAnimationContext resting = anim;
    resting.inputs.is_constructing = false;
    resting.construction_role = Animation::HumanoidConstructionRole::None;
    resting.construction_blend = 0.0F;
    return build_selection_for_pose(
        spec, resting, Render::Creature::resolve_pose(resting.inputs), seed, variant);
  };
  if (blend <= 0.001F) {
    selection = build_rest_selection();
    return true;
  }

  Render::GL::HumanoidAnimationContext working = anim;
  working.inputs.is_constructing = true;
  HumanoidAnimationSelection const work_selection = build_selection_for_pose(
      spec, working, Render::Creature::resolve_pose(working.inputs), seed, variant);
  if (!work_selection.clip_id.has_value()) {
    return false;
  }

  selection = work_selection;
  if (blend >= 0.999F) {
    return true;
  }

  HumanoidAnimationSelection const rest_selection = build_rest_selection();
  if (rest_selection.clip_id.has_value()) {
    selection.full_body_blend = playback_layer_from_selection(
        rest_selection,
        1.0F - blend,
        Render::Creature::PlaybackLayerMode::FullBodyBlend);
  }
  return true;
}

} // namespace

void blend_out_interrupted_clip(HumanoidAnimationSelection& selection,
                                std::uint16_t outgoing_clip,
                                float outgoing_phase,
                                float weight) noexcept {
  if (outgoing_clip == Animation::k_unmapped_clip || weight <= 0.001F ||
      !selection.clip_id.has_value() || selection.clip_id == outgoing_clip) {
    return;
  }
  HumanoidAnimationSelection outgoing = selection;
  outgoing.clip_id = outgoing_clip;
  outgoing.phase = std::clamp(outgoing_phase, 0.0F, 1.0F);
  outgoing.clip_variant = 0U;
  outgoing.full_body_blend = {};
  outgoing.upper_body_overlay = {};
  selection.full_body_blend =
      playback_layer_from_selection(outgoing,
                                    std::clamp(weight, 0.0F, 1.0F),
                                    Render::Creature::PlaybackLayerMode::FullBodyBlend);
}

namespace {

auto resolve_selection_layers(const UnitVisualSpec& spec,
                              const Render::GL::HumanoidAnimationContext& anim,
                              std::uint32_t seed,
                              const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection;

auto gesture_rides_locomotion(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> bool {
  auto const clip = anim.inputs.showcase_clip;
  return anim.inputs.has_showcase_clip &&
         clip >= Animation::k_humanoid_gesture_first_clip &&
         clip < Animation::k_humanoid_gesture_first_clip +
                    Animation::k_humanoid_gesture_clip_count &&
         Render::Creature::is_moving_animation(anim.inputs.movement_state) &&
         !anim.inputs.is_mounted && !anim.inputs.is_attacking &&
         !anim.inputs.is_dying && !anim.inputs.is_dead;
}

} // namespace

auto resolve_humanoid_animation_selection(
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim,
    std::uint32_t seed,
    const Render::GL::HumanoidVariant* variant) noexcept -> HumanoidAnimationSelection {
  if (!gesture_rides_locomotion(anim)) {
    return resolve_selection_layers(spec, anim, seed, variant);
  }
  Render::GL::HumanoidAnimationContext walking = anim;
  walking.inputs.has_showcase_clip = false;
  walking.inputs.showcase_clip = Animation::k_unmapped_clip;
  HumanoidAnimationSelection selection =
      resolve_selection_layers(spec, walking, seed, variant);
  HumanoidAnimationSelection gesture = selection;
  gesture.clip_id = anim.inputs.showcase_clip;
  gesture.clip_variant = 0U;
  gesture.phase = std::clamp(anim.inputs.showcase_phase, 0.0F, 1.0F);
  selection.upper_body_overlay = playback_layer_from_selection(
      gesture, 1.0F, Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
  return selection;
}

namespace {

void keep_weapon_ready_while_stepping(HumanoidAnimationSelection& selection,
                                      const Render::GL::HumanoidAnimationContext& anim,
                                      std::uint32_t seed) noexcept {
  bool const strides = selection.state == Render::Creature::AnimationStateId::Walk ||
                       selection.state == Render::Creature::AnimationStateId::Run;
  if (!steps_over_ground(anim) || !anim.inputs.is_in_melee_lock || !strides ||
      selection.upper_body_overlay.active() || anim.inputs.is_hit_reacting ||
      anim.inputs.is_dying || anim.inputs.is_dead || anim.inputs.is_constructing ||
      anim.inputs.is_healing || anim.inputs.is_casting ||
      anim.inputs.has_showcase_clip || anim.inputs.is_in_hold_mode ||
      anim.inputs.is_exiting_hold || anim.inputs.is_defensive_layout_locked) {
    return;
  }
  HumanoidAnimationSelection ready = selection;
  ready.clip_id = Animation::k_humanoid_combat_ready_clip;
  ready.clip_variant = 0U;
  ready.phase = Animation::wrap_locomotion_phase(
      anim.inputs.time / Animation::k_humanoid_combat_ready_cycle_time +
      Animation::humanoid_idle_breath_offset(seed));
  selection.upper_body_overlay = playback_layer_from_selection(
      ready, 1.0F, Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
}

auto resolve_action_layers(const UnitVisualSpec& spec,
                           const Render::GL::HumanoidAnimationContext& anim,
                           std::uint32_t seed,
                           const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection;

auto resolve_selection_layers(const UnitVisualSpec& spec,
                              const Render::GL::HumanoidAnimationContext& anim,
                              std::uint32_t seed,
                              const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection {
  HumanoidAnimationSelection selection =
      resolve_action_layers(spec, anim, seed, variant);
  keep_weapon_ready_while_stepping(selection, anim, seed);
  return selection;
}

auto resolve_action_layers(const UnitVisualSpec& spec,
                           const Render::GL::HumanoidAnimationContext& anim,
                           std::uint32_t seed,
                           const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection {
  HumanoidAnimationSelection selection = build_selection_for_pose(
      spec, anim, Render::Creature::resolve_pose(anim.inputs), seed, variant);

  auto const defensive_clip =
      anim.inputs.is_defensive_layout_locked
          ? defensive_layout_clip_for(anim.inputs.shield_formation_pose)
          : Animation::k_unmapped_clip;
  auto const apply_defensive_overlay = [&](HumanoidAnimationSelection& target) {
    if (defensive_clip == Animation::k_unmapped_clip) {
      return;
    }
    HumanoidAnimationSelection defensive = target;
    defensive.clip_id = defensive_clip;
    defensive.clip_variant = 0U;
    target.upper_body_overlay = playback_layer_from_selection(
        defensive, 1.0F, Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
  };
  auto const apply_resource_carry_overlay = [&](HumanoidAnimationSelection& target) {
    if (!anim.inputs.is_carrying_load || anim.inputs.is_attacking ||
        anim.inputs.is_casting || anim.inputs.is_mounted ||
        anim.inputs.has_showcase_clip || anim.inputs.has_authored_action_clip ||
        anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold ||
        anim.inputs.is_guarding || anim.inputs.is_exiting_guard ||
        anim.inputs.is_hit_reacting || anim.inputs.is_healing ||
        anim.inputs.is_constructing || anim.inputs.is_dying || anim.inputs.is_dead) {
      return false;
    }

    HumanoidAnimationSelection carry = target;
    carry.clip_id = Animation::k_humanoid_resource_carry_clip;
    carry.clip_variant = 0U;
    carry.phase = std::fmod(std::max(anim.inputs.time, 0.0F), 1.8F) / 1.8F;
    target.upper_body_overlay = playback_layer_from_selection(
        carry, 1.0F, Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
    return true;
  };

  auto const apply_wade_overlay = [&](HumanoidAnimationSelection& target) {
    if (!anim.inputs.is_wading || anim.inputs.wade_amount <= 0.0F ||
        anim.inputs.is_attacking || anim.inputs.is_casting || anim.inputs.is_mounted ||
        anim.inputs.has_showcase_clip || anim.inputs.has_authored_action_clip ||
        anim.inputs.is_guarding || anim.inputs.is_exiting_guard ||
        anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold ||
        anim.inputs.is_defensive_layout_locked ||
        anim.ambient_idle_type != Render::GL::AmbientIdleType::None ||
        anim.inputs.is_hit_reacting || anim.inputs.is_healing ||
        anim.inputs.is_constructing || anim.inputs.is_dying || anim.inputs.is_dead) {
      return false;
    }

    HumanoidAnimationSelection wade = target;
    wade.clip_id = Animation::k_humanoid_wade_clip;
    wade.clip_variant = 0U;

    wade.phase = anim.gait.cycle_phase;
    target.upper_body_overlay = playback_layer_from_selection(
        wade,
        std::clamp(anim.inputs.wade_amount, 0.0F, 1.0F),
        Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
    return true;
  };

  if (apply_construction_crossfade(selection, anim, spec, seed, variant)) {
    return selection;
  }

  if (apply_locomotion_crossfade(selection, anim, spec, seed, variant)) {
    if (!apply_wade_overlay(selection)) {
      apply_resource_carry_overlay(selection);
      apply_defensive_overlay(selection);
    }
    return selection;
  }

  if (apply_wade_overlay(selection)) {
    return selection;
  }

  if (defensive_clip != Animation::k_unmapped_clip) {

    bool const holding_still =
        (selection.state == Render::Creature::AnimationStateId::Idle ||
         selection.state == Render::Creature::AnimationStateId::Hold) &&
        !Render::Creature::is_moving_animation(anim.inputs.movement_state) &&
        !anim.inputs.is_attacking && !anim.inputs.is_hit_reacting &&
        !anim.inputs.is_dying && !anim.inputs.is_dead;
    if (holding_still) {
      selection.clip_id = defensive_clip;
      selection.clip_variant = 0U;
      selection.phase = k_formed_shield_hold_phase;
      selection.full_body_blend = {};
      selection.upper_body_overlay = {};
      return selection;
    }
    apply_defensive_overlay(selection);
    return selection;
  }

  if (apply_resource_carry_overlay(selection)) {
    return selection;
  }

  if (apply_ambient_idle_crossfade(selection, anim, spec, seed, variant)) {
    return selection;
  }

  if (apply_hold_stance_crossfade(selection, anim, spec, seed, variant)) {
    return selection;
  }

  auto const* combat =
      anim.inputs.combat_visual.authoritative ? &anim.inputs.combat_visual : nullptr;
  if (combat == nullptr || anim.inputs.is_hit_reacting || anim.inputs.is_dying ||
      anim.inputs.is_dead || anim.inputs.is_constructing || anim.inputs.is_healing) {
    return selection;
  }

  auto const base_anim = locomotion_only_context(anim);
  auto const action_anim = action_only_context(anim);
  auto const base_selection = build_selection_for_pose(
      spec, base_anim, Render::Creature::resolve_pose(base_anim.inputs), seed, variant);
  auto const action_selection =
      build_selection_for_pose(spec,
                               action_anim,
                               Render::Creature::resolve_pose(action_anim.inputs),
                               seed,
                               variant);
  bool const moving =
      Render::Creature::is_moving_animation(anim.inputs.movement_state) &&
      base_selection.state != Render::Creature::AnimationStateId::Idle;
  bool const action_keeps_hold_stance =
      action_selection.clip_id == Animation::k_humanoid_hold_bow_attack_clip;

  auto const policy = Animation::resolve_combat_playback_layer_policy({
      .has_authoritative_combat = true,
      .phase = combat->phase,
      .phase_progress = combat->phase_progress,
      .exit_blend_progress = combat->exit_blend_progress,
      .attack_emphasis = combat->attack_emphasis,
      .finisher_attack = combat->finisher_attack,
      .mounted = anim.inputs.is_mounted,
      .moving = moving,
      .forced_displacement = anim.inputs.visual_movement.forced_displacement,
      .preserve_base_stance = anim.inputs.is_in_hold_mode && !action_keeps_hold_stance,
      .rooted_action = anim.inputs.is_in_melee_lock || steps_over_ground(anim),
      .action_state_differs_from_base = action_selection.state != base_selection.state,
      .selection_state_differs_from_base = selection.state != base_selection.state,
      .action_state_differs_from_selection = action_selection.state != selection.state,
  });

  if (policy.use_base_selection) {
    selection = base_selection;
    if (steps_over_ground(anim) &&
        policy.full_body_source == Animation::PlaybackLayerSource::None) {
      (void)apply_locomotion_crossfade(selection, base_anim, spec, seed, variant);
    }
  }

  if (auto const* full_body = selection_from_layer_source(
          policy.full_body_source, base_selection, action_selection);
      full_body != nullptr) {
    selection.full_body_blend = playback_layer_from_selection(
        *full_body,
        policy.full_body_weight,
        Render::Creature::PlaybackLayerMode::FullBodyBlend);
  }

  if (auto const* upper_body = selection_from_layer_source(
          policy.upper_body_source, base_selection, action_selection);
      upper_body != nullptr) {
    selection.upper_body_overlay = playback_layer_from_selection(
        *upper_body,
        policy.upper_body_weight,
        Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
  }

  return selection;
}

} // namespace

} // namespace Render::Creature::Pipeline
