#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "game/wildlife/wildlife_species.h"
#include "render/entity/wildlife/sheep_renderer.h"
#include "render/entity/wildlife/sheep_slapstick.h"
#include "render/entity/wildlife/wildlife_draw_state.h"
#include "render/entity/wildlife/wolf_renderer.h"

namespace {

using Render::Creature::AnimationStateId;
using Render::GL::Wildlife::DrawState;
using Render::GL::Wildlife::GaitTier;
using Render::GL::Wildlife::resolve_sheep_clip;
using Render::GL::Wildlife::resolve_wolf_clip;

TEST(WildlifeClipSelection, ABitingWolfThatIsStillRunningKeepsRunning) {
  DrawState state;
  state.behavior = Game::Wildlife::Behavior::Stalk;
  state.bite_progress = 0.2F;

  EXPECT_EQ(resolve_wolf_clip(state, GaitTier::Run), AnimationStateId::Run);
  EXPECT_EQ(resolve_wolf_clip(state, GaitTier::Stand), AnimationStateId::AttackMelee);
}

TEST(WildlifeClipSelection, AStartledSheepThatIsBoltingKeepsRunning) {
  DrawState state;
  state.behavior = Game::Wildlife::Behavior::Flee;
  state.alert = true;
  state.flinch_progress = 0.3F;

  EXPECT_EQ(resolve_sheep_clip(state, GaitTier::Run, false), AnimationStateId::Run);
  EXPECT_EQ(resolve_sheep_clip(state, GaitTier::Walk, false), AnimationStateId::Walk);
  EXPECT_EQ(resolve_sheep_clip(state, GaitTier::Stand, false),
            AnimationStateId::WildlifeStartle);
}

TEST(WildlifeClipSelection, DeathOutranksEverySpeed) {
  DrawState wolf;
  wolf.bite_progress = 0.5F;
  wolf.death_progress = 0.1F;
  EXPECT_EQ(resolve_wolf_clip(wolf, GaitTier::Run), AnimationStateId::Die);
  wolf.dead = true;
  EXPECT_EQ(resolve_wolf_clip(wolf, GaitTier::Run), AnimationStateId::Dead);

  DrawState sheep;
  sheep.flinch_progress = 0.5F;
  sheep.death_progress = 0.1F;
  EXPECT_EQ(resolve_sheep_clip(sheep, GaitTier::Run, false), AnimationStateId::Die);
  sheep.dead = true;
  EXPECT_EQ(resolve_sheep_clip(sheep, GaitTier::Run, false), AnimationStateId::Dead);
}

TEST(WildlifeClipSelection, WolfReactsWithoutInterruptingACommittedBite) {
  DrawState state;
  state.flinch_progress = 0.1F;
  EXPECT_EQ(resolve_wolf_clip(state, GaitTier::Stand),
            AnimationStateId::WildlifeStartle);
  state.bite_progress = 0.2F;
  EXPECT_EQ(resolve_wolf_clip(state, GaitTier::Stand), AnimationStateId::AttackMelee);
}

TEST(WildlifeClipSelection, AGrazingSheepStillHoldsItsGrazePose) {
  DrawState state;
  state.grazing = true;
  EXPECT_EQ(resolve_sheep_clip(state, GaitTier::Stand, true), AnimationStateId::Hold);
}

TEST(WildlifeLocomotion, FeetTrackDistanceDuringAccelerationAndStop) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17001U;
  (void)gait_speed(state);
  float const first = gait_phase(state, 1.0F);
  state.time = 0.1F;
  state.distance = 0.2F;
  (void)gait_speed(state);
  float const moved = gait_phase(state, 1.0F);
  float const expected = first + 0.2F < 1.0F ? first + 0.2F : first - 0.8F;
  EXPECT_NEAR(moved, expected, 1.0e-5F);

  EXPECT_FLOAT_EQ(gait_phase(state, 1.0F), moved);
  state.time = 0.2F;
  EXPECT_GT(gait_speed(state), 0.0F);
  EXPECT_FLOAT_EQ(gait_phase(state, 1.0F), moved);
}

TEST(WildlifeLocomotion, LongFrameDoesNotInventRunningSpeed) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17002U;
  (void)gait_speed(state);
  state.time = 1.0F;
  state.distance = 1.0F;
  EXPECT_LE(gait_speed(state), 1.0F);
}

TEST(WildlifeLocomotion, AStoppedWolfShowsItsBiteWindupImmediately) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17003U;
  (void)gait_speed(state);
  state.time = 0.1F;
  state.distance = 0.4F;
  EXPECT_GT(gait_speed(state), 0.5F);
  (void)gait_phase(state, 1.0F);
  state.time = 0.2F;
  state.bite_progress = 0.01F;
  float const speed = gait_speed(state);
  auto const tier = resolve_gait_tier(state, speed / 4.6F, 0.02F, 0.55F);
  EXPECT_EQ(tier, GaitTier::Stand);
  EXPECT_EQ(resolve_wolf_clip(state, tier), AnimationStateId::AttackMelee);
}

