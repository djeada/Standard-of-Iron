#include <cmath>
#include <gtest/gtest.h>
#include <tuple>

#include "render/humanoid/runtime/soldier_turn_smoothing.h"

namespace {

using Render::Humanoid::resolve_soldier_turn_smoothing;
using Render::Humanoid::soldier_turn_variation;
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

TEST(SoldierTurnSmoothing, PublishedSimulationAnchorOwnsPosition) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 3.25F;
  inputs.target_z = -1.75F;
  inputs.max_speed = 0.01F;
  inputs.position_is_authoritative = true;
  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  EXPECT_FLOAT_EQ(result.x, inputs.target_x);
  EXPECT_FLOAT_EQ(result.z, inputs.target_z);
  EXPECT_FALSE(result.relocating);
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

TEST(SoldierTurnSmoothing, IndividualResponseDelayStaggersDirectionChanges) {
  SoldierTurnSmoothingState immediate{};
  SoldierTurnSmoothingState delayed{};
  auto immediate_inputs = default_inputs();
  auto delayed_inputs = default_inputs();
  std::ignore = resolve_soldier_turn_smoothing(immediate, immediate_inputs);
  std::ignore = resolve_soldier_turn_smoothing(delayed, delayed_inputs);

  immediate_inputs.formation_yaw_degrees = 90.0F;
  delayed_inputs.formation_yaw_degrees = 90.0F;
  delayed_inputs.response_delay_seconds = 0.12F;
  auto const immediate_turn =
      resolve_soldier_turn_smoothing(immediate, immediate_inputs);
  auto delayed_turn = resolve_soldier_turn_smoothing(delayed, delayed_inputs);

  EXPECT_GT(immediate_turn.yaw_degrees, 0.0F);
  EXPECT_FLOAT_EQ(delayed_turn.yaw_degrees, 0.0F);
  for (int frame = 0; frame < 12; ++frame) {
    delayed_turn = resolve_soldier_turn_smoothing(delayed, delayed_inputs);
  }
  EXPECT_GT(delayed_turn.yaw_degrees, 0.0F);
  for (int frame = 0; frame < 120; ++frame) {
    delayed_turn = resolve_soldier_turn_smoothing(delayed, delayed_inputs);
  }
  EXPECT_NEAR(delayed_turn.yaw_degrees, 90.0F, 1.0F);
}

TEST(SoldierTurnSmoothing, RearRanksReactLaterWithBoundedIndividualVariation) {
  constexpr std::uint32_t k_seed = 0x12345678U;
  auto const front = soldier_turn_variation(k_seed, 3, 4, false);
  auto const rear = soldier_turn_variation(k_seed, 0, 4, false);
  auto const mounted = soldier_turn_variation(k_seed, 3, 4, true);

  EXPECT_LT(front.response_delay_seconds, rear.response_delay_seconds);
  EXPECT_GT(mounted.response_delay_seconds, front.response_delay_seconds);
  EXPECT_GE(front.catch_up_speed_scale, 0.90F);
  EXPECT_LE(front.catch_up_speed_scale, 1.08F);
  EXPECT_GE(front.turn_rate_scale, 0.78F);
  EXPECT_LE(front.turn_rate_scale, 1.18F);
  EXPECT_EQ(front.response_delay_seconds,
            soldier_turn_variation(k_seed, 3, 4, false).response_delay_seconds);
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

TEST(SoldierTurnSmoothing, AHalfTurnWheelsAroundInsteadOfCrossingTheRanks) {

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
    EXPECT_GT(std::hypot(result.x, result.z), 2.5F);
    ++frames;
  } while (result.relocating && frames < 600);

  EXPECT_LT(frames, 600);
  EXPECT_GT(travelled, 8.0F);
  EXPECT_LT(travelled, 11.0F);
  EXPECT_NEAR(result.x, -3.0F, 0.1F);
  EXPECT_NEAR(result.z, 0.0F, 0.1F);
}

TEST(SoldierTurnSmoothing, OppositeWingsKeepOneFormationFacingDuringAHalfTurn) {
  auto inputs = default_inputs();
  SoldierTurnSmoothingState right_wing{};
  SoldierTurnSmoothingState left_wing{};

  inputs.target_x = 3.0F;
  std::ignore = resolve_soldier_turn_smoothing(right_wing, inputs);
  inputs.target_x = -3.0F;
  std::ignore = resolve_soldier_turn_smoothing(left_wing, inputs);

  inputs.formation_yaw_degrees = 180.0F;
  inputs.target_x = -3.0F;
  auto const right_result = resolve_soldier_turn_smoothing(right_wing, inputs);
  inputs.target_x = 3.0F;
  auto const left_result = resolve_soldier_turn_smoothing(left_wing, inputs);

  EXPECT_GT(right_result.z, 0.0F);
  EXPECT_LT(left_result.z, 0.0F);
  EXPECT_NEAR(right_result.yaw_degrees, left_result.yaw_degrees, 1.0e-3F);
  EXPECT_LT(right_result.yaw_degrees, 0.0F);
}

TEST(SoldierTurnSmoothing, WheelDirectionStaysStableAfterTheHeadingChange) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.target_x = 3.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = -3.0F;
  inputs.formation_yaw_degrees = 179.0F;
  auto first = resolve_soldier_turn_smoothing(state, inputs);
  EXPECT_LT(first.z, 0.0F);

