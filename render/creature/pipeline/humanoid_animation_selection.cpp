#include "humanoid_animation_selection.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "animation/locomotion_manifest.h"
#include "animation/selection_manifest.h"
#include "creature_asset.h"
#include "preparation_common.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/facial_hair_catalog.h"
#include "render/humanoid/skeleton.h"

namespace Render::Creature::Pipeline {

namespace {

auto default_humanoid_archetype(Render::Creature::ArchetypeId archetype_id) noexcept
    -> Render::Creature::ArchetypeId {
  return (archetype_id != Render::Creature::k_invalid_archetype)
             ? archetype_id
             : Render::Creature::ArchetypeRegistry::k_humanoid_base;
}

void update_clip_id(HumanoidAnimationSelection& selection) noexcept {
  auto const clip_id =
      Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
          selection.resolved_archetype, selection.state, selection.clip_variant);
  if (clip_id == Render::Creature::ArchetypeDescriptor::k_unmapped_clip) {
    selection.clip_id.reset();
    return;
  }
  selection.clip_id = clip_id;
}

void apply_named_sword_attack_state(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.has_sword_attack_animation || !anim.inputs.is_attacking ||
      anim.inputs.attack_family != Engine::Core::CombatAttackFamily::Sword) {
    return;
  }
  auto const state =
      Animation::state_for_sword_attack_animation(anim.inputs.sword_attack_animation);
  if (state == Render::Creature::AnimationStateId::AttackSword) {
    return;
  }
  selection.state = state;
  selection.phase = humanoid_phase_for_state(anim, selection.state);
  selection.clip_variant = 0U;
  update_clip_id(selection);
}

void apply_authored_action_clip(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.has_authored_action_clip || !anim.inputs.is_attacking ||
      anim.inputs.authored_action_clip == Animation::k_unmapped_clip) {
    return;
  }
  selection.clip_id = anim.inputs.authored_action_clip;
  selection.phase = std::clamp(anim.inputs.authored_action_phase, 0.0F, 1.0F);
  selection.clip_variant = 0U;
}

void apply_melee_reaction_clip(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.is_hit_reacting || anim.inputs.is_mounted || anim.inputs.is_dying ||
      anim.inputs.is_dead || anim.inputs.is_in_hold_mode ||
      anim.inputs.is_exiting_hold || anim.inputs.is_defensive_layout_locked ||
      anim.inputs.has_showcase_clip) {
    return;
  }
  if (selection.state != Render::Creature::AnimationStateId::Idle) {
    return;
  }
  auto const clip = Animation::humanoid_reaction_clip(
      static_cast<std::uint8_t>(anim.inputs.hit_reaction_kind));
  if (clip == Animation::k_unmapped_clip) {
    return;
  }
  selection.clip_id = clip;
  selection.clip_variant = 0U;
  selection.phase = std::clamp(anim.inputs.hit_reaction_progress, 0.0F, 1.0F);
}

void apply_combat_ready_clip(HumanoidAnimationSelection& selection,
                             const Render::GL::HumanoidAnimationContext& anim,
                             std::uint32_t seed) noexcept {
  if (selection.state != Render::Creature::AnimationStateId::Idle ||
      anim.inputs.is_attacking || anim.inputs.is_hit_reacting ||
      anim.inputs.is_mounted || anim.inputs.is_dying || anim.inputs.is_dead ||
      anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold ||
      anim.inputs.is_guarding || anim.inputs.is_exiting_guard ||
      anim.inputs.is_defensive_layout_locked || anim.inputs.is_constructing ||
      anim.inputs.is_healing || anim.inputs.is_casting ||
      anim.inputs.has_showcase_clip ||
      anim.ambient_idle_type != Render::GL::AmbientIdleType::None) {
    return;
  }
  if (!anim.inputs.is_in_melee_lock) {
    return;
  }
  if (Render::Creature::is_moving_animation(anim.inputs.movement_state)) {
    return;
  }
  selection.clip_id = Animation::k_humanoid_combat_ready_clip;
  selection.clip_variant = 0U;
  selection.phase = Animation::wrap_locomotion_phase(
      anim.inputs.time / Animation::k_humanoid_combat_ready_cycle_time +
      Animation::humanoid_idle_breath_offset(seed));
}

