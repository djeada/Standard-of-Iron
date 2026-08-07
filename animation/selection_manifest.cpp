#include "selection_manifest.h"

#include <algorithm>
#include <array>
#include <utility>

namespace Animation {

namespace {

[[nodiscard]] auto smooth01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto variant_index_for_table(
    const VariantTableResolveInputs& inputs) noexcept -> std::pair<bool, std::size_t> {
  if (inputs.table == nullptr || inputs.table->variant_stride == 0U) {
    return {false, 0U};
  }

  if (inputs.table->variant_is_seed_based) {
    return {true,
            seeded_visual_variant_index(inputs.seed, inputs.table->variant_stride)};
  }

  if (inputs.has_variant_index_hint) {
    auto const max_idx = static_cast<std::size_t>(inputs.table->variant_stride) - 1U;
    return {true, std::min<std::size_t>(inputs.variant_index_hint, max_idx)};
  }

  return {false, 0U};
}

} // namespace

ArchetypeVariantTable::ArchetypeVariantTable() noexcept {
  archetype_for_pose.fill(k_invalid_visual_archetype);
  state_for_pose.fill(StateId::Count);
  archetype_for_variant.fill(k_invalid_visual_archetype);
  state_for_variant.fill(StateId::Count);
}

auto seeded_visual_variant_index(std::uint32_t seed,
                                 std::uint8_t stride) noexcept -> std::uint8_t {
  if (stride == 0U) {
    return 0U;
  }

  seed ^= seed >> 16U;
  seed *= 0x7FEB352DU;
  seed ^= seed >> 15U;
  seed *= 0x846CA68BU;
  seed ^= seed >> 16U;
  return static_cast<std::uint8_t>(seed % stride);
}

auto resolve_archetype_variant_override(
    const VariantTableResolveInputs& inputs) noexcept -> VariantTableResolution {
  VariantTableResolution resolved{};
  if (inputs.table == nullptr) {
    return resolved;
  }

  auto const pose_idx = static_cast<std::size_t>(inputs.pose_intent);
  auto const pose_archetype = inputs.table->archetype_for_pose[pose_idx];
  if (pose_archetype != k_invalid_visual_archetype) {
    resolved.archetype = pose_archetype;
    resolved.archetype_changed = true;
  }

  auto const pose_state = inputs.table->state_for_pose[pose_idx];
  if (pose_state != StateId::Count) {
    resolved.state = pose_state;
    resolved.state_changed = true;
  }

  bool const trigger_matches =
      (inputs.table->variant_trigger_pose == PoseIntent::Count) ||
      (inputs.pose_intent == inputs.table->variant_trigger_pose);
  if (!trigger_matches) {
    return resolved;
  }

  auto const [has_variant_idx, variant_idx] = variant_index_for_table(inputs);
  if (!has_variant_idx) {
    return resolved;
  }

  auto const variant_archetype = inputs.table->archetype_for_variant[variant_idx];
  if (variant_archetype != k_invalid_visual_archetype) {
    resolved.archetype = variant_archetype;
    resolved.archetype_changed = true;
  }

  auto const variant_state = inputs.table->state_for_variant[variant_idx];
  if (variant_state != StateId::Count) {
    resolved.state = variant_state;
    resolved.state_changed = true;
  }

  return resolved;
}

auto combat_attack_visual_weight(CombatTransactionPhase phase,
                                 float phase_progress,
                                 float exit_blend_progress) noexcept -> float {
  switch (phase) {
  case CombatTransactionPhase::None:
    return 0.0F;
  case CombatTransactionPhase::Enter:
    return 0.28F * smooth01(phase_progress);
  case CombatTransactionPhase::Anticipation:
    return 0.28F + 0.42F * smooth01(phase_progress);
  case CombatTransactionPhase::Strike:
    return 0.70F + 0.25F * smooth01(phase_progress);
  case CombatTransactionPhase::FollowThrough:
    return 0.95F;
  case CombatTransactionPhase::Recover:
    return 0.95F - 0.15F * smooth01(phase_progress);
  case CombatTransactionPhase::ExitBlend:
    return 0.80F * (1.0F - smooth01(exit_blend_progress));
  }
  return 0.0F;
}

auto resolve_combat_playback_layer_policy(
    const CombatPlaybackLayerPolicyInputs& inputs) noexcept
    -> CombatPlaybackLayerPolicy {
  CombatPlaybackLayerPolicy policy{};
  if (!inputs.has_authoritative_combat || inputs.blocked_by_action_state) {
    return policy;
  }

  float const emphasis =
      std::clamp(inputs.attack_emphasis, 0.72F, inputs.finisher_attack ? 1.35F : 1.18F);
  policy.combat_weight =
      std::clamp(combat_attack_visual_weight(
                     inputs.phase, inputs.phase_progress, inputs.exit_blend_progress) *
                     emphasis,
                 0.0F,
                 1.0F);

  if (inputs.phase == CombatTransactionPhase::ExitBlend &&
      policy.combat_weight <= 0.01F) {
    policy.use_base_selection = true;
    return policy;
  }

  if (inputs.preserve_base_stance) {
    policy.use_base_selection = true;
    return policy;
  }

  if (inputs.rooted_action && inputs.action_state_differs_from_base &&
      policy.combat_weight > 0.01F) {
    policy.use_base_selection = true;
    policy.upper_body_source = PlaybackLayerSource::Action;
    policy.upper_body_weight = policy.combat_weight;
    return policy;
  }

  if (!inputs.mounted && inputs.moving && !inputs.forced_displacement &&
      inputs.phase != CombatTransactionPhase::Recover &&
      inputs.phase != CombatTransactionPhase::ExitBlend &&
      inputs.action_state_differs_from_base && policy.combat_weight > 0.01F) {
    policy.use_base_selection = true;
    policy.upper_body_source = PlaybackLayerSource::Action;
    policy.upper_body_weight = policy.combat_weight;
    return policy;
  }

  float const stance_weight = 1.0F - policy.combat_weight;
  if (inputs.selection_state_differs_from_base) {
    if (stance_weight > k_combat_stance_blend_epsilon) {
      policy.full_body_source = PlaybackLayerSource::Base;
      policy.full_body_weight = stance_weight;
    }
  } else if (inputs.phase == CombatTransactionPhase::ExitBlend &&
             inputs.action_state_differs_from_selection) {
    policy.full_body_source = PlaybackLayerSource::Action;
    policy.full_body_weight = policy.combat_weight;
  }

  return policy;
}

auto resolve_locomotion_crossfade(const LocomotionCrossfadeInputs& inputs) noexcept
    -> LocomotionCrossfade {
  float const locomotion = std::clamp(inputs.locomotion_presence, 0.0F, 1.0F);
  float const run = std::clamp(inputs.run_presence, 0.0F, 1.0F);

  struct Candidate {
    StateId state;
    float weight;
  };
  std::array<Candidate, 3> const candidates{{
      {StateId::Idle, 1.0F - locomotion},
      {StateId::Walk, locomotion * (1.0F - run)},
      {StateId::Run, locomotion * run},
  }};

  LocomotionCrossfade crossfade{};
  crossfade.primary = inputs.resolved;
  crossfade.secondary = inputs.resolved;

  float primary_weight = 0.0F;
  float secondary_weight = -1.0F;
  for (auto const& candidate : candidates) {
    if (candidate.state == crossfade.primary) {
      primary_weight = candidate.weight;
    } else if (candidate.weight > secondary_weight) {
      secondary_weight = candidate.weight;
      crossfade.secondary = candidate.state;
    }
  }

  float const pair_total = primary_weight + std::max(0.0F, secondary_weight);
  if (pair_total <= 1.0e-4F || crossfade.secondary == crossfade.primary) {
    return crossfade;
  }
  crossfade.secondary_weight = std::max(0.0F, secondary_weight) / pair_total;
  crossfade.active = crossfade.secondary_weight > k_locomotion_crossfade_epsilon;
  return crossfade;
}

} // namespace Animation
