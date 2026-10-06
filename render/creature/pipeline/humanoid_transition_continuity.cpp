#include "humanoid_transition_continuity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_playback.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_max_sample_gap_seconds = 0.25F;
constexpr float k_negligible_weight = 1.0e-3F;
constexpr std::size_t k_loop_cache_size = 256U;

auto humanoid_clip_loops(std::uint16_t clip) noexcept -> bool {
  constexpr std::int8_t k_unknown = 0;
  constexpr std::int8_t k_looping = 1;
  constexpr std::int8_t k_one_shot = 2;
  static std::array<std::atomic<std::int8_t>, k_loop_cache_size> cache{};
  if (clip >= k_loop_cache_size) {
    return true;
  }
  std::int8_t const known = cache[clip].load(std::memory_order_relaxed);
  if (known != k_unknown) {
    return known == k_looping;
  }
  auto const playback =
      resolve_bpat_playback(Render::Creature::Bpat::k_species_humanoid, clip, 0.0F);
  if (!playback.valid()) {
    return true;
  }
  cache[clip].store(playback.loops ? k_looping : k_one_shot, std::memory_order_relaxed);
  return playback.loops;
}

auto blend_weight_of(const HumanoidPlaybackLayerSelection& layer) noexcept -> float {
  return layer.active() && layer.clip_id.has_value()
             ? std::clamp(layer.weight, 0.0F, 1.0F)
             : 0.0F;
}

auto requested_state(const HumanoidAnimationSelection& selection) noexcept
    -> Render::Creature::AnimationStateId {
  return blend_weight_of(selection.full_body_blend) >= 0.5F
             ? selection.full_body_blend.state
             : selection.state;
}

auto same_source(const Render::Creature::TransitionSource& lhs,
                 const Render::Creature::TransitionSource& rhs) noexcept -> bool {
  constexpr float k_same_phase = 0.02F;
  float const delta = std::fmod(std::abs(lhs.phase - rhs.phase), 1.0F);
  return lhs.clip_id == rhs.clip_id && std::min(delta, 1.0F - delta) <= k_same_phase;
}

} // namespace

auto requested_clip_mix(const HumanoidAnimationSelection& selection) noexcept
    -> Animation::ClipMix {
  float const blend = blend_weight_of(selection.full_body_blend);
  return {{
      {.clip = selection.clip_id.value_or(Animation::k_unmapped_clip),
       .phase = selection.phase,
       .weight = 1.0F - blend,
       .loops =
           humanoid_clip_loops(selection.clip_id.value_or(Animation::k_unmapped_clip))},
      {.clip = blend > 0.0F ? *selection.full_body_blend.clip_id
                            : Animation::k_unmapped_clip,
       .phase = selection.full_body_blend.phase,
       .weight = blend,
       .loops = blend > 0.0F ? humanoid_clip_loops(*selection.full_body_blend.clip_id)
                             : true},
      {},
  }};
}