void apply_construction_clip(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.is_constructing || anim.inputs.is_attacking ||
      anim.inputs.is_mounted || anim.inputs.is_dying || anim.inputs.is_dead) {
    return;
  }
  auto const role = anim.construction_role == Animation::HumanoidConstructionRole::None
                        ? Animation::HumanoidConstructionRole::Hammer
                        : anim.construction_role;
  selection.clip_id = Animation::humanoid_construction_clip_for_role(role);
  selection.clip_variant = 0U;
  selection.phase =
      humanoid_phase_for_state(anim, Render::Creature::AnimationStateId::AttackSword);
}

void apply_showcase_clip(HumanoidAnimationSelection& selection,
                         const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.has_showcase_clip ||
      anim.inputs.showcase_clip == Animation::k_unmapped_clip) {
    return;
  }
  selection.clip_id = anim.inputs.showcase_clip;
  selection.phase = std::clamp(anim.inputs.showcase_phase, 0.0F, 1.0F);
  selection.clip_variant = 0U;
  selection.full_body_blend = {};
  selection.upper_body_overlay = {};
}

void apply_role_specific_combat_clip(
    HumanoidAnimationSelection& selection,
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim) noexcept {
  if (!anim.inputs.is_attacking || anim.inputs.is_mounted) {
    return;
  }

  if (anim.inputs.is_in_hold_mode) {
    if (anim.inputs.attack_family == Engine::Core::CombatAttackFamily::Spear) {
      selection.clip_id = Animation::k_humanoid_hold_spear_attack_clip;
      selection.clip_variant = 0U;
    } else if (!anim.inputs.is_melee &&
               anim.inputs.attack_family == Engine::Core::CombatAttackFamily::Bow) {
      selection.clip_id = Animation::k_humanoid_hold_bow_attack_clip;
      selection.clip_variant = 0U;
    }
  }

  if (anim.inputs.is_melee &&
      spec.animation_manifest.melee_clip_override != Animation::k_unmapped_clip) {
    selection.clip_id = spec.animation_manifest.melee_clip_override;
    selection.clip_variant = 0U;
  }
}

[[nodiscard]] auto
defensive_layout_clip_for(Render::GL::ShieldFormationPose pose) -> std::uint16_t {
  switch (pose) {
  case Render::GL::ShieldFormationPose::RomanTop:
    return Animation::k_humanoid_testudo_top_clip;
  case Render::GL::ShieldFormationPose::RomanLeft:
    return Animation::k_humanoid_testudo_left_clip;
  case Render::GL::ShieldFormationPose::RomanRight:
    return Animation::k_humanoid_testudo_right_clip;
  case Render::GL::ShieldFormationPose::RomanRear:
    return Animation::k_humanoid_testudo_rear_clip;
  case Render::GL::ShieldFormationPose::RomanFront:
    return Animation::k_humanoid_testudo_front_clip;
  case Render::GL::ShieldFormationPose::CarthageFront:
    return Animation::k_humanoid_carthage_shield_wall_front_clip;
  case Render::GL::ShieldFormationPose::CarthageLeft:
    return Animation::k_humanoid_carthage_shield_wall_left_clip;
  case Render::GL::ShieldFormationPose::CarthageRight:
    return Animation::k_humanoid_carthage_shield_wall_right_clip;
  default:
    break;
  }
  return Animation::k_unmapped_clip;
}