  inputs.target_z = 0.001F;
  for (int frame = 0; frame < 30; ++frame) {
    auto const next = resolve_soldier_turn_smoothing(state, inputs);
    EXPECT_LE(next.z, first.z);
    first = next;
  }
}

TEST(SoldierTurnSmoothing, ASmallTurnStillTakesTheDirectCorrection) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.target_x = 3.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  inputs.target_x = 2.30F;
  inputs.target_z = 1.93F;
  auto const result = resolve_soldier_turn_smoothing(state, inputs);

  EXPECT_LT(result.x, 3.0F);
  EXPECT_GT(result.z, 0.0F);
  EXPECT_NEAR(result.travel_yaw_degrees, -20.0F, 1.0F);
}

struct PivotFrame {
  float yaw_degrees{0.0F};
  float target_x{0.0F};
  float target_z{0.0F};
};

auto pivot_slot(float local_x, float local_z, float yaw_degrees) -> PivotFrame {
  float const yaw = yaw_degrees * (3.14159265F / 180.0F);
  return {yaw_degrees,
          local_x * std::cos(yaw) + local_z * std::sin(yaw),
          -local_x * std::sin(yaw) + local_z * std::cos(yaw)};
}

TEST(SoldierTurnSmoothing, AnAuthoritativeSlotSweptByAPivotIsWalkedNotSnapped) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.max_speed = 3.0F;
  inputs.target_x = 4.0F;
  inputs.target_z = 0.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  constexpr float k_turn_rate = 67.0F;
  float const max_step = inputs.max_speed * 1.5F * inputs.dt;
  float yaw = 0.0F;
  float last_x = state.world_x;
  float last_z = state.world_z;
  bool relocated = false;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int frame = 0; frame < 200; ++frame) {
    yaw += k_turn_rate * inputs.dt;
    auto const slot = pivot_slot(4.0F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = slot.target_z;
    result = resolve_soldier_turn_smoothing(state, inputs);
    float const moved = std::hypot(result.x - last_x, result.z - last_z);
    EXPECT_LE(moved, max_step + 1e-4F) << "frame " << frame;
    last_x = result.x;
    last_z = result.z;
    relocated = relocated || result.relocating;
  }
  EXPECT_TRUE(result.pivoting);
  EXPECT_TRUE(relocated);
  EXPECT_TRUE(result.relocating);
  EXPECT_GT(std::hypot(inputs.target_x - result.x, inputs.target_z - result.z), 0.5F);

  int frames = 0;
  do {
    result = resolve_soldier_turn_smoothing(state, inputs);
    float const moved = std::hypot(result.x - last_x, result.z - last_z);
    EXPECT_LE(moved, max_step + 1e-4F);
    last_x = result.x;
    last_z = result.z;
    ++frames;
  } while (result.relocating && frames < 600);
  EXPECT_LT(frames, 600);
  EXPECT_FALSE(result.pivoting);
  EXPECT_NEAR(result.x, inputs.target_x, inputs.settle_distance + 1e-3F);
  EXPECT_NEAR(result.z, inputs.target_z, inputs.settle_distance + 1e-3F);

  auto const settled = resolve_soldier_turn_smoothing(state, inputs);
  EXPECT_FLOAT_EQ(settled.x, inputs.target_x);
  EXPECT_FLOAT_EQ(settled.z, inputs.target_z);
  EXPECT_FALSE(settled.relocating);
}

TEST(SoldierTurnSmoothing, AnInnerManSweptSlowlyStillWalksThePivot) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.target_x = 0.8F;
  inputs.target_z = 0.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float yaw = 0.0F;
  Render::Humanoid::SoldierTurnSmoothingResult result{};
  for (int frame = 0; frame < 60; ++frame) {
    yaw += 67.0F * inputs.dt;
    auto const slot = pivot_slot(0.8F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = slot.target_z;
    result = resolve_soldier_turn_smoothing(state, inputs);
  }
  EXPECT_TRUE(result.pivoting);
  EXPECT_TRUE(result.relocating);
  EXPECT_GT(result.travel_speed, 0.3F);
  EXPECT_LT(std::hypot(inputs.target_x - result.x, inputs.target_z - result.z), 0.3F);
}