auto displayed_clip_mix(const HumanoidAnimationSelection& selection) noexcept
    -> HumanoidShownMix {
  constexpr std::size_t k_candidates = 2U + Render::Creature::k_transition_source_count;
  std::array<Render::Creature::TransitionSource, k_candidates> candidates{};
  std::size_t count = 0U;
  auto add = [&](const Render::Creature::TransitionSource& source) {
    if (source.clip_id == Animation::k_unmapped_clip ||
        std::max(source.upper_body_share, source.lower_body_share) <= 0.0F) {
      return;
    }
    for (std::size_t index = 0; index < count; ++index) {
      if (same_source(candidates[index], source)) {
        candidates[index].upper_body_share += source.upper_body_share;
        candidates[index].lower_body_share += source.lower_body_share;
        return;
      }
    }
    candidates[count++] = source;
  };

  auto const& outgoing = selection.transition;
  bool const transitioning = outgoing.active();
  float const upper_out =
      transitioning ? std::clamp(outgoing.upper_body_weight, 0.0F, 1.0F) : 0.0F;
  float const lower_out =
      transitioning ? std::clamp(outgoing.lower_body_weight, 0.0F, 1.0F) : 0.0F;
  float const blend = blend_weight_of(selection.full_body_blend);
  add({.state = selection.state,
       .clip_id = selection.clip_id.value_or(Animation::k_unmapped_clip),
       .clip_variant = selection.clip_variant,
       .phase = selection.phase,
       .upper_body_share = (1.0F - upper_out) * (1.0F - blend),
       .lower_body_share = (1.0F - lower_out) * (1.0F - blend)});
  if (blend > 0.0F) {
    auto const& layer = selection.full_body_blend;
    add({.state = layer.state,
         .clip_id = *layer.clip_id,
         .clip_variant = layer.clip_variant,
         .phase = layer.phase,
         .upper_body_share = (1.0F - upper_out) * blend,
         .lower_body_share = (1.0F - lower_out) * blend});
  }
  if (transitioning) {
    for (auto const& source : outgoing.sources) {
      auto scaled = source;
      scaled.upper_body_share *= upper_out;
      scaled.lower_body_share *= lower_out;
      add(scaled);
    }
  }

  std::sort(candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(count),
            [](const auto& lhs, const auto& rhs) {
              return lhs.upper_body_share + lhs.lower_body_share >
                     rhs.upper_body_share + rhs.lower_body_share;
            });
  HumanoidShownMix shown{};
  float upper_total = 0.0F;
  float lower_total = 0.0F;
  for (std::size_t index = 0; index < shown.size() && index < count; ++index) {
    shown[index] = candidates[index];
    upper_total += shown[index].upper_body_share;
    lower_total += shown[index].lower_body_share;
  }
  for (auto& source : shown) {
    source.upper_body_share =
        upper_total > 0.0F ? source.upper_body_share / upper_total : 0.0F;
    source.lower_body_share =
        lower_total > 0.0F ? source.lower_body_share / lower_total : 0.0F;
  }
  return shown;
}

auto apply_humanoid_transition_continuity(HumanoidAnimationSelection& selection,
                                          HumanoidTransitionMemory& memory,
                                          const HumanoidTransitionFrame& frame) noexcept
    -> HumanoidTransitionResult {
  HumanoidTransitionResult result{};
  selection.transition = {};
  if (!selection.clip_id.has_value()) {
    return result;
  }

  auto const requested = requested_clip_mix(selection);
  float const delta = memory.valid ? frame.time - memory.sample_time : 0.0F;
  bool const continuous_history =
      memory.valid && delta >= 0.0F && delta <= k_max_sample_gap_seconds;

  HumanoidTransitionLink link =
      continuous_history ? memory.link : HumanoidTransitionLink{};
  if (continuous_history && memory.shown[0].clip_id != Animation::k_unmapped_clip &&
      Animation::clip_mix_discontinuity(memory.requested, requested) >
          Animation::humanoid_transition_snap_threshold(delta)) {
    link.outgoing = memory.shown;
    link.started_at = frame.time;
    link.timing = Animation::humanoid_transition_timing({
        .from_state = memory.shown[0].state,
        .to_state = requested_state(selection),
        .to_hit_reaction = frame.hit_reaction,
        .to_dying = frame.dying,
        .to_working = frame.working,
    });
    result.started = true;
  }

  float upper = 0.0F;
  float lower = 0.0F;
  if (link.active()) {
    float const elapsed = frame.time - link.started_at;
    upper = Animation::humanoid_transition_link_weight(elapsed,
                                                       link.timing.upper_body_seconds);
    lower = Animation::humanoid_transition_link_weight(elapsed,
                                                       link.timing.lower_body_seconds);
  }
  if (std::max(upper, lower) > k_negligible_weight) {
    selection.transition.archetype = selection.resolved_archetype;
    selection.transition.sources = link.outgoing;
    selection.transition.upper_body_weight = upper;
    selection.transition.lower_body_weight = lower;
  } else {
    upper = 0.0F;
    lower = 0.0F;
    link = {};
  }
  result.link_weight = upper;
  result.lower_body_link_weight = lower;

  if (frame.persist) {
    memory.link = link;
    memory.valid = true;
    memory.sample_time = frame.time;
    memory.requested = requested;
    memory.shown = displayed_clip_mix(selection);
  }
  return result;
}

} // namespace Render::Creature::Pipeline
