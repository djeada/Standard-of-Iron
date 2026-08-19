#include <cmath>
#include <gtest/gtest.h>
#include <tuple>

#include "render/humanoid/soldier_turn_smoothing.h"

namespace {

using Render::Humanoid::resolve_soldier_turn_smoothing;
using Render::Humanoid::SoldierTurnSmoothingInputs;
using Render::Humanoid::SoldierTurnSmoothingState;

auto default_inputs() -> SoldierTurnSmoothingInputs {
  SoldierTurnSmoothingInputs inputs{};
  inputs.dt = 1.0F / 60.0F;
  inputs.max_speed = 2.5F;
  inputs.turn_rate_degrees = 540.0F;
  inputs.snap_distance = 7.0F;
  inputs.settle_distance = 0.06F;
  inputs.allow_travel_yaw = true;
  return inputs;
}

TEST(SoldierTurnSmoothing, AFreshStateSnapsStraightToTheSlot) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.target_x = 3.0F;
  inputs.target_z = -2.0F;
  inputs.formation_yaw_degrees = 90.0F;

  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  EXPECT_FLOAT_EQ(result.x, 3.0F);
  EXPECT_FLOAT_EQ(result.z, -2.0F);
  EXPECT_FLOAT_EQ(result.yaw_degrees, 90.0F);
  EXPECT_FALSE(result.relocating);
  EXPECT_TRUE(state.valid);
}

TEST(SoldierTurnSmoothing, ATeleportBeyondTheSnapDistanceSnaps) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 30.0F;
  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  EXPECT_FLOAT_EQ(result.x, 30.0F);
  EXPECT_FALSE(result.relocating);
}

TEST(SoldierTurnSmoothing, TheStepNeverExceedsTheSpeedCap) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 4.0F;
  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  float const max_step = inputs.max_speed * inputs.dt;
  EXPECT_LE(result.x, max_step + 1e-5F);
  EXPECT_GT(result.x, 0.0F);
  EXPECT_TRUE(result.relocating);
  EXPECT_NEAR(result.travel_speed, inputs.max_speed, 1e-3F);
}

TEST(SoldierTurnSmoothing, ARelocatingSoldierFacesHisDirectionOfTravel) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.formation_yaw_degrees = 0.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 3.0F;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int i = 0; i < 30; ++i) {
    result = resolve_soldier_turn_smoothing(state, inputs);
  }
  EXPECT_TRUE(result.relocating);
  EXPECT_NEAR(result.travel_yaw_degrees, 90.0F, 1e-3F);
  EXPECT_NEAR(result.yaw_degrees, 90.0F, 1.0F);
}

TEST(SoldierTurnSmoothing, ASettledSoldierEasesBackToTheFormationFacing) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.formation_yaw_degrees = 180.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);
  state.body_yaw_degrees = 90.0F;

  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int i = 0; i < 60; ++i) {
    result = resolve_soldier_turn_smoothing(state, inputs);
  }
  EXPECT_FALSE(result.relocating);
  EXPECT_NEAR(std::abs(result.yaw_degrees), 180.0F, 1.0F);
}

TEST(SoldierTurnSmoothing, CombatKeepsTheFormationFacingWhileStepping) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.formation_yaw_degrees = 0.0F;
  inputs.allow_travel_yaw = false;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 3.0F;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int i = 0; i < 30; ++i) {
    result = resolve_soldier_turn_smoothing(state, inputs);
  }
  EXPECT_TRUE(result.relocating);
  EXPECT_NEAR(result.yaw_degrees, 0.0F, 1e-3F);
}

TEST(SoldierTurnSmoothing, ZeroDtLeavesTheStateUntouched) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 3.0F;
  inputs.dt = 0.0F;
  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  EXPECT_FLOAT_EQ(result.x, 0.0F);
  EXPECT_FALSE(result.relocating);
}

TEST(SoldierTurnSmoothing, SlotMicroWobbleNeverFlapsTheWalkState) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  for (int i = 0; i < 120; ++i) {
    inputs.target_x = ((i % 2) == 0) ? 0.2F : -0.2F;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    EXPECT_FALSE(result.relocating) << "frame " << i;
  }
}

TEST(SoldierTurnSmoothing, TheCatchUpSpeedOutrunsTheUnitItFollows) {

  constexpr float k_unit_speed = 2.0F;
  constexpr float k_run_speed = k_unit_speed * 1.5F;

  EXPECT_GT(Render::Humanoid::soldier_catch_up_speed(k_run_speed), k_run_speed);
  EXPECT_FLOAT_EQ(Render::Humanoid::soldier_catch_up_speed(0.5F), 1.6F);
}

TEST(SoldierTurnSmoothing, ARunningFormationDoesNotDriftAwayFromItsSlots) {

  constexpr float k_run_speed = 3.0F;
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.max_speed = Render::Humanoid::soldier_catch_up_speed(k_run_speed);
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float slot_x = 0.0F;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int frame = 0; frame < 600; ++frame) {
    slot_x += k_run_speed * inputs.dt;
    inputs.target_x = slot_x;
    result = resolve_soldier_turn_smoothing(state, inputs);
  }

  EXPECT_LT(std::abs(slot_x - result.x), 0.1F);
}

TEST(SoldierTurnSmoothing, ACappedFormationWouldFallBehindWhileRunning) {

  constexpr float k_run_speed = 3.0F;
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.max_speed = 2.5F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float slot_x = 0.0F;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int frame = 0; frame < 600; ++frame) {
    slot_x += k_run_speed * inputs.dt;
    inputs.target_x = slot_x;
    result = resolve_soldier_turn_smoothing(state, inputs);
  }

  EXPECT_GT(slot_x - result.x, 1.0F);
}

TEST(SoldierTurnSmoothing, EveryResolveStampsTheFrameThatMovedTheSoldier) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.frame_index = 7U;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);
  EXPECT_EQ(state.updated_frame, 7U);

  inputs.frame_index = 8U;
  inputs.target_x = 0.5F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);
  EXPECT_EQ(state.updated_frame, 8U);

  inputs.frame_index = 9U;
  inputs.dt = 0.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);
  EXPECT_EQ(state.updated_frame, 9U);
}

TEST(SoldierTurnSmoothing, AHalfTurnWalksThroughInsteadOfSweepingTheArc) {

  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.target_x = 3.0F;
  inputs.target_z = 0.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = -3.0F;
  inputs.formation_yaw_degrees = 180.0F;
  float travelled = 0.0F;
  float last_x = state.world_x;
  float last_z = state.world_z;
  int frames = 0;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  do {
    result = resolve_soldier_turn_smoothing(state, inputs);
    float const dx = result.x - last_x;
    float const dz = result.z - last_z;
    travelled += std::sqrt(dx * dx + dz * dz);
    last_x = result.x;
    last_z = result.z;
    ++frames;
  } while (result.relocating && frames < 600);

  EXPECT_LT(frames, 600);
  EXPECT_NEAR(travelled, 6.0F, 0.2F);
  EXPECT_NEAR(result.x, -3.0F, 0.1F);
}

} // namespace
