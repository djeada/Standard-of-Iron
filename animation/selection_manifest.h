#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "combat_manifest.h"
#include "pose_manifest.h"

namespace Animation {

using VisualArchetypeId = std::uint16_t;
inline constexpr VisualArchetypeId k_invalid_visual_archetype =
    static_cast<VisualArchetypeId>(0xFFFFu);
inline constexpr std::size_t k_max_visual_variants = 8U;

struct ArchetypeVariantTable {
  std::array<VisualArchetypeId, pose_intent_count()> archetype_for_pose{};
  std::array<StateId, pose_intent_count()> state_for_pose{};

  PoseIntent variant_trigger_pose{PoseIntent::Count};
  std::uint8_t variant_stride{0U};
  bool variant_is_seed_based{false};

  std::array<VisualArchetypeId, k_max_visual_variants> archetype_for_variant{};
  std::array<StateId, k_max_visual_variants> state_for_variant{};

  ArchetypeVariantTable() noexcept;
};

struct VariantTableResolveInputs {
  const ArchetypeVariantTable* table{nullptr};
  PoseIntent pose_intent{PoseIntent::Idle};
  std::uint32_t seed{0U};
  std::uint8_t variant_index_hint{0U};
  bool has_variant_index_hint{false};
};

struct VariantTableResolution {
  VisualArchetypeId archetype{k_invalid_visual_archetype};
  StateId state{StateId::Count};
  bool archetype_changed{false};
  bool state_changed{false};

  [[nodiscard]] auto changed() const noexcept -> bool {
    return archetype_changed || state_changed;
  }
};

enum class PlaybackLayerSource : std::uint8_t {
  None,
  Base,
  Action,
};

struct CombatPlaybackLayerPolicyInputs {
  bool has_authoritative_combat{false};
  bool blocked_by_action_state{false};
  CombatTransactionPhase phase{CombatTransactionPhase::None};
  float phase_progress{0.0F};
  float exit_blend_progress{0.0F};
  float attack_emphasis{1.0F};
  bool finisher_attack{false};
  bool mounted{false};
  bool moving{false};
  bool forced_displacement{false};
  // A persistent stance owns the complete pose, including weapon sockets.
  bool preserve_base_stance{false};
  // The action belongs to a gameplay transaction that owns the creature root
  // (formation melee, for example). Keep locomotion/stance on the base layer
  // and apply the authored action to the upper body only.
  bool rooted_action{false};
  bool action_state_differs_from_base{false};
  bool selection_state_differs_from_base{false};
  bool action_state_differs_from_selection{false};
};

struct CombatPlaybackLayerPolicy {
  bool use_base_selection{false};
  PlaybackLayerSource full_body_source{PlaybackLayerSource::None};
  float full_body_weight{0.0F};
  PlaybackLayerSource upper_body_source{PlaybackLayerSource::None};
  float upper_body_weight{0.0F};
  float combat_weight{0.0F};
};

[[nodiscard]] auto
seeded_visual_variant_index(std::uint32_t seed,
                            std::uint8_t stride) noexcept -> std::uint8_t;

[[nodiscard]] auto resolve_archetype_variant_override(
    const VariantTableResolveInputs& inputs) noexcept -> VariantTableResolution;

[[nodiscard]] auto
combat_attack_visual_weight(CombatTransactionPhase phase,
                            float phase_progress,
                            float exit_blend_progress) noexcept -> float;

[[nodiscard]] auto resolve_combat_playback_layer_policy(
    const CombatPlaybackLayerPolicyInputs& inputs) noexcept
    -> CombatPlaybackLayerPolicy;

} // namespace Animation
