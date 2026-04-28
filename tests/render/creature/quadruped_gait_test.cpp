#include "render/creature/quadruped/gait.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using namespace Render::Creature::Quadruped;

TEST(QuadrupedGaitTest, WrapPhaseNormalizesNegativeAndOverflowInput) {
  EXPECT_NEAR(wrap_phase(-0.25F), 0.75F, 1.0e-6F);
  EXPECT_NEAR(wrap_phase(1.25F), 0.25F, 1.0e-6F);
}

TEST(QuadrupedGaitTest, DefaultFootPositionsAreMirroredAcrossBodyCenterline) {
  Dimensions dims;
  dims.body_width = 2.0F;
  dims.body_height = 1.0F;
  dims.body_length = 3.0F;
  QVector3D const barrel_center(0.1F, 0.8F, -0.2F);

  QVector3D const fl =
      default_foot_position(dims, LegId::FrontLeft, barrel_center);
  QVector3D const fr =
      default_foot_position(dims, LegId::FrontRight, barrel_center);
  QVector3D const bl =
      default_foot_position(dims, LegId::BackLeft, barrel_center);
  QVector3D const br =
      default_foot_position(dims, LegId::BackRight, barrel_center);

  EXPECT_NEAR(fl.x() - barrel_center.x(), -(fr.x() - barrel_center.x()),
              1.0e-6F);
  EXPECT_NEAR(bl.x() - barrel_center.x(), -(br.x() - barrel_center.x()),
              1.0e-6F);
  EXPECT_GT(fl.z(), bl.z());
  EXPECT_GT(fr.z(), br.z());
}

TEST(QuadrupedGaitTest, EvaluateCycleMotionRaisesLocomotionAndSwayWhenMoving) {
  Dimensions dims;
  dims.barrel_center_y = 0.9F;
  dims.idle_bob_amplitude = 0.01F;
  dims.move_bob_amplitude = 0.04F;

  Gait gait;
  gait.cycle_time = 0.8F;
  gait.stride_swing = 0.6F;
  gait.stride_lift = 0.22F;

  MotionSample const idle =
      evaluate_cycle_motion(dims, gait, 1.7F, false, false, false);
  MotionSample const moving =
      evaluate_cycle_motion(dims, gait, 1.7F, true, true, false);

  EXPECT_FALSE(idle.is_moving);
  EXPECT_TRUE(moving.is_moving);
  EXPECT_GT(moving.locomotion_intensity, idle.locomotion_intensity);
  EXPECT_GT(std::abs(moving.body_sway), std::abs(idle.body_sway));
  EXPECT_NEAR(idle.barrel_center.y(), dims.barrel_center_y + idle.bob, 1.0e-6F);
  EXPECT_NEAR(moving.barrel_center.y(), dims.barrel_center_y + moving.bob,
              1.0e-6F);
}

TEST(QuadrupedGaitTest, SwingTargetMovesFeetForwardAlongLegSide) {
  Dimensions dims;
  dims.body_width = 1.8F;
  dims.body_height = 1.2F;
  dims.body_length = 4.0F;
  QVector3D const barrel_center(0.0F, 1.0F, 0.0F);

  QVector3D const front =
      swing_target(dims, LegId::FrontLeft, barrel_center, 0.5F);
  QVector3D const back =
      swing_target(dims, LegId::BackRight, barrel_center, 0.5F);

  EXPECT_GT(front.z(),
            default_foot_position(dims, LegId::FrontLeft, barrel_center).z());
  EXPECT_LT(back.z(),
            default_foot_position(dims, LegId::BackRight, barrel_center).z());
}

} // namespace
