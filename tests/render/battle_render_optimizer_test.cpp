#include "render/battle_render_optimizer.h"
#include <gtest/gtest.h>

using namespace Render;

class BattleRenderOptimizerTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto &optimizer = BattleRenderOptimizer::instance();
    BattleRenderConfig config;
    config.temporal_culling_threshold = 15;
    config.animation_throttle_threshold = 30;
    config.animation_throttle_distance = 40.0F;
    config.animation_skip_frames = 2;
    config.enabled = true;
    optimizer.set_config(config);
  }
};

TEST_F(BattleRenderOptimizerTest, DisabledOptimizerAlwaysRendersUnits) {
  auto &optimizer = BattleRenderOptimizer::instance();
  BattleRenderConfig config = optimizer.config();
  config.enabled = false;
  optimizer.set_config(config);
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, false, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, false, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(3, false, false, false));
}

TEST_F(BattleRenderOptimizerTest, SelectedUnitsAlwaysRender) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, false, true, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, false, true, false));
}

TEST_F(BattleRenderOptimizerTest, HoveredUnitsAlwaysRender) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, false, false, true));
  EXPECT_TRUE(optimizer.should_render_unit(2, false, false, true));
}

TEST_F(BattleRenderOptimizerTest, MovingUnitsAlwaysRender) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, true, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, true, false, false));
}

TEST_F(BattleRenderOptimizerTest, BelowThresholdAlwaysRenders) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(10);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, false, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, false, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(3, false, false, false));
}

TEST_F(BattleRenderOptimizerTest, AboveThresholdSkipsIdleUnitsAlternately) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  uint32_t frame = optimizer.frame_counter();
  bool unit1_render = optimizer.should_render_unit(1, false, false, false);
  bool unit2_render = optimizer.should_render_unit(2, false, false, false);

  EXPECT_NE(unit1_render, unit2_render);
}

TEST_F(BattleRenderOptimizerTest, TemporalCullingAlternatesBetweenFrames) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  bool frame1_result = optimizer.should_render_unit(1, false, false, false);

  optimizer.begin_frame();
  bool frame2_result = optimizer.should_render_unit(1, false, false, false);

  EXPECT_NE(frame1_result, frame2_result);
}

TEST_F(BattleRenderOptimizerTest,
       AnimationThrottlingBelowThresholdAlwaysUpdates) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(20);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_update_animation(1, 100.0F * 100.0F, false));
  EXPECT_TRUE(optimizer.should_update_animation(2, 100.0F * 100.0F, false));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingSelectedAlwaysUpdates) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_update_animation(1, 100.0F * 100.0F, true));
  EXPECT_TRUE(optimizer.should_update_animation(2, 100.0F * 100.0F, true));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingCloseUnitsAlwaysUpdate) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_update_animation(1, 10.0F * 10.0F, false));
  EXPECT_TRUE(optimizer.should_update_animation(2, 30.0F * 30.0F, false));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingDistantUnitsThrottled) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  int updated = 0;
  int throttled = 0;
  for (int frame = 0; frame < 6; ++frame) {
    optimizer.begin_frame();
    if (optimizer.should_update_animation(1, 100.0F * 100.0F, false)) {
      ++updated;
    } else {
      ++throttled;
    }
  }

  EXPECT_GT(throttled, 0);
  EXPECT_GT(updated, 0);
}

TEST_F(BattleRenderOptimizerTest, BatchingBoostIncreasesWithUnitCount) {
  auto &optimizer = BattleRenderOptimizer::instance();

  optimizer.set_visible_unit_count(10);
  float boost_low = optimizer.get_batching_boost();

  optimizer.set_visible_unit_count(30);
  float boost_high = optimizer.get_batching_boost();

  EXPECT_FLOAT_EQ(boost_low, 1.0F);
  EXPECT_GT(boost_high, 1.0F);
}

TEST_F(BattleRenderOptimizerTest, IsBattleModeDetectsBattles) {
  auto &optimizer = BattleRenderOptimizer::instance();

  optimizer.set_visible_unit_count(10);
  EXPECT_FALSE(optimizer.is_battle_mode());

  optimizer.set_visible_unit_count(20);
  EXPECT_TRUE(optimizer.is_battle_mode());
}

TEST_F(BattleRenderOptimizerTest, FrameCounterIncrements) {
  auto &optimizer = BattleRenderOptimizer::instance();

  uint32_t frame1 = optimizer.frame_counter();
  optimizer.begin_frame();
  uint32_t frame2 = optimizer.frame_counter();

  EXPECT_EQ(frame2, frame1 + 1);
}

TEST_F(BattleRenderOptimizerTest, StatsResetOnBeginFrame) {
  auto &optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  optimizer.should_render_unit(1, false, false, false);
  optimizer.should_render_unit(2, false, false, false);

  optimizer.begin_frame();
  EXPECT_EQ(optimizer.units_rendered_this_frame(), 0);
  EXPECT_EQ(optimizer.units_skipped_temporal(), 0);
  EXPECT_EQ(optimizer.animations_throttled(), 0);
}