TEST(WildlifeClipSelection, DeathStartsAtItsOwnPhaseAfterAnInterruptedBite) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17004U;
  (void)gait_speed(state);
  EXPECT_FLOAT_EQ(action_phase(state, 0.20F), 0.20F);
  state.time = 0.1F;
  state.death_progress = 0.0F;
  EXPECT_FLOAT_EQ(action_phase(state, state.death_progress), 0.0F);
}

TEST(WildlifeClipSelection, FlinchIsFullyVisibleBeforeItsPeak) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17005U;
  (void)resolve_clip_transition(state, AnimationStateId::Run, 0.4F);
  state.time = 1.0F;
  auto transition =
      resolve_clip_transition(state, AnimationStateId::WildlifeStartle, 0.0F);
  EXPECT_FLOAT_EQ(transition.weight, 1.0F);
  state.time += 0.075F;
  transition = resolve_clip_transition(state, AnimationStateId::WildlifeStartle, 0.14F);
  EXPECT_FLOAT_EQ(transition.weight, 0.0F);
}

TEST(WildlifeClipSelection, ReplayingAScenarioDoesNotBlendFromItsPreviousDeath) {
  using namespace Render::GL::Wildlife;
  DrawState state;
  state.seed = 0xA17006U;
  state.time = 10.0F;
  (void)resolve_clip_transition(state, AnimationStateId::Dead, 1.0F);
  state.time = 0.0F;
  EXPECT_FLOAT_EQ(resolve_clip_transition(state, AnimationStateId::Run, 0.0F).weight,
                  0.0F);
}

} // namespace

namespace {

using Render::GL::Wildlife::plan_sheep_slapstick;
using Render::GL::Wildlife::sheep_wool_tuft;

TEST(SheepSlapstick, AGrazingSheepIsLeftAlone) {
  DrawState state;
  state.time = 3.0F;
  auto const gag = plan_sheep_slapstick(state);
  EXPECT_EQ(gag.stars, 0.0F);
  EXPECT_EQ(gag.sway_roll, 0.0F);
  EXPECT_LT(gag.poof_time, 0.0F);
}

TEST(SheepSlapstick, ASheepUnderTheMalletSwaysAndSeesStars) {
  DrawState state;
  state.dazed = 0.7F;
  float widest = 0.0F;
  for (int frame = 0; frame < 60; ++frame) {
    state.time = static_cast<float>(frame) / 30.0F;
    auto const gag = plan_sheep_slapstick(state);
    EXPECT_GT(gag.stars, 0.99F);
    widest = std::max(widest, std::abs(gag.sway_roll));
  }
  EXPECT_GT(widest, 5.0F) << "a dazed sheep should wobble visibly";
}

TEST(SheepSlapstick, TheStarsOutstayTheFallThenFade) {
  DrawState state;
  state.death_progress = 1.0F;
  state.dead = true;
  state.death_elapsed = 2.0F;
  auto const lying = plan_sheep_slapstick(state);
  EXPECT_GT(lying.stars, 0.99F);
  EXPECT_LT(lying.star_centre.y(), 0.5F) << "the halo follows the head down";

  state.death_elapsed = Render::GL::Wildlife::k_sheep_star_linger_seconds +
                        Render::GL::Wildlife::k_sheep_star_fade_seconds + 0.1F;
  EXPECT_EQ(plan_sheep_slapstick(state).stars, 0.0F);

  state.death_elapsed = 2.0F;
  state.sink_progress = 0.3F;
  EXPECT_EQ(plan_sheep_slapstick(state).stars, 0.0F) << "nothing over a sinking corpse";
}

TEST(SheepSlapstick, TheWoolBurstsOutAndFloatsAway) {
  DrawState state;
  state.death_progress = 0.1F;
  state.death_elapsed = 0.12F;
  auto const gag = plan_sheep_slapstick(state);
  ASSERT_GE(gag.poof_time, 0.0F);

  float spread = 0.0F;
  for (int i = 0; i < Render::GL::Wildlife::k_sheep_wool_tuft_count; ++i) {
    auto const early = sheep_wool_tuft(17U, i, 0.1F);
    auto const later = sheep_wool_tuft(17U, i, 0.8F);
    EXPECT_GT(later.radius, 0.0F);
    spread = std::max(spread, (later.position - early.position).length());
    EXPECT_EQ(
        sheep_wool_tuft(17U, i, Render::GL::Wildlife::k_sheep_wool_poof_seconds).radius,
        0.0F);
  }
  EXPECT_GT(spread, 0.15F);
}

} // namespace
