#include <gtest/gtest.h>

#include "render/battle_render_optimizer.h"

using namespace Render;

class BattleRenderOptimizerTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& optimizer = BattleRenderOptimizer::instance();
    BattleRenderConfig config;
    config.temporal_culling_threshold = 15;
    config.animation_throttle_threshold = 30;
    config.animation_throttle_distance = 40.0F;
    config.combat_render_priority_distance = 50.0F;
    config.combat_animation_priority_distance = 36.0F;
    config.animation_skip_frames = 2;
    config.enabled = true;
    optimizer.set_config(config);
  }

  static auto moving_motion() -> Engine::Core::MotionPresentationComponent {
    Engine::Core::MotionPresentationComponent motion;
    motion.snapshot_valid = true;
    motion.set_state(Engine::Core::MotionPresentationState::Walk);
    return motion;
  }
};

TEST_F(BattleRenderOptimizerTest, DisabledOptimizerAlwaysRendersUnits) {
  auto& optimizer = BattleRenderOptimizer::instance();
  BattleRenderConfig config = optimizer.config();
  config.enabled = false;
  optimizer.set_config(config);
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, nullptr, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(3, nullptr, false, false));
}

TEST_F(BattleRenderOptimizerTest, SelectedUnitsAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, true, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, nullptr, true, false));
}

TEST_F(BattleRenderOptimizerTest, HoveredUnitsAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, false, true));
  EXPECT_TRUE(optimizer.should_render_unit(2, nullptr, false, true));
}

TEST_F(BattleRenderOptimizerTest, MovingUnitsWithStaleSnapshotAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  Engine::Core::MotionPresentationComponent motion;
  motion.snapshot_valid = false;
  motion.set_state(Engine::Core::MotionPresentationState::Walk);

  uint32_t const frame = optimizer.frame_counter();
  uint32_t const id_would_skip = (frame % 2 == 0) ? 1 : 2;

  EXPECT_TRUE(optimizer.should_render_unit(id_would_skip, &motion, false, false));
}

TEST_F(BattleRenderOptimizerTest,
       AnimationThrottlingMovingUnitsAlwaysUpdatesWithStaleSnapshot) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  uint32_t const frame = optimizer.frame_counter();

  uint32_t throttled_id = 0;
  for (uint32_t id = 1; id <= 3; ++id) {
    if ((id + frame) % 3 != 0) {
      throttled_id = id;
      break;
    }
  }
  ASSERT_NE(throttled_id, 0U);

  Engine::Core::MotionPresentationComponent motion;
  motion.snapshot_valid = false;
  motion.set_state(Engine::Core::MotionPresentationState::Walk);

  float const far_distance_sq = 100.0F * 100.0F;
  EXPECT_TRUE(optimizer.should_update_animation(
      throttled_id, far_distance_sq, false, false, &motion));
}

TEST_F(BattleRenderOptimizerTest, MovingUnitsAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();
  auto motion = moving_motion();

  EXPECT_TRUE(optimizer.should_render_unit(1, &motion, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, &motion, false, false));
}

TEST_F(BattleRenderOptimizerTest, BelowThresholdAlwaysRenders) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(10);
  optimizer.begin_frame();

  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(2, nullptr, false, false));
  EXPECT_TRUE(optimizer.should_render_unit(3, nullptr, false, false));
}

TEST_F(BattleRenderOptimizerTest, AboveThresholdStillRendersIdleUnitsEveryFrame) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  bool const unit1_render = optimizer.should_render_unit(1, nullptr, false, false);
  bool const unit2_render = optimizer.should_render_unit(2, nullptr, false, false);

  EXPECT_TRUE(unit1_render);
  EXPECT_TRUE(unit2_render);
  EXPECT_EQ(optimizer.units_skipped_temporal(), 0);
}

TEST_F(BattleRenderOptimizerTest, TemporalCullingDoesNotDropDrawSubmission) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  bool const frame1_result = optimizer.should_render_unit(1, nullptr, false, false);

  optimizer.begin_frame();
  bool const frame2_result = optimizer.should_render_unit(1, nullptr, false, false);

  EXPECT_TRUE(frame1_result);
  EXPECT_TRUE(frame2_result);
  EXPECT_EQ(optimizer.units_skipped_temporal(), 0);
}

TEST_F(BattleRenderOptimizerTest, ActiveCombatUnitsAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, false, false, true));

  optimizer.begin_frame();
  EXPECT_TRUE(optimizer.should_render_unit(1, nullptr, false, false, true));
}

