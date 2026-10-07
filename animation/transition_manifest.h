#pragma once

#include <array>
#include <cstdint>

#include "clip_manifest.h"

namespace Animation {

enum class TransitionFamily : std::uint8_t {
  Locomotion,
  Stance,
  Attack,
  HitReaction,
  Death,
  Work,
  Riding,
};

struct HumanoidTransitionInputs {
  StateId from_state{StateId::Idle};
  StateId to_state{StateId::Idle};
  bool to_hit_reaction{false};
  bool to_dying{false};
  bool to_working{false};
};

inline constexpr float k_humanoid_transition_min_seconds = 0.06F;
inline constexpr float k_humanoid_transition_max_seconds = 0.30F;

inline constexpr float k_transition_phase_break = 0.30F;

[[nodiscard]] auto transition_family(StateId state,
                                     bool hit_reaction,
                                     bool dying,
                                     bool working) noexcept -> TransitionFamily;

struct HumanoidTransitionTiming {
  float upper_body_seconds{0.0F};
  float lower_body_seconds{0.0F};
};

[[nodiscard]] auto humanoid_transition_timing(
    const HumanoidTransitionInputs& inputs) noexcept -> HumanoidTransitionTiming;

struct ClipMixEntry {
  std::uint16_t clip{k_unmapped_clip};
  float phase{0.0F};
  float weight{0.0F};
  bool loops{true};
};

inline constexpr float k_transition_phase_rewind = 0.06F;

[[nodiscard]] auto same_clip_instance(const ClipMixEntry& before,
                                      const ClipMixEntry& after) noexcept -> bool;

using ClipMix = std::array<ClipMixEntry, 3>;

[[nodiscard]] auto clip_mix_discontinuity(const ClipMix& before,
                                          const ClipMix& after) noexcept -> float;

[[nodiscard]] auto
humanoid_transition_snap_threshold(float delta_seconds) noexcept -> float;

[[nodiscard]] auto
humanoid_transition_link_weight(float elapsed_seconds,
                                float duration_seconds) noexcept -> float;

} // namespace Animation