auto guard_shield_turn(Render::GL::ShieldFormationPose pose) -> QMatrix4x4 {
  auto const profile = Animation::guard_shield_attachment_profile(pose);
  QMatrix4x4 guard_turn;
  guard_turn.rotate(profile.base_yaw_degrees, 0.0F, 1.0F, 0.0F);
  if (profile.yaw_degrees != 0.0F) {
    guard_turn.rotate(profile.yaw_degrees, 0.0F, 1.0F, 0.0F);
  }
  if (profile.pitch_degrees != 0.0F) {
    guard_turn.rotate(profile.pitch_degrees, 1.0F, 0.0F, 0.0F);
  }
  if (profile.translate_y != 0.0F || profile.translate_z != 0.0F) {
    guard_turn.translate(0.0F, profile.translate_y, profile.translate_z);
  }
  return guard_turn;
}

auto guard_pose_suffix(Render::GL::ShieldFormationPose pose) -> const char* {
  switch (pose) {
  case Render::GL::ShieldFormationPose::RomanFront:
    return "_guard_shield_roman_front";
  case Render::GL::ShieldFormationPose::RomanTop:
    return "_guard_shield_roman_top";
  case Render::GL::ShieldFormationPose::CarthageFront:
    return "_guard_shield_carthage_front";
  case Render::GL::ShieldFormationPose::RomanLeft:
    return "_guard_shield_roman_left";
  case Render::GL::ShieldFormationPose::RomanRight:
    return "_guard_shield_roman_right";
  case Render::GL::ShieldFormationPose::RomanRear:
    return "_guard_shield_roman_rear";
  case Render::GL::ShieldFormationPose::CarthageLeft:
    return "_guard_shield_carthage_left";
  case Render::GL::ShieldFormationPose::CarthageRight:
    return "_guard_shield_carthage_right";
  case Render::GL::ShieldFormationPose::GuardDefault:
    return "_guard_shield";
  case Render::GL::ShieldFormationPose::None:
    return "_guard_shield_none";
  }
  return "_guard_shield";
}

auto guard_shield_archetype(Render::Creature::ArchetypeId base_archetype,
                            Render::GL::ShieldFormationPose pose)
    -> Render::Creature::ArchetypeId {
  if (base_archetype == Render::Creature::k_invalid_archetype) {
    return base_archetype;
  }

  static std::mutex cache_mutex;
  static std::unordered_map<std::uint64_t, Render::Creature::ArchetypeId> cache;
  std::uint64_t const cache_key =
      (static_cast<std::uint64_t>(static_cast<std::uint32_t>(base_archetype)) << 8U) |
      static_cast<std::uint8_t>(pose);
  std::lock_guard<std::mutex> const lock(cache_mutex);
  if (auto const it = cache.find(cache_key); it != cache.end()) {
    return it->second;
  }

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const* base_desc = registry.get(base_archetype);
  if (base_desc == nullptr || base_desc->bake_attachment_count == 0U) {
    cache.emplace(cache_key, base_archetype);
    return base_archetype;
  }

  if (Render::Creature::runtime_bake_forbidden()) {
    Render::Creature::report_runtime_bake_violation(
        Render::Creature::RuntimeBakeOperation::StaticArchetypeBuild,
        base_desc->debug_name + guard_pose_suffix(pose));
    return base_archetype;
  }

  auto desc = *base_desc;
  desc.debug_name += guard_pose_suffix(pose);
  bool changed = false;

  QMatrix4x4 const guard_turn = guard_shield_turn(pose);
  constexpr auto k_hand_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);

  for (std::uint8_t i = 0; i < desc.bake_attachment_count; ++i) {
    auto& attachment = desc.bake_attachments[i];
    if (attachment.socket_bone_index != k_hand_l_bone) {
      continue;
    }
    attachment.local_offset = attachment.local_offset * guard_turn;
    changed = true;
  }

  if (!changed) {
    cache.emplace(cache_key, base_archetype);
    return base_archetype;
  }

  auto const guard_archetype = registry.register_archetype(desc);
  auto const resolved = guard_archetype != Render::Creature::k_invalid_archetype
                            ? guard_archetype
                            : base_archetype;
  cache.emplace(cache_key, resolved);
  return resolved;
}

