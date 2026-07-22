#include "humanoid_animation_selection.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "../../gl/humanoid/humanoid_types.h"
#include "../../humanoid/facial_hair_catalog.h"
#include "../../humanoid/skeleton.h"
#include "../archetype_registry.h"
#include "../runtime_bake_guard.h"
#include "animation/selection_manifest.h"
#include "creature_asset.h"
#include "preparation_common.h"

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
    auto const override = Animation::resolve_archetype_variant_override({
        .table = spec.animation_manifest.variant_table,
        .pose_intent = selection.pose.intent,
        .seed = seed,
        .variant_index_hint =
            variant != nullptr ? static_cast<std::uint8_t>(variant->facial_hair.style)
                               : std::uint8_t{0U},
        .has_variant_index_hint = variant != nullptr,
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
  apply_authored_action_clip(selection, anim);
  apply_role_specific_combat_clip(selection, spec, anim);
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

auto finalize_visible_humanoid_spec(UnitVisualSpec spec,
                                    const Render::GL::HumanoidVariant& variant,
                                    const Render::GL::AnimationInputs& anim,
                                    bool has_locomotion) -> UnitVisualSpec {
  spec.owned_legacy_slots =
      spec.owned_legacy_slots | Render::Creature::Pipeline::LegacySlotMask::FacialHair;
  if (!spec.skip_default_facial_hair_archetype) {
    spec.archetype_id =
        Render::Humanoid::resolve_facial_hair_archetype(spec.archetype_id, variant);
  }
  if (Render::GL::guard_pose_amount(anim) > 0.0F && !has_locomotion &&
      !anim.is_attacking) {
    auto pose = anim.shield_formation_pose;
    if (pose == Render::GL::ShieldFormationPose::None) {
      pose = Render::GL::ShieldFormationPose::GuardDefault;
    }
    spec.archetype_id = guard_shield_archetype(spec.archetype_id, pose);
  }
  return spec;
}

auto resolve_humanoid_animation_selection(
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim,
    std::uint32_t seed,
    const Render::GL::HumanoidVariant* variant) noexcept -> HumanoidAnimationSelection {
  HumanoidAnimationSelection selection = build_selection_for_pose(
      spec, anim, Render::Creature::resolve_pose(anim.inputs), seed, variant);

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
