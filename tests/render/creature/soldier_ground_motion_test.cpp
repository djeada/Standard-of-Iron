#include <cmath>
#include <gtest/gtest.h>

#include "render/creature/humanoid/runtime/soldier_ground_motion.h"

namespace {

using Render::Humanoid::observe_soldier_ground_motion;
using Render::Humanoid::SoldierGroundMotion;
using Render::Humanoid::SoldierGroundMotionInputs;
using Render::Humanoid::SoldierGroundMotionState;

constexpr float k_dt = 1.0F / 30.0F;

struct Walker {
  SoldierGroundMotionState state;
  SoldierGroundMotionInputs inputs;
  float slid{0.0F};

  auto stand() -> SoldierGroundMotion {
    return observe_soldier_ground_motion(state, inputs);
  }

  auto move(float vx, float vz, float turn_rate = 0.0F) -> SoldierGroundMotion {
    inputs.x += vx * k_dt;
    inputs.z += vz * k_dt;
    inputs.yaw_degrees += turn_rate * k_dt;
    inputs.time += k_dt;
    auto const motion = observe_soldier_ground_motion(state, inputs);
    if (!motion.stepping) {
      slid += std::hypot(vx, vz) * k_dt;
    }
    return motion;
  }
};

auto rested_walker() -> Walker {
  Walker walker;
  walker.inputs.x = 4.0F;
  walker.inputs.z = -2.0F;
  walker.inputs.time = 10.0F;
  (void)walker.stand();
  for (int frame = 0; frame < 30; ++frame) {
    (void)walker.move(0.0F, 0.0F);
  }
  return walker;
}

} // namespace

TEST(SoldierGroundMotion, AStandingBodyDoesNotStep) {
  Walker walker = rested_walker();
  for (int frame = 0; frame < 90; ++frame) {
    auto const motion = walker.move(0.0F, 0.0F);
    EXPECT_FALSE(motion.stepping);
    EXPECT_FLOAT_EQ(motion.stride_presence, 0.0F);
  }
}

TEST(SoldierGroundMotion, AShoveMakesTheBodyStepWithinAHandsBreadth) {
  Walker walker = rested_walker();
  SoldierGroundMotion motion{};
  for (int frame = 0; frame < 30; ++frame) {
    motion = walker.move(0.4F, 0.0F);
  }
  EXPECT_TRUE(motion.stepping);
  EXPECT_FALSE(motion.running);
  EXPECT_LT(walker.slid, 0.10F) << "he slid " << walker.slid << " m before stepping";
  EXPECT_NEAR(motion.speed, 0.4F, 0.05F);
  EXPECT_GT(motion.travel_x, 0.95F) << "the legs step the way the body went";
  EXPECT_LT(motion.stride_presence, 0.5F) << "a shuffle, not a marching stride";
  EXPECT_GE(motion.stride_presence,
            Render::Humanoid::k_ground_motion_min_stride_presence);
}

TEST(SoldierGroundMotion, TheLegsStopSoonAfterTheBodyDoes) {
  Walker walker = rested_walker();
  for (int frame = 0; frame < 30; ++frame) {
    (void)walker.move(0.0F, 0.5F);
  }
  float walked_on_the_spot = 0.0F;
  SoldierGroundMotion motion{};
  for (int frame = 0; frame < 60; ++frame) {
    motion = walker.move(0.0F, 0.0F);
    if (motion.stepping) {
      walked_on_the_spot += k_dt;
    }
  }
  EXPECT_FALSE(motion.stepping);
  EXPECT_LT(walked_on_the_spot, 0.5F);
}

TEST(SoldierGroundMotion, AFrameToFrameWobbleIsNotAStep) {
  Walker walker = rested_walker();
  for (int frame = 0; frame < 120; ++frame) {
    float const wobble = (frame % 2 == 0 ? 0.03F : -0.03F) / k_dt;
    auto const motion = walker.move(wobble, 0.0F);
    EXPECT_FALSE(motion.stepping) << "frame " << frame;
  }
}

TEST(SoldierGroundMotion, ASlowDriftIsPaidOffWithAStep) {
  Walker walker = rested_walker();
  bool stepped = false;
  float drifted_before_step = 0.0F;
  for (int frame = 0; frame < 150 && !stepped; ++frame) {
    stepped = walker.move(0.04F, 0.0F).stepping;
    drifted_before_step += 0.04F * k_dt;
  }
  EXPECT_TRUE(stepped) << "a crowd easing him aside still makes him move his feet";
  EXPECT_LT(drifted_before_step, 0.12F);
}

TEST(SoldierGroundMotion, ACarriedBodyStandsStill) {
  Walker walker = rested_walker();
  walker.inputs.carried = true;
  for (int frame = 0; frame < 60; ++frame) {
    auto const motion = walker.move(1.5F, 0.0F);
    EXPECT_FALSE(motion.stepping) << "a man on a raft is not walking";
  }
  walker.inputs.carried = false;
  SoldierGroundMotion motion{};
  for (int frame = 0; frame < 10; ++frame) {
    motion = walker.move(1.5F, 0.0F);
  }
  EXPECT_TRUE(motion.stepping) << "he walks off it";
}

TEST(SoldierGroundMotion, AMarchIsAFullStrideAndASprintIsARun) {
  Walker walker = rested_walker();
  SoldierGroundMotion motion{};
  for (int frame = 0; frame < 30; ++frame) {
    motion = walker.move(0.0F, 2.0F);
  }
  EXPECT_TRUE(motion.stepping);
  EXPECT_FALSE(motion.running);
  EXPECT_FLOAT_EQ(motion.stride_presence, 1.0F);
  for (int frame = 0; frame < 30; ++frame) {
    motion = walker.move(0.0F, 3.6F);
  }
  EXPECT_TRUE(motion.running);
  for (int frame = 0; frame < 30; ++frame) {
    motion = walker.move(0.0F, 2.0F);
  }
  EXPECT_TRUE(motion.stepping);
  EXPECT_FALSE(motion.running);
}

TEST(SoldierGroundMotion, TurningOnTheSpotMovesTheFeet) {
  Walker walker = rested_walker();
  SoldierGroundMotion motion{};
  for (int frame = 0; frame < 20; ++frame) {
    motion = walker.move(0.0F, 0.0F, 120.0F);
  }
  EXPECT_TRUE(motion.stepping);
  EXPECT_FALSE(motion.travelling);
  EXPECT_GT(motion.stride_speed, 0.2F);
}

TEST(SoldierGroundMotion, AFirstSightingTakesTheDeclaredVelocity) {
  Walker walker;
  walker.inputs.time = 3.0F;
  walker.inputs.seed_velocity_z = 2.0F;
  auto const first = walker.stand();
  EXPECT_TRUE(first.stepping) << "a marching unit entering view is already walking";
  EXPECT_NEAR(first.speed, 2.0F, 1.0e-4F);

  walker.inputs.time += 2.0F;
  walker.inputs.x += 30.0F;
  walker.inputs.seed_velocity_z = 0.0F;
  auto const after_gap = walker.stand();
  EXPECT_FALSE(after_gap.stepping) << "a stale sighting is not a 30 m stride";
}

TEST(SoldierGroundMotion, TheSameFrameSeenTwiceChangesNothing) {
  Walker walker = rested_walker();
  for (int frame = 0; frame < 10; ++frame) {
    (void)walker.move(0.6F, 0.0F);
  }
  auto const once = walker.stand();
  auto const twice = walker.stand();
  EXPECT_FLOAT_EQ(once.speed, twice.speed);
  EXPECT_EQ(once.stepping, twice.stepping);
}