TEST_F(BattleRenderOptimizerTest, DistantCombatUnitsAlwaysRender) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  float const far_distance_sq = 100.0F * 100.0F;

  optimizer.begin_frame();
  bool const frame1 =
      optimizer.should_render_unit(1, nullptr, false, false, true, far_distance_sq);

  optimizer.begin_frame();
  bool const frame2 =
      optimizer.should_render_unit(1, nullptr, false, false, true, far_distance_sq);

  EXPECT_TRUE(frame1);
  EXPECT_TRUE(frame2);
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingBelowThresholdAlwaysUpdates) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(20);
  optimizer.begin_frame();

  EXPECT_TRUE(
      optimizer.should_update_animation(1, 100.0F * 100.0F, false, false, nullptr));
  EXPECT_TRUE(
      optimizer.should_update_animation(2, 100.0F * 100.0F, false, false, nullptr));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingSelectedAlwaysUpdates) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(
      optimizer.should_update_animation(1, 100.0F * 100.0F, true, false, nullptr));
  EXPECT_TRUE(
      optimizer.should_update_animation(2, 100.0F * 100.0F, true, false, nullptr));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingMovingUnitsAlwaysUpdates) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  auto motion = moving_motion();

  for (int frame = 0; frame < 6; ++frame) {
    optimizer.begin_frame();
    EXPECT_TRUE(
        optimizer.should_update_animation(1, 100.0F * 100.0F, false, false, &motion));
  }
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingCloseUnitsAlwaysUpdate) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);
  optimizer.begin_frame();

  EXPECT_TRUE(
      optimizer.should_update_animation(1, 10.0F * 10.0F, false, false, nullptr));
  EXPECT_TRUE(
      optimizer.should_update_animation(2, 30.0F * 30.0F, false, false, nullptr));
}

TEST_F(BattleRenderOptimizerTest, AnimationThrottlingDistantUnitsThrottled) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  int updated = 0;
  int throttled = 0;
  for (int frame = 0; frame < 6; ++frame) {
    optimizer.begin_frame();
    if (optimizer.should_update_animation(1, 100.0F * 100.0F, false, false, nullptr)) {
      ++updated;
    } else {
      ++throttled;
    }
  }

  EXPECT_GT(throttled, 0);
  EXPECT_GT(updated, 0);
}

TEST_F(BattleRenderOptimizerTest, NearbyCombatAnimationsAreNeverThrottled) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  for (int frame = 0; frame < 6; ++frame) {
    optimizer.begin_frame();
    EXPECT_TRUE(
        optimizer.should_update_animation(1, 10.0F * 10.0F, false, true, nullptr));
  }
}

TEST_F(BattleRenderOptimizerTest, DistantCombatAnimationsAlwaysUpdate) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  for (int frame = 0; frame < 6; ++frame) {
    optimizer.begin_frame();
    EXPECT_TRUE(
        optimizer.should_update_animation(1, 100.0F * 100.0F, false, true, nullptr));
  }
}

TEST_F(BattleRenderOptimizerTest, BatchingBoostIncreasesWithUnitCount) {
  auto& optimizer = BattleRenderOptimizer::instance();

  optimizer.set_visible_unit_count(10);
  float const boost_low = optimizer.get_batching_boost();

  optimizer.set_visible_unit_count(30);
  float const boost_high = optimizer.get_batching_boost();

  EXPECT_FLOAT_EQ(boost_low, 1.0F);
  EXPECT_GT(boost_high, 1.0F);
}

TEST_F(BattleRenderOptimizerTest, IsBattleModeDetectsBattles) {
  auto& optimizer = BattleRenderOptimizer::instance();

  optimizer.set_visible_unit_count(10);
  EXPECT_FALSE(optimizer.is_battle_mode());

  optimizer.set_visible_unit_count(20);
  EXPECT_TRUE(optimizer.is_battle_mode());
}

TEST_F(BattleRenderOptimizerTest, FrameCounterIncrements) {
  auto& optimizer = BattleRenderOptimizer::instance();

  uint32_t const frame1 = optimizer.frame_counter();
  optimizer.begin_frame();
  uint32_t const frame2 = optimizer.frame_counter();

  EXPECT_EQ(frame2, frame1 + 1);
}

TEST_F(BattleRenderOptimizerTest, StatsResetOnBeginFrame) {
  auto& optimizer = BattleRenderOptimizer::instance();
  optimizer.set_visible_unit_count(100);

  optimizer.begin_frame();
  (void)optimizer.should_render_unit(1, nullptr, false, false);
  (void)optimizer.should_render_unit(2, nullptr, false, false);

  optimizer.begin_frame();
  EXPECT_EQ(optimizer.units_rendered_this_frame(), 0);
  EXPECT_EQ(optimizer.units_skipped_temporal(), 0);
  EXPECT_EQ(optimizer.animations_throttled(), 0);
}
