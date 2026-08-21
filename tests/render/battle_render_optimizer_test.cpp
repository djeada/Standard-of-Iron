#include <gtest/gtest.h>

#include "render/battle_render_optimizer.h"

using namespace Render;

namespace {

class BattleRenderOptimizerTest : public ::testing::Test {
protected:
  void SetUp() override {
    BattleRenderConfig config;
    config.battle_mode_unit_threshold = 15;
    config.animation_throttle_threshold = 30;
    config.animation_throttle_distance = 40.0F;
    config.combat_render_priority_distance = 50.0F;
    config.combat_animation_priority_distance = 36.0F;
    config.animation_skip_frames = 2;
    config.enabled = true;
    optimizer.set_config(config);
  }

  BattleRenderOptimizer optimizer;
  BattleRenderOptimizer::FrameStats stats;

  auto open_frame(int visible_units) -> const BattleRenderOptimizer::FrameSnapshot& {
    optimizer.begin_frame();
    optimizer.set_visible_unit_count(visible_units);
    return optimizer.frame();
  }

  static auto moving_motion() -> Engine::Core::MotionPresentationComponent {
    Engine::Core::MotionPresentationComponent motion;
    motion.snapshot_valid = true;
    motion.set_state(Engine::Core::MotionPresentationState::Walk);
    return motion;
  }
};

TEST_F(BattleRenderOptimizerTest, EachOptimizerCountsItsOwnFrames) {
  BattleRenderOptimizer other;

  optimizer.begin_frame();
  optimizer.begin_frame();
  other.begin_frame();

  EXPECT_EQ(optimizer.frame_counter(), 2U);
  EXPECT_EQ(other.frame_counter(), 1U);
}

TEST_F(BattleRenderOptimizerTest, FrameCounterIncrements) {
  const std::uint32_t before = optimizer.frame_counter();
  optimizer.begin_frame();
  EXPECT_EQ(optimizer.frame_counter(), before + 1U);
}

TEST_F(BattleRenderOptimizerTest, TheFrameSnapshotIsTakenOnceAndDoesNotShift) {
  const auto& frame = open_frame(100);
  ASSERT_TRUE(frame.config.enabled);

  BattleRenderConfig disabled = optimizer.config();
  disabled.enabled = false;
  optimizer.set_config(disabled);

  EXPECT_TRUE(optimizer.frame().config.enabled);
  optimizer.begin_frame();
  EXPECT_FALSE(optimizer.frame().config.enabled);
}

TEST_F(BattleRenderOptimizerTest, TheVisibleCountSurvivesTheFrameBoundary) {
  open_frame(120);
  optimizer.begin_frame();

  EXPECT_EQ(optimizer.visible_unit_count(), 120);
}

TEST_F(BattleRenderOptimizerTest, DisabledOptimizerAlwaysUpdatesAnimations) {
  BattleRenderConfig config = optimizer.config();
  config.enabled = false;
  optimizer.set_config(config);
  const auto& frame = open_frame(100);

  for (std::uint32_t id = 1; id <= 6; ++id) {
    EXPECT_TRUE(frame.should_update_animation(
        id, 100.0F * 100.0F, false, false, nullptr, stats));
  }
}

TEST_F(BattleRenderOptimizerTest, SelectedUnitsAlwaysUpdate) {
  const auto& frame = open_frame(100);
  EXPECT_TRUE(
      frame.should_update_animation(1, 100.0F * 100.0F, true, false, nullptr, stats));
  EXPECT_TRUE(
      frame.should_update_animation(2, 100.0F * 100.0F, true, false, nullptr, stats));
}

TEST_F(BattleRenderOptimizerTest, MovingUnitsAlwaysUpdate) {
  auto motion = moving_motion();
  for (int i = 0; i < 6; ++i) {
    const auto& frame = open_frame(100);
    EXPECT_TRUE(frame.should_update_animation(
        1, 100.0F * 100.0F, false, false, &motion, stats));
  }
}

TEST_F(BattleRenderOptimizerTest, BelowTheThresholdNothingIsThrottled) {
  const auto& frame = open_frame(20);
  EXPECT_TRUE(
      frame.should_update_animation(1, 100.0F * 100.0F, false, false, nullptr, stats));
  EXPECT_TRUE(
      frame.should_update_animation(2, 100.0F * 100.0F, false, false, nullptr, stats));
  EXPECT_EQ(stats.animations_throttled, 0);
}

TEST_F(BattleRenderOptimizerTest, CloseUnitsAlwaysUpdate) {
  const auto& frame = open_frame(100);
  EXPECT_TRUE(
      frame.should_update_animation(1, 10.0F * 10.0F, false, false, nullptr, stats));
  EXPECT_TRUE(
      frame.should_update_animation(2, 30.0F * 30.0F, false, false, nullptr, stats));
}

TEST_F(BattleRenderOptimizerTest, DistantIdleUnitsTakeTurns) {
  int updated = 0;
  int throttled = 0;
  for (int i = 0; i < 6; ++i) {
    const auto& frame = open_frame(100);
    if (frame.should_update_animation(
            1, 100.0F * 100.0F, false, false, nullptr, stats)) {
      ++updated;
    } else {
      ++throttled;
    }
  }

  EXPECT_GT(throttled, 0);
  EXPECT_GT(updated, 0);
  EXPECT_EQ(updated + throttled, 6);
}

TEST_F(BattleRenderOptimizerTest, UnitsInCombatAreNeverThrottled) {
  for (int i = 0; i < 6; ++i) {
    const auto& frame = open_frame(100);
    EXPECT_TRUE(
        frame.should_update_animation(1, 10.0F * 10.0F, false, true, nullptr, stats));
    EXPECT_TRUE(
        frame.should_update_animation(2, 100.0F * 100.0F, false, true, nullptr, stats));
  }
  EXPECT_EQ(stats.animations_throttled, 0);
}

TEST_F(BattleRenderOptimizerTest, StatsAreCallerLocalUntilCommitted) {
  const auto& frame = open_frame(100);
  for (std::uint32_t id = 1; id <= 12; ++id) {
    (void)frame.should_update_animation(
        id, 100.0F * 100.0F, false, false, nullptr, stats);
  }

  EXPECT_GT(stats.animations_updated + stats.animations_throttled, 0);
  EXPECT_EQ(optimizer.animations_updated(), 0);
  EXPECT_EQ(optimizer.animations_throttled(), 0);

  optimizer.commit_frame_stats(stats);
  EXPECT_EQ(optimizer.animations_updated(), stats.animations_updated);
  EXPECT_EQ(optimizer.animations_throttled(), stats.animations_throttled);
}

TEST_F(BattleRenderOptimizerTest, CommittedStatsResetOnTheNextFrame) {
  const auto& frame = open_frame(100);
  (void)frame.should_update_animation(1, 100.0F * 100.0F, false, false, nullptr, stats);
  optimizer.commit_frame_stats(stats);

  optimizer.begin_frame();
  EXPECT_EQ(optimizer.animations_updated(), 0);
  EXPECT_EQ(optimizer.animations_throttled(), 0);
}

TEST_F(BattleRenderOptimizerTest, BatchingBoostRisesWithTheUnitCount) {
  const float low = open_frame(10).batching_boost();
  const float high = open_frame(30).batching_boost();

  EXPECT_FLOAT_EQ(low, 1.0F);
  EXPECT_GT(high, 1.0F);
}

TEST_F(BattleRenderOptimizerTest, BattleModeIsABatchingThresholdNotACullingOne) {
  EXPECT_FALSE(open_frame(10).battle_mode());
  EXPECT_TRUE(open_frame(20).battle_mode());
}

} // namespace
