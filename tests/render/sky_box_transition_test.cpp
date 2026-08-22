#include <gtest/gtest.h>

#include "render/gl/backend/sky_box_transition.h"

namespace {

using Render::GL::BackendPipelines::SkyBoxTransition;

constexpr float k_frame = 1.0F / 60.0F;

auto settle(SkyBoxTransition& transition) -> int {
  int frames = 0;
  while (!transition.is_settled() && frames < 1000) {
    transition.advance(k_frame);
    ++frames;
  }
  return frames;
}

} // namespace

TEST(SkyBoxTransition, StartsHiddenSoTheRtsFrameSkipsThePass) {
  SkyBoxTransition transition;
  EXPECT_FALSE(transition.is_visible());
  EXPECT_FLOAT_EQ(transition.blend(), 0.0F);
  EXPECT_TRUE(transition.is_settled());
}

TEST(SkyBoxTransition, RpgModeFadesInWithinTheTransitionWindow) {
  SkyBoxTransition transition;
  transition.set_target(true);

  transition.advance(k_frame);
  EXPECT_TRUE(transition.is_visible());
  EXPECT_GT(transition.blend(), 0.0F);
  EXPECT_LT(transition.blend(), 1.0F);

  const int frames = settle(transition);
  EXPECT_FLOAT_EQ(transition.blend(), 1.0F);
  EXPECT_LE(static_cast<float>(frames) * k_frame,
            SkyBoxTransition::k_transition_seconds + k_frame);
}

TEST(SkyBoxTransition, BlendRisesMonotonicallyAndEasesAtBothEnds) {
  SkyBoxTransition transition;
  transition.set_target(true);

  float previous = transition.blend();
  bool sampled_midpoint = false;
  float linear_at_midpoint = 0.0F;
  float eased_at_midpoint = 0.0F;
  for (int frame = 0; frame < 60; ++frame) {
    const float blend = transition.advance(k_frame);
    EXPECT_GE(blend, previous);
    if (!sampled_midpoint && transition.progress() >= 0.5F) {
      sampled_midpoint = true;
      linear_at_midpoint = transition.progress();
      eased_at_midpoint = blend;
    }
    previous = blend;
  }

  ASSERT_TRUE(sampled_midpoint);
  EXPECT_NEAR(eased_at_midpoint, linear_at_midpoint, 0.06F);
  EXPECT_FLOAT_EQ(transition.blend(), 1.0F);
}

TEST(SkyBoxTransition, LeavingRpgFadesBackOutAndStopsDrawing) {
  SkyBoxTransition transition;
  transition.set_target(true);
  settle(transition);

  transition.set_target(false);
  transition.advance(k_frame);
  EXPECT_TRUE(transition.is_visible());
  EXPECT_LT(transition.blend(), 1.0F);

  settle(transition);
  EXPECT_FALSE(transition.is_visible());
  EXPECT_FLOAT_EQ(transition.blend(), 0.0F);
}

TEST(SkyBoxTransition, ASingleStalledFrameCannotSnapTheSkyOver) {
  SkyBoxTransition transition;
  transition.set_target(true);

  const float blend = transition.advance(30.0F);
  EXPECT_LT(blend, 1.0F);
  EXPECT_NEAR(transition.progress(),
              SkyBoxTransition::k_max_step_seconds /
                  SkyBoxTransition::k_transition_seconds,
              1e-5F);
}

TEST(SkyBoxTransition, NegativeOrZeroDeltaHoldsTheCurrentBlend) {
  SkyBoxTransition transition;
  transition.set_target(true);
  transition.advance(k_frame);
  const float held = transition.blend();

  EXPECT_FLOAT_EQ(transition.advance(0.0F), held);
  EXPECT_FLOAT_EQ(transition.advance(-1.0F), held);
}
