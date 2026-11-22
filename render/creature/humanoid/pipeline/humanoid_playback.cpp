#include "render/creature/humanoid/pipeline/humanoid_playback.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/playback_manifest.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/gpu/anim_key.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/pose_intent.h"

namespace Render::Creature::Pipeline {

auto resolved_humanoid_pose(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::ResolvedPose {
  return Render::Creature::resolve_pose(anim.inputs);
}

auto humanoid_state_for_intent(Render::Creature::PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  return Render::Creature::animation_state_for_intent(intent);
}

auto humanoid_state_for_anim(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::AnimationStateId {
  return resolved_humanoid_pose(anim).animation_state;
}

namespace {

auto humanoid_playback_phase_inputs_for_state(
    const Render::GL::HumanoidAnimationContext& anim,
    Render::Creature::AnimationStateId state) noexcept
    -> Animation::HumanoidPlaybackPhaseInputs {
  return {
      .state = state,
      .is_mounted = anim.inputs.is_mounted,
      .is_attacking = anim.inputs.is_attacking,
      .is_melee = anim.inputs.is_melee,
      .movement_state = anim.inputs.movement_state,
      .is_constructing = anim.inputs.is_constructing,
      .construction_role = anim.construction_role,
      .construction_progress = anim.inputs.construction_progress,
      .construction_jitter_seed = anim.jitter_seed,
      .is_in_hold_mode = anim.inputs.is_in_hold_mode,
      .is_exiting_hold = anim.inputs.is_exiting_hold,
      .hold_entry_progress = anim.inputs.hold_entry_progress,
      .hold_exit_progress = anim.inputs.hold_exit_progress,
      .is_guarding = anim.inputs.is_guarding,
      .is_exiting_guard = anim.inputs.is_exiting_guard,
      .guard_pose_progress = anim.inputs.guard_pose_progress,
      .death_progress = anim.inputs.death_progress,
      .attack_phase = anim.attack_phase,
      .ambient_idle = anim.ambient_idle_type,
      .ambient_idle_phase = anim.ambient_idle_phase,
      .idle_breath_phase = anim.idle_breath_phase,
      .gait_cycle_phase = anim.gait.cycle_phase,
  };
}

} // namespace

auto humanoid_phase_for_state(const Render::GL::HumanoidAnimationContext& anim,
                              Render::Creature::AnimationStateId state) noexcept
    -> float {
  return Animation::resolve_humanoid_playback_phase(
      humanoid_playback_phase_inputs_for_state(anim, state));
}

auto humanoid_idle_breath_phase_for_lod(float sample_time,
                                        std::uint32_t inst_seed,
                                        Render::Creature::CreatureLOD lod,
                                        bool template_prewarm) noexcept -> float {

  float const phase = template_prewarm
                          ? sample_time
                          : sample_time / Animation::k_humanoid_idle_breath_cycle_time +
                                Animation::humanoid_idle_breath_offset(inst_seed);

  if (lod == Render::Creature::CreatureLOD::Full) {
    return Animation::wrap_locomotion_phase(phase);
  }

  constexpr auto k_steps = static_cast<float>(Render::GL::k_anim_frame_count - 1U);
  float const wrapped = Animation::wrap_locomotion_phase(phase);
  return Animation::wrap_locomotion_phase(std::round(wrapped * k_steps) / k_steps);
}

namespace {

auto default_humanoid_archetype(Render::Creature::ArchetypeId archetype_id) noexcept
    -> Render::Creature::ArchetypeId {
  return (archetype_id != Render::Creature::k_invalid_archetype)
             ? archetype_id
             : Render::Creature::ArchetypeRegistry::k_humanoid_base;
}

auto humanoid_variant_inputs_for_state(const Render::GL::HumanoidAnimationContext& anim,
                                       Render::Creature::AnimationStateId state,
                                       std::uint8_t available_variant_count) noexcept
    -> Animation::HumanoidClipVariantInputs {
  return {
      .state = state,
      .is_constructing = anim.inputs.is_constructing,
      .construction_role = anim.construction_role,
      .construction_jitter_seed = anim.jitter_seed,
      .death_variant = anim.inputs.death_variant,
      .attack_variant = anim.inputs.attack_variant,
      .ambient_idle = anim.ambient_idle_type,
      .available_variant_count = available_variant_count,
  };
}

} // namespace

auto humanoid_clip_variant_for_state(Render::Creature::ArchetypeId archetype_id,
                                     const Render::GL::HumanoidAnimationContext& anim,
                                     Render::Creature::AnimationStateId state) noexcept
    -> std::uint8_t {
  auto const resolved_archetype = default_humanoid_archetype(archetype_id);
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const variant_count = registry.clip_variant_count(resolved_archetype, state);
  if (variant_count <= 1U) {
    return 0U;
  }

  if (state == Render::Creature::AnimationStateId::Idle &&
      registry.bpat_clip(resolved_archetype, state) !=
          Render::Creature::k_humanoid_idle_clip) {
    return 0U;
  }
  return Animation::resolve_humanoid_clip_variant(
      humanoid_variant_inputs_for_state(anim, state, variant_count));
}

auto humanoid_phase_for_anim(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> float {
  return humanoid_phase_for_state(anim, humanoid_state_for_anim(anim));
}

auto humanoid_clip_variant_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> std::uint8_t {
  return humanoid_clip_variant_for_state(
      archetype_id, anim, humanoid_state_for_anim(anim));
}

auto humanoid_bpat_playback_for_anim(Render::Creature::ArchetypeId archetype_id,
                                     std::uint32_t species_id,
                                     const Render::GL::HumanoidAnimationContext&
                                         anim) noexcept -> std::optional<BpatPlayback> {
  using Render::Creature::ArchetypeDescriptor;

  archetype_id = default_humanoid_archetype(archetype_id);

  auto const resolved_pose = resolved_humanoid_pose(anim);
  auto const state = resolved_pose.animation_state;

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto clip_variant = anim.inputs.is_in_hold_mode
                          ? std::uint8_t{0U}
                          : humanoid_clip_variant_for_state(archetype_id, anim, state);
  auto const base_clip_id = registry.resolve_bpat_clip(archetype_id, state, 0U);
  auto clip_id = registry.resolve_bpat_clip(archetype_id, state, clip_variant);
  if (clip_id == ArchetypeDescriptor::k_unmapped_clip) {
    return std::nullopt;
  }
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr) {
    return std::nullopt;
  }

  if (clip_id != base_clip_id &&
      !blob->clip_is_variant_of(base_clip_id, clip_id, clip_variant)) {
    clip_variant = 0U;
    clip_id = base_clip_id;
  }
  if (clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const playback =
      resolve_bpat_playback(blob, clip_id, humanoid_phase_for_state(anim, state));
  if (!playback.valid()) {
    return std::nullopt;
  }

  return BpatPlayback{playback.clip_id,
                      static_cast<std::uint16_t>(playback.frame_in_clip)};
}

auto humanoid_clip_contact_y(Render::Creature::ArchetypeId archetype_id,
                             std::uint32_t species_id,
                             const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> std::optional<float> {
  auto const playback = humanoid_bpat_playback_for_anim(archetype_id, species_id, anim);
  if (!playback.has_value()) {
    return std::nullopt;
  }

  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || playback->clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(playback->clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  auto const contacts = blob->frame_contacts();
  std::uint32_t const global_frame = clip.frame_offset + playback->frame_in_clip;
  if (global_frame >= contacts.size()) {
    return std::nullopt;
  }
  return contacts[global_frame].foot_y;
}

auto grounded_humanoid_contact_y(
    Render::Creature::ArchetypeId archetype_id,
    std::uint32_t species_id,
    const Render::GL::HumanoidPose& pose,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> float {
  if (auto const clip_contact = humanoid_clip_contact_y(archetype_id, species_id, anim);
      clip_contact.has_value()) {
    return *clip_contact;
  }
  return std::min(pose.foot_l.y(), pose.foot_r.y());
}

} // namespace Render::Creature::Pipeline
