#include <gtest/gtest.h>

#include "game/wildlife/wildlife_species.h"
#include "render/entity/wildlife/sheep_renderer.h"
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

} // namespace