auto build_selection_for_pose(const UnitVisualSpec& spec,
                              const Render::GL::HumanoidAnimationContext& anim,
                              const Render::Creature::ResolvedPose& pose,
                              std::uint32_t seed,
                              const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection {
  HumanoidAnimationSelection selection{};
  selection.pose = pose;
  selection.requested_archetype = default_humanoid_archetype(spec.archetype_id);
  selection.resolved_archetype = selection.requested_archetype;
  selection.state = selection.pose.animation_state;
  selection.phase = humanoid_phase_for_state(anim, selection.state);
  selection.clip_variant = humanoid_clip_variant_for_state(
      selection.resolved_archetype, anim, selection.state);

  if (spec.animation_manifest.variant_table != nullptr) {
    auto const job_role = Animation::humanoid_construction_role_for_job(
        static_cast<Animation::HumanoidWorkJob>(anim.inputs.construction_job));
    bool const job_forces_tool = anim.inputs.is_constructing &&
                                 job_role != Animation::HumanoidConstructionRole::None;
    auto const override = Animation::resolve_archetype_variant_override({
        .table = spec.animation_manifest.variant_table,
        .pose_intent = selection.pose.intent,
        .seed = seed,
        .variant_index_hint =
            variant != nullptr ? static_cast<std::uint8_t>(variant->facial_hair.style)
                               : std::uint8_t{0U},
        .has_variant_index_hint = variant != nullptr,
        .forced_variant_index =
            Animation::humanoid_construction_variant_for_role(job_role),
        .has_forced_variant_index = job_forces_tool,
    });

    if (override.archetype_changed) {
      selection.resolved_archetype = override.archetype;
    }
    if (override.state_changed) {
      selection.state = override.state;
    }
    selection.variant_table_changed = override.changed();

    if (override.changed()) {
      selection.phase = humanoid_phase_for_state(anim, selection.state);
      selection.clip_variant = humanoid_clip_variant_for_state(
          selection.resolved_archetype, anim, selection.state);
    }
  }

  apply_named_sword_attack_state(selection, anim);
  update_clip_id(selection);
  apply_melee_reaction_clip(selection, anim);
  apply_combat_ready_clip(selection, anim, seed);
  apply_construction_clip(selection, anim);
  apply_authored_action_clip(selection, anim);
  apply_role_specific_combat_clip(selection, spec, anim);
  apply_showcase_clip(selection, anim);
  return selection;
}

auto playback_layer_from_selection(const HumanoidAnimationSelection& selection,
                                   float weight,
                                   Render::Creature::PlaybackLayerMode mode) noexcept
    -> HumanoidPlaybackLayerSelection {
  HumanoidPlaybackLayerSelection layer{};
  layer.archetype = selection.resolved_archetype;
  layer.state = selection.state;
  layer.phase = selection.phase;
  layer.clip_variant = selection.clip_variant;
  layer.clip_id = selection.clip_id;
  layer.weight = std::clamp(weight, 0.0F, 1.0F);
  layer.mode = mode;
  return layer;
}

auto selection_from_layer_source(Animation::PlaybackLayerSource source,
                                 const HumanoidAnimationSelection& base,
                                 const HumanoidAnimationSelection& action) noexcept
    -> const HumanoidAnimationSelection* {
  switch (source) {
  case Animation::PlaybackLayerSource::Base:
    return &base;
  case Animation::PlaybackLayerSource::Action:
    return &action;
  case Animation::PlaybackLayerSource::None:
    break;
  }
  return nullptr;
}

auto locomotion_only_pose(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::ResolvedPose {
  Render::GL::AnimationInputs base_inputs = anim.inputs;
  base_inputs.is_attacking = false;
  base_inputs.is_casting = false;
  base_inputs.combat_visual = {};
  base_inputs.has_authored_action_clip = false;
  base_inputs.authored_action_clip = Animation::k_unmapped_clip;
  if (anim.inputs.is_in_melee_lock) {

    base_inputs.movement_state = Render::Creature::MovementAnimationState::Idle;
  }
  return Render::Creature::resolve_pose(base_inputs);
}

auto action_only_pose(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::ResolvedPose {
  Render::GL::AnimationInputs action_inputs = anim.inputs;
  if (action_inputs.combat_visual.authoritative) {
    action_inputs.combat_visual.active = true;
    action_inputs.combat_visual.prioritize_action_over_locomotion = true;
  }
  action_inputs.is_attacking = true;
  return Render::Creature::resolve_pose(action_inputs);
}

} // namespace

auto resolve_unit_visual_spec(
    UnitVisualSpec spec, const Render::GL::HumanoidVariant& variant) -> UnitVisualSpec {
  if (!spec.skip_default_facial_hair_archetype) {
    spec.archetype_id =
        Render::Humanoid::resolve_facial_hair_archetype(spec.archetype_id, variant);
    spec.skip_default_facial_hair_archetype = true;
  }
  return spec;
}

auto finalize_visible_humanoid_spec(UnitVisualSpec spec,
                                    const Render::GL::AnimationInputs& anim,
                                    bool has_locomotion) -> UnitVisualSpec {
  if (Render::GL::guard_pose_amount(anim) > 0.0F &&
      (anim.is_defensive_layout_locked || !has_locomotion) &&
      (anim.is_defensive_layout_locked || !anim.is_attacking)) {
    auto pose = anim.shield_formation_pose;
    if (pose == Render::GL::ShieldFormationPose::None) {
      pose = Render::GL::ShieldFormationPose::GuardDefault;
    }
    spec.archetype_id = guard_shield_archetype(spec.archetype_id, pose);
  }
  return spec;
}

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

  if (!is_locomotion_state(selection.state) || anim.inputs.is_attacking ||
      anim.inputs.is_casting || anim.inputs.is_mounted ||
      anim.inputs.has_showcase_clip || anim.inputs.is_in_hold_mode ||
      anim.inputs.is_exiting_hold || anim.inputs.is_guarding ||
      anim.inputs.is_exiting_guard || anim.inputs.is_hit_reacting ||
      anim.inputs.is_healing || anim.inputs.is_constructing || anim.inputs.is_dying ||
      anim.inputs.is_dead) {
    return false;
  }

  auto const crossfade = Animation::resolve_locomotion_crossfade({
      .resolved = selection.state,
      .locomotion_presence = anim.gait.locomotion_presence,
      .run_presence = anim.gait.run_presence,
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

} // namespace

auto resolve_humanoid_animation_selection(
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim,
    std::uint32_t seed,
    const Render::GL::HumanoidVariant* variant) noexcept -> HumanoidAnimationSelection {
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

  if (apply_locomotion_crossfade(selection, anim, spec, seed, variant)) {
    apply_resource_carry_overlay(selection);
    apply_defensive_overlay(selection);
    return selection;
  }

  if (defensive_clip != Animation::k_unmapped_clip) {
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

  auto const base_selection =
      build_selection_for_pose(spec, anim, locomotion_only_pose(anim), seed, variant);
  auto const action_selection =
      build_selection_for_pose(spec, anim, action_only_pose(anim), seed, variant);
  bool const moving =
      Render::Creature::is_moving_animation(anim.inputs.movement_state) &&
      base_selection.state != Render::Creature::AnimationStateId::Idle;

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
      .preserve_base_stance = anim.inputs.is_in_hold_mode,
      .rooted_action = anim.inputs.is_in_melee_lock,
      .action_state_differs_from_base = action_selection.state != base_selection.state,
      .selection_state_differs_from_base = selection.state != base_selection.state,
      .action_state_differs_from_selection = action_selection.state != selection.state,
  });

  if (policy.use_base_selection) {
    selection = base_selection;
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

} // namespace Render::Creature::Pipeline