TEST(SoldierTurnSmoothing, ATranslatingAuthoritativeSlotIsNotAPivot) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.max_speed = 0.01F;
  inputs.target_x = 1.5F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  for (int frame = 0; frame < 30; ++frame) {
    inputs.formation_center_z += 2.0F * inputs.dt;
    inputs.formation_yaw_degrees += 30.0F * inputs.dt;
    auto const slot = pivot_slot(1.5F, 0.0F, inputs.formation_yaw_degrees);
    inputs.target_x = slot.target_x;
    inputs.target_z = inputs.formation_center_z + slot.target_z;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    EXPECT_FALSE(result.pivoting);
    EXPECT_FALSE(result.relocating);
    EXPECT_FLOAT_EQ(result.x, inputs.target_x);
    EXPECT_FLOAT_EQ(result.z, inputs.target_z);
  }
}

TEST(SoldierTurnSmoothing, ARefusedPivotWheelKeepsTheSlotAuthoritative) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.allow_pivot_wheel = false;
  inputs.max_speed = 0.01F;
  inputs.target_x = 4.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float yaw = 0.0F;
  for (int frame = 0; frame < 30; ++frame) {
    yaw += 67.0F * inputs.dt;
    auto const slot = pivot_slot(4.0F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = slot.target_z;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    EXPECT_FALSE(result.pivoting);
    EXPECT_FALSE(result.relocating);
    EXPECT_FLOAT_EQ(result.x, inputs.target_x);
    EXPECT_FLOAT_EQ(result.z, inputs.target_z);
  }
}

TEST(SoldierTurnSmoothing, AWingSweptByAWalkingTurnWalksAfterItsSlot) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.max_speed = 3.0F;
  inputs.target_x = 4.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float const max_step = inputs.max_speed * 1.5F * inputs.dt;
  float yaw = 0.0F;
  float last_x = state.world_x;
  float last_z = state.world_z;
  bool relocated = false;
  for (int frame = 0; frame < 90; ++frame) {
    yaw += 67.0F * inputs.dt;
    inputs.formation_center_z += 2.5F * inputs.dt;
    auto const slot = pivot_slot(4.0F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = inputs.formation_center_z + slot.target_z;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    float const moved = std::hypot(result.x - last_x, result.z - last_z);
    EXPECT_LE(moved, max_step + 1e-4F) << "frame " << frame;
    last_x = result.x;
    last_z = result.z;
    relocated = relocated || (result.pivoting && result.relocating);
  }
  EXPECT_TRUE(relocated);
}

TEST(SoldierTurnSmoothing, AnInnerFileOnAGentleMarchingTurnStaysOnItsSlot) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.max_speed = 0.01F;
  inputs.target_x = 0.8F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float yaw = 0.0F;
  for (int frame = 0; frame < 60; ++frame) {
    yaw += 30.0F * inputs.dt;
    inputs.formation_center_z += 2.0F * inputs.dt;
    auto const slot = pivot_slot(0.8F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = inputs.formation_center_z + slot.target_z;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    EXPECT_FALSE(result.pivoting);
    EXPECT_FALSE(result.relocating);
    EXPECT_FLOAT_EQ(result.x, inputs.target_x);
    EXPECT_FLOAT_EQ(result.z, inputs.target_z);
  }
}

TEST(SoldierTurnSmoothing, AWheelingManJogsFasterThanTheOrdinaryCatchUp) {
  SoldierTurnSmoothingState state{};
  auto inputs = default_inputs();
  inputs.position_is_authoritative = true;
  inputs.max_speed = 2.0F;
  inputs.target_x = 4.0F;
  std::ignore = resolve_soldier_turn_smoothing(state, inputs);

  float yaw = 0.0F;
  float best_speed = 0.0F;
  for (int frame = 0; frame < 120; ++frame) {
    yaw += 67.0F * inputs.dt;
    auto const slot = pivot_slot(4.0F, 0.0F, yaw);
    inputs.formation_yaw_degrees = slot.yaw_degrees;
    inputs.target_x = slot.target_x;
    inputs.target_z = slot.target_z;
    auto const result = resolve_soldier_turn_smoothing(state, inputs);
    best_speed = std::max(best_speed, result.travel_speed);
  }
  EXPECT_GT(best_speed, inputs.max_speed + 0.5F);
  EXPECT_LE(best_speed, inputs.max_speed * 1.5F + 1e-3F);
}

} // namespace
