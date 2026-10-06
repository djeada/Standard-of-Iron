#include <cmath>
#include <gtest/gtest.h>

#include "animation/clip_manifest.h"
#include "animation/transition_manifest.h"
#include "render/creature/pipeline/humanoid_transition_continuity.h"

namespace {

using Render::Creature::AnimationStateId;
using Render::Creature::Pipeline::apply_humanoid_transition_continuity;
using Render::Creature::Pipeline::HumanoidAnimationSelection;
using Render::Creature::Pipeline::HumanoidTransitionFrame;
using Render::Creature::Pipeline::HumanoidTransitionMemory;

constexpr Render::Creature::ArchetypeId k_archetype = 1U;
constexpr float k_frame = 1.0F / 60.0F;

auto single_clip(AnimationStateId state,
                 std::uint16_t clip,
                 float phase) -> HumanoidAnimationSelection {
  HumanoidAnimationSelection selection{};
  selection.requested_archetype = k_archetype;
  selection.resolved_archetype = k_archetype;
  selection.state = state;
  selection.clip_id = clip;
  selection.phase = phase;
  return selection;
}

void blend_onto(HumanoidAnimationSelection& selection,
                AnimationStateId state,
                std::uint16_t clip,
                float phase,
                float weight) {
  selection.full_body_blend.archetype = k_archetype;
  selection.full_body_blend.state = state;
  selection.full_body_blend.clip_id = clip;
  selection.full_body_blend.phase = phase;
  selection.full_body_blend.weight = weight;
  selection.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
}

auto step(HumanoidAnimationSelection selection,
          HumanoidTransitionMemory& memory,
          float time,
          bool hit_reaction = false) -> HumanoidAnimationSelection {
  HumanoidTransitionFrame frame{};
  frame.time = time;
  frame.hit_reaction = hit_reaction;
  (void)apply_humanoid_transition_continuity(selection, memory, frame);
  return selection;
}

TEST(TransitionManifest, EveryTransitionIsBoundedAndHitReactionsReadFastest) {
  using Animation::StateId;
  for (auto const from :
       {StateId::Idle, StateId::Run, StateId::Hold, StateId::AttackSword}) {
    for (auto const to :
         {StateId::Idle, StateId::Walk, StateId::Hold, StateId::AttackBow}) {
      auto const timing =
          Animation::humanoid_transition_timing({.from_state = from, .to_state = to});
      EXPECT_GE(timing.upper_body_seconds,
                Animation::k_humanoid_transition_min_seconds);
      EXPECT_LE(timing.upper_body_seconds,
                Animation::k_humanoid_transition_max_seconds);
      EXPECT_GE(timing.lower_body_seconds,
                Animation::k_humanoid_transition_min_seconds);
      EXPECT_LE(timing.lower_body_seconds,
                Animation::k_humanoid_transition_max_seconds);
    }
  }
  auto const reaction =
      Animation::humanoid_transition_timing({.from_state = StateId::AttackSword,
                                             .to_state = StateId::Idle,
                                             .to_hit_reaction = true});
  auto const guard = Animation::humanoid_transition_timing(
      {.from_state = StateId::Run, .to_state = StateId::Hold});
  EXPECT_LT(reaction.upper_body_seconds, guard.upper_body_seconds);
  EXPECT_GT(reaction.lower_body_seconds, reaction.upper_body_seconds)
      << "planted feet must not be dragged at the speed the flinch reads";
}

TEST(TransitionManifest, SmoothCrossfadesNeverCountAsSnaps) {
  float const dt = k_frame;
  float const smooth_step = dt / 0.12F;
  Animation::ClipMix const before{{{.clip = 0U, .phase = 0.2F, .weight = 0.5F},
                                   {.clip = 6U, .phase = 0.4F, .weight = 0.5F},
                                   {}}};
  Animation::ClipMix const after{
      {{.clip = 0U, .phase = 0.21F, .weight = 0.5F - smooth_step},
       {.clip = 6U, .phase = 0.42F, .weight = 0.5F + smooth_step},
       {}}};
  EXPECT_LT(Animation::clip_mix_discontinuity(before, after),
            Animation::humanoid_transition_snap_threshold(dt));
}

TEST(TransitionManifest, AClipSwapAndAOneShotRewindAreSnaps) {
  float const dt = k_frame;
  Animation::ClipMix const running{
      {{.clip = 7U, .phase = 0.9F, .weight = 1.0F}, {}, {}}};
  Animation::ClipMix const guarding{
      {{.clip = 8U, .phase = 0.0F, .weight = 1.0F}, {}, {}}};
  EXPECT_GT(Animation::clip_mix_discontinuity(running, guarding),
            Animation::humanoid_transition_snap_threshold(dt));

  Animation::ClipMix const reacting{
      {{.clip = 69U, .phase = 0.14F, .weight = 1.0F, .loops = false}, {}, {}}};
  Animation::ClipMix const restarted{
      {{.clip = 69U, .phase = 0.05F, .weight = 1.0F, .loops = false}, {}, {}}};
  EXPECT_GT(Animation::clip_mix_discontinuity(reacting, restarted),
            Animation::humanoid_transition_snap_threshold(dt));

  Animation::ClipMix const walking_back{
      {{.clip = 6U, .phase = 0.14F, .weight = 1.0F, .loops = true}, {}, {}}};
  Animation::ClipMix const stepped_back{
      {{.clip = 6U, .phase = 0.11F, .weight = 1.0F, .loops = true}, {}, {}}};
  EXPECT_LT(Animation::clip_mix_discontinuity(walking_back, stepped_back),
            Animation::humanoid_transition_snap_threshold(dt))
      << "a reverse gait plays its loop backwards every frame";
}

TEST(HumanoidTransitionContinuity, ACutBetweenClipsStartsFromWhatWasOnScreen) {
  HumanoidTransitionMemory memory{};
  float time = 1.0F;
  auto run = single_clip(AnimationStateId::Run, Animation::k_humanoid_run_clip, 0.80F);
  (void)step(run, memory, time);

  time += k_frame;
  auto guard =
      single_clip(AnimationStateId::Run, Animation::k_humanoid_run_clip, 0.83F);
  blend_onto(
      guard, AnimationStateId::Hold, Animation::k_humanoid_hold_clip, 0.0F, 1.0F);
  auto const shown = step(guard, memory, time);

  ASSERT_TRUE(shown.transition.active());
  EXPECT_EQ(shown.transition.sources[0].clip_id, Animation::k_humanoid_run_clip);
  EXPECT_FLOAT_EQ(shown.transition.sources[0].phase, 0.80F);
  EXPECT_FLOAT_EQ(shown.transition.upper_body_weight, 1.0F);
  EXPECT_FLOAT_EQ(shown.transition.lower_body_weight, 1.0F);
  EXPECT_EQ(shown.full_body_blend.clip_id, Animation::k_humanoid_hold_clip)
      << "the selection's own layers are untouched";

  float previous = shown.transition.upper_body_weight;
  bool finished = false;
  for (int frame = 0; frame < 30; ++frame) {
    time += k_frame;
    auto const later = step(guard, memory, time);
    if (!later.transition.active()) {
      finished = true;
      break;
    }
    EXPECT_LE(later.transition.upper_body_weight, previous);
    previous = later.transition.upper_body_weight;
  }
  EXPECT_TRUE(finished) << "the outgoing pose must be gone within the bounded duration";
}

TEST(HumanoidTransitionContinuity, ASmoothCrossfadeIsLeftAlone) {
  HumanoidTransitionMemory memory{};
  float time = 2.0F;
  float presence = 1.0F;
  for (int frame = 0; frame < 40; ++frame) {
    auto stopping = single_clip(AnimationStateId::Idle,
                                Animation::k_humanoid_idle_clip,
                                0.1F + 0.01F * static_cast<float>(frame));
    blend_onto(stopping,
               AnimationStateId::Walk,
               Animation::k_humanoid_walk_clip,
               0.3F + 0.02F * static_cast<float>(frame),
               presence);
    auto const shown = step(stopping, memory, time);
    EXPECT_FALSE(shown.transition.active()) << "frame " << frame;
    presence *= std::exp(-k_frame / 0.12F);
    time += k_frame;
  }
}

TEST(HumanoidTransitionContinuity, AnInterruptedTransitionBlendsFromTheWholeMix) {
  HumanoidTransitionMemory memory{};
  float time = 3.0F;
  (void)step(single_clip(AnimationStateId::Hold, Animation::k_humanoid_hold_clip, 0.5F),
             memory,
             time);
  time += k_frame;
  auto reacting = single_clip(
      AnimationStateId::Idle, Animation::k_humanoid_react_flinch_clip, 0.0F);
  ASSERT_TRUE(step(reacting, memory, time, true).transition.active());

  for (int frame = 0; frame < 3; ++frame) {
    time += k_frame;
    reacting.phase += 0.05F;
    (void)step(reacting, memory, time, true);
  }

  time += k_frame;
  auto restarted =
      single_clip(AnimationStateId::Idle, Animation::k_humanoid_react_block_clip, 0.0F);
  auto const second = step(restarted, memory, time, true);
  ASSERT_TRUE(second.transition.active());
  bool holds_guard = false;
  bool holds_reaction = false;
  for (auto const& source : second.transition.sources) {
    holds_guard = holds_guard || (source.clip_id == Animation::k_humanoid_hold_clip &&
                                  source.lower_body_share > 0.0F);
    holds_reaction =
        holds_reaction || (source.clip_id == Animation::k_humanoid_react_flinch_clip &&
                           source.upper_body_share > 0.0F);
  }
  EXPECT_TRUE(holds_guard)
      << "the legs were still in the guard when the next hit landed";
  EXPECT_TRUE(holds_reaction);
}

TEST(HumanoidTransitionContinuity, ALongGapStartsFresh) {
  HumanoidTransitionMemory memory{};
  (void)step(single_clip(AnimationStateId::Run, Animation::k_humanoid_run_clip, 0.2F),
             memory,
             1.0F);
  auto const after_gap =
      step(single_clip(AnimationStateId::Hold, Animation::k_humanoid_hold_clip, 0.0F),
           memory,
           2.0F);
  EXPECT_FALSE(after_gap.transition.active())
      << "a body that was not sampled for a second has nothing on screen to blend from";
}

} // namespace
