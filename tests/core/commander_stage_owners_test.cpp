#include <Qt>

#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_body_facing.h"
#include "app/commander/commander_input_port.h"
#include "app/commander/commander_look.h"

namespace {

using App::Core::BodyFacingInput;
using App::Core::CommanderBodyFacing;
using App::Core::CommanderInputPort;
using App::Core::CommanderLook;
using App::Core::EdgeOutcome;

TEST(CommanderInputPortTest, AnUnconsumedPressIsCarriedWhileHeldThenDropped) {
  CommanderInputPort port;
  port.primary_action_down();
  port.capture_tick();
  EXPECT_TRUE(port.tick().primary_pressed);
  EXPECT_TRUE(port.tick().primary_held);

  port.settle_primary_press();
  port.capture_tick();
  EXPECT_TRUE(port.tick().primary_pressed)
      << "an unconsumed press is carried to the next tick while held";

  port.settle_primary_press();
  port.primary_action_up();
  port.capture_tick();
  ASSERT_TRUE(port.tick().primary_pressed);
  ASSERT_FALSE(port.tick().primary_held);

  port.settle_primary_press();
  EXPECT_EQ(port.edges().primary_dropped_sequence, 1U)
      << "the press dies with the button, and is counted";
  port.capture_tick();
  EXPECT_FALSE(port.tick().primary_pressed);
}

TEST(CommanderInputPortTest, AReleasedPressIsCountedAsDropped) {
  CommanderInputPort port;
  port.primary_action_down();
  port.primary_action_up();
  port.capture_tick();
  ASSERT_TRUE(port.tick().primary_pressed);
  ASSERT_FALSE(port.tick().primary_held);

  port.settle_primary_press();
  EXPECT_FALSE(port.tick().primary_pressed);
  EXPECT_EQ(port.edges().primary_dropped_sequence, 1U);
  EXPECT_EQ(port.edges().primary_consumed_sequence, 0U);
}

TEST(CommanderInputPortTest, ConsumingAPressCountsItOnceAndClearsTheEdges) {
  CommanderInputPort port;
  port.primary_action_down();
  port.request_heavy_action();
  port.capture_tick();

  port.consume_press_edges(port.tick().primary_pressed);
  EXPECT_FALSE(port.tick().primary_pressed);
  EXPECT_FALSE(port.tick().heavy_pressed);
  EXPECT_EQ(port.edges().primary_consumed_sequence, 1U);
}

TEST(CommanderInputPortTest, ReleaseAllDiscardsEveryPendingEdge) {
  CommanderInputPort port;
  port.key_down(Qt::Key_W);
  port.primary_action_down();
  port.request_jump();
  port.request_dodge();
  port.secondary_action_down();

  port.release_all();

  EXPECT_FALSE(port.held().forward);
  EXPECT_FALSE(port.held().primary_action);
  EXPECT_FALSE(port.held().secondary_action);
  EXPECT_EQ(port.edges().primary_dropped_sequence, 1U);
  EXPECT_EQ(port.edges().jump_refused_sequence, 1U);
  EXPECT_EQ(port.edges().dodge_refused_sequence, 1U);
  EXPECT_EQ(port.edges().primary_release_sequence, 1U);
  EXPECT_EQ(port.edges().guard_release_sequence, 1U);

  port.capture_tick();
  EXPECT_FALSE(port.tick().has_pending_edge());
  EXPECT_FALSE(port.tick().forward);
}

TEST(CommanderInputPortTest, EdgeOutcomesFeedTheSequenceCounters) {
  CommanderInputPort port;
  port.record_jump(EdgeOutcome::Consumed);
  port.record_jump(EdgeOutcome::Refused);
  port.record_jump(EdgeOutcome::None);
  port.record_dodge(EdgeOutcome::Refused);
  EXPECT_EQ(port.edges().jump_consumed_sequence, 1U);
  EXPECT_EQ(port.edges().jump_refused_sequence, 1U);
  EXPECT_EQ(port.edges().dodge_consumed_sequence, 0U);
  EXPECT_EQ(port.edges().dodge_refused_sequence, 1U);
}

TEST(CommanderBodyFacingTest, TheBodySnapsToTheViewOnFirstUseAndWhenItMustFaceIt) {
  CommanderBodyFacing facing;
  BodyFacingInput input;
  input.view_yaw = 120.0F;
  input.dt = 0.016F;
  EXPECT_FLOAT_EQ(facing.advance(input), 120.0F);

  input.view_yaw = 200.0F;
  input.must_face_view = true;
  EXPECT_FLOAT_EQ(facing.advance(input), 200.0F);
}

TEST(CommanderBodyFacingTest, SmallViewOffsetsDoNotTurnAStandingBody) {
  CommanderBodyFacing facing;
  BodyFacingInput input;
  input.view_yaw = 10.0F;
  input.dt = 0.016F;
  static_cast<void>(facing.advance(input));

  input.view_yaw = 40.0F;
  EXPECT_FLOAT_EQ(facing.advance(input), 10.0F)
      << "under the turn-in-place threshold the body stays put";
}

TEST(CommanderBodyFacingTest, ALargeOffsetTurnsInPlaceAtALimitedRate) {
  CommanderBodyFacing facing;
  BodyFacingInput input;
  input.view_yaw = 0.0F;
  input.dt = 0.05F;
  static_cast<void>(facing.advance(input));

  input.view_yaw = 90.0F;
  float const first = facing.advance(input);
  EXPECT_GT(first, 0.0F);
  EXPECT_LT(first, 90.0F);
  EXPECT_NEAR(first, 260.0F * 0.05F, 1.0e-3F);
}

TEST(CommanderBodyFacingTest, TravelKeepsTheBodyOnTheViewFast) {
  CommanderBodyFacing facing;
  BodyFacingInput input;
  input.view_yaw = 0.0F;
  input.dt = 0.05F;
  static_cast<void>(facing.advance(input));

  input.view_yaw = 30.0F;
  input.follows_travel = true;
  EXPECT_FLOAT_EQ(facing.advance(input), 30.0F);
}

TEST(CommanderLookTest, MouseDeltasWrapYawAndClampPitch) {
  CommanderLook look;
  look.set_yaw(359.0F);
  look.apply_mouse_delta(1000.0, 100000.0, 1.0F);
  EXPECT_GE(look.yaw(), 0.0F);
  EXPECT_LT(look.yaw(), 360.0F);
  EXPECT_LE(std::abs(look.pitch()), App::Core::k_commander_view_pitch_limit_degrees);

  look.set_pitch(500.0F);
  EXPECT_FLOAT_EQ(look.pitch(), App::Core::k_commander_view_pitch_limit_degrees);
}

TEST(CommanderLookTest, FrameIntentReportsLookDeltaSinceTheLastSample) {
  CommanderLook look;
  App::Core::CommanderHeldInput held;
  held.forward = true;
  held.right = true;

  look.set_yaw(10.0F);
  auto first = look.sample_frame_intent(held);
  EXPECT_FLOAT_EQ(first.look_delta.x(), 0.0F);
  EXPECT_FLOAT_EQ(first.move.x(), 1.0F);
  EXPECT_FLOAT_EQ(first.move.y(), 1.0F);

  look.set_yaw(25.0F);
  auto second = look.sample_frame_intent(held);
  EXPECT_FLOAT_EQ(second.look_delta.x(), 15.0F);
  EXPECT_EQ(second.frame_index, first.frame_index + 1);
}

} // namespace
