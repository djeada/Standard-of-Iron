#pragma once

#include <algorithm>
#include <array>

#include "animation/transition_manifest.h"
#include "humanoid_animation_selection.h"
#include "render/creature/render_request.h"

namespace Render::Creature::Pipeline {

using HumanoidShownMix = std::array<Render::Creature::TransitionSource,
                                    Render::Creature::k_transition_source_count>;

struct HumanoidTransitionLink {
  HumanoidShownMix outgoing{};
  float started_at{0.0F};
  Animation::HumanoidTransitionTiming timing{};

  [[nodiscard]] auto active() const noexcept -> bool {
    return outgoing[0].clip_id != Animation::k_unmapped_clip &&
           std::max(timing.upper_body_seconds, timing.lower_body_seconds) > 0.0F;
  }
};

struct HumanoidTransitionMemory {
  bool valid{false};
  float sample_time{0.0F};
  Animation::ClipMix requested{};
  HumanoidShownMix shown{};
  HumanoidTransitionLink link{};
};

struct HumanoidTransitionFrame {
  float time{0.0F};
  bool persist{true};
  bool hit_reaction{false};
  bool dying{false};
  bool working{false};
};

struct HumanoidTransitionResult {
  float link_weight{0.0F};
  float lower_body_link_weight{0.0F};
  bool started{false};
};

inline constexpr float k_overlay_weight_rate_per_second = 2.1F;

inline constexpr float k_overlay_takeover_release_seconds = 0.12F;

struct HumanoidOverlayMemory {
  Render::Creature::PlaybackLayerRequest shown{};
  float weight_velocity{0.0F};
  float sample_time{0.0F};
  bool valid{false};

  bool shown_combat{false};
};

struct HumanoidOverlayFrame {
  float time{0.0F};
  bool persist{true};

  bool combat_pose{false};

  bool full_body_takeover{false};
};

[[nodiscard]] auto
advance_humanoid_overlay(const Render::Creature::PlaybackLayerRequest& requested,
                         HumanoidOverlayMemory& memory,
                         const HumanoidOverlayFrame& frame) noexcept
    -> Render::Creature::PlaybackLayerRequest;

[[nodiscard]] auto requested_clip_mix(
    const HumanoidAnimationSelection& selection) noexcept -> Animation::ClipMix;

[[nodiscard]] auto displayed_clip_mix(
    const HumanoidAnimationSelection& selection) noexcept -> HumanoidShownMix;

auto apply_humanoid_transition_continuity(HumanoidAnimationSelection& selection,
                                          HumanoidTransitionMemory& memory,
                                          const HumanoidTransitionFrame& frame) noexcept
    -> HumanoidTransitionResult;

} // namespace Render::Creature::Pipeline
