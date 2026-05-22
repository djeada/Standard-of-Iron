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

auto guard_shield_turn(Render::GL::ShieldFormationPose pose) -> QMatrix4x4 {
  QMatrix4x4 guard_turn;
  guard_turn.rotate(-90.0F, 0.0F, 1.0F, 0.0F);
  switch (pose) {
  case Render::GL::ShieldFormationPose::RomanFront:
    guard_turn.rotate(180.0F, 0.0F, 1.0F, 0.0F);
    guard_turn.translate(0.0F, 0.05F, 0.08F);
    break;
  case Render::GL::ShieldFormationPose::RomanTop:
    guard_turn.rotate(180.0F, 0.0F, 1.0F, 0.0F);
    guard_turn.rotate(-72.0F, 1.0F, 0.0F, 0.0F);
    guard_turn.translate(0.0F, 0.18F, -0.02F);
    break;
  case Render::GL::ShieldFormationPose::CarthageFront:
    guard_turn.rotate(180.0F, 0.0F, 1.0F, 0.0F);
    guard_turn.rotate(12.0F, 1.0F, 0.0F, 0.0F);
    guard_turn.translate(0.0F, 0.01F, 0.10F);
    break;
  case Render::GL::ShieldFormationPose::GuardDefault:
  case Render::GL::ShieldFormationPose::None:
    break;
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

auto smooth01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto attack_visual_weight(
    const Render::Creature::CombatVisualResolvedState& combat) noexcept -> float {
  using Phase = Render::Creature::CombatVisualTransactionPhase;
  switch (combat.phase) {
  case Phase::None:
    return 0.0F;
  case Phase::Enter:
    return 0.28F * smooth01(combat.phase_progress);
  case Phase::Anticipation:
    return 0.28F + 0.42F * smooth01(combat.phase_progress);
  case Phase::Strike:
    return 0.70F + 0.25F * smooth01(combat.phase_progress);
  case Phase::FollowThrough:
    return 0.95F;
  case Phase::Recover:
    return 0.95F - 0.15F * smooth01(combat.phase_progress);
  case Phase::ExitBlend:
    return 0.80F * (1.0F - smooth01(combat.exit_blend_progress));
  }
  return 0.0F;
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
    auto const* table = spec.animation_manifest.variant_table;
    auto const pose_idx = static_cast<std::size_t>(selection.pose.intent);

    auto const pose_archetype = table->archetype_for_pose[pose_idx];
    if (pose_archetype != Render::Creature::k_invalid_archetype) {
      selection.resolved_archetype = pose_archetype;
      selection.variant_table_changed = true;
    }

    auto const pose_state = table->state_for_pose[pose_idx];
    if (pose_state != Render::Creature::AnimationStateId::Count) {
      selection.state = pose_state;
      selection.variant_table_changed = true;
    }

    if (table->variant_stride > 0U) {
      bool const trigger_matches =
          (table->variant_trigger_pose == Render::Creature::PoseIntent::Count) ||
          (selection.pose.intent == table->variant_trigger_pose);
      if (trigger_matches) {
        std::size_t variant_idx{0U};
        bool have_variant_idx = false;
        if (table->variant_is_seed_based) {
          variant_idx = seeded_variant_index(seed, table->variant_stride);
          have_variant_idx = true;
        } else if (variant != nullptr) {
          variant_idx = std::min<std::size_t>(
              static_cast<std::size_t>(variant->facial_hair.style),
              static_cast<std::size_t>(table->variant_stride) - 1U);
          have_variant_idx = true;
        }

        if (have_variant_idx) {
          auto const variant_archetype = table->archetype_for_variant[variant_idx];
          if (variant_archetype != Render::Creature::k_invalid_archetype) {
            selection.resolved_archetype = variant_archetype;
            selection.variant_table_changed = true;
          }

          auto const variant_state = table->state_for_variant[variant_idx];
          if (variant_state != Render::Creature::AnimationStateId::Count) {
            selection.state = variant_state;
            selection.variant_table_changed = true;
          }
        }
      }
    }

    if (selection.variant_table_changed) {
      selection.phase = humanoid_phase_for_state(anim, selection.state);
      selection.clip_variant = humanoid_clip_variant_for_state(
          selection.resolved_archetype, anim, selection.state);
    }
  }

  update_clip_id(selection);
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
  layer.weight = std::clamp(weight, 0.0F, 1.0F);
  layer.mode = mode;
  return layer;
}

auto locomotion_only_pose(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::ResolvedPose {
  Render::GL::AnimationInputs base_inputs = anim.inputs;
  base_inputs.is_attacking = false;
  base_inputs.is_casting = false;
  base_inputs.combat_visual = {};
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
  float const emphasis = std::clamp(
      combat->attack_emphasis, 0.72F, combat->finisher_attack ? 1.35F : 1.18F);
  float const combat_weight =
      std::clamp(attack_visual_weight(*combat) * emphasis, 0.0F, 1.0F);
  if (combat->phase == Render::Creature::CombatVisualTransactionPhase::ExitBlend &&
      combat_weight <= 0.01F) {
    return base_selection;
  }

  bool const moving =
      Render::Creature::is_moving_animation(anim.inputs.movement_state) &&
      base_selection.state != Render::Creature::AnimationStateId::Idle;

  if (!anim.inputs.is_mounted && moving &&
      !anim.inputs.visual_movement.forced_displacement &&
      combat->phase != Render::Creature::CombatVisualTransactionPhase::Recover &&
      combat->phase != Render::Creature::CombatVisualTransactionPhase::ExitBlend &&
      action_selection.state != base_selection.state && combat_weight > 0.01F) {
    selection = base_selection;
    selection.upper_body_overlay = playback_layer_from_selection(
        action_selection,
        combat_weight,
        Render::Creature::PlaybackLayerMode::UpperBodyOverlay);
    return selection;
  }

  using Phase = Render::Creature::CombatVisualTransactionPhase;
  if (combat->phase == Phase::Enter || combat->phase == Phase::Anticipation) {
    if (selection.state != base_selection.state) {
      selection.full_body_blend = playback_layer_from_selection(
          base_selection,
          1.0F - combat_weight,
          Render::Creature::PlaybackLayerMode::FullBodyBlend);
    }
  } else if (combat->phase == Phase::ExitBlend) {
    if (selection.state != base_selection.state) {
      selection.full_body_blend = playback_layer_from_selection(
          base_selection,
          1.0F - combat_weight,
          Render::Creature::PlaybackLayerMode::FullBodyBlend);
    } else if (action_selection.state != selection.state) {
      selection.full_body_blend = playback_layer_from_selection(
          action_selection,
          combat_weight,
          Render::Creature::PlaybackLayerMode::FullBodyBlend);
    }
  }
  return selection;
}

} // namespace Render::Creature::Pipeline
