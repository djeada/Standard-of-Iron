#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class HumanoidPoseControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize a default pose with basic standing configuration
    pose = HumanoidPose{};
    pose.headPos = QVector3D(0.0F, 1.70F, 0.0F);
    pose.headR = 0.10F;
    pose.neck_base = QVector3D(0.0F, 1.49F, 0.0F);
    pose.shoulderL = QVector3D(-0.21F, 1.45F, 0.0F);
    pose.shoulderR = QVector3D(0.21F, 1.45F, 0.0F);
    pose.pelvisPos = QVector3D(0.0F, 0.95F, 0.0F);
    pose.handL = QVector3D(-0.05F, 1.50F, 0.55F);
    pose.hand_r = QVector3D(0.15F, 1.60F, 0.20F);
    pose.elbowL = QVector3D(-0.15F, 1.30F, 0.25F);
    pose.elbowR = QVector3D(0.25F, 1.35F, 0.10F);
    pose.knee_l = QVector3D(-0.10F, 0.44F, 0.05F);
    pose.knee_r = QVector3D(0.10F, 0.44F, -0.05F);
    pose.footL = QVector3D(-0.14F, 0.022F, 0.06F);
    pose.foot_r = QVector3D(0.14F, 0.022F, -0.06F);
    pose.footYOffset = 0.022F;

    // Initialize animation context with default idle state
    anim_ctx = HumanoidAnimationContext{};
    anim_ctx.inputs.time = 0.0F;
    anim_ctx.inputs.isMoving = false;
    anim_ctx.inputs.is_attacking = false;
    anim_ctx.variation = VariationParams::fromSeed(12345);
    anim_ctx.gait.state = HumanoidMotionState::Idle;
  }

  HumanoidPose pose;
  HumanoidAnimationContext anim_ctx;

  // Helper to check if a position is approximately equal
  bool approxEqual(const QVector3D &a, const QVector3D &b,
                   float epsilon = 0.01F) {
    return std::abs(a.x() - b.x()) < epsilon &&
           std::abs(a.y() - b.y()) < epsilon &&
           std::abs(a.z() - b.z()) < epsilon;
  }
};

TEST_F(HumanoidPoseControllerTest, ConstructorInitializesCorrectly) {
  HumanoidPoseController controller(pose, anim_ctx);
  // Constructor should not modify the pose
  EXPECT_FLOAT_EQ(pose.headPos.y(), 1.70F);
  EXPECT_FLOAT_EQ(pose.pelvisPos.y(), 0.95F);
}

TEST_F(HumanoidPoseControllerTest, StandIdleDoesNotModifyPose) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvisPos;
  QVector3D const original_shoulder_l = pose.shoulderL;

  controller.standIdle();

  // standIdle should be a no-op, keeping pose unchanged
  EXPECT_TRUE(approxEqual(pose.pelvisPos, original_pelvis));
  EXPECT_TRUE(approxEqual(pose.shoulderL, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, KneelLowersPelvis) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvisPos.y();

  controller.kneel(0.5F);

  // Kneeling should lower the pelvis
  EXPECT_LT(pose.pelvisPos.y(), original_pelvis_y);

  // Pelvis should be lowered by approximately depth * 0.40F
  float const expected_offset = 0.5F * 0.40F;
  EXPECT_NEAR(pose.pelvisPos.y(), HumanProportions::WAIST_Y - expected_offset,
              0.05F);
}

TEST_F(HumanoidPoseControllerTest, KneelFullDepthTouchesGroundWithKnee) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.kneel(1.0F);

  // At full kneel, left knee should be very close to ground
  EXPECT_NEAR(pose.knee_l.y(), HumanProportions::GROUND_Y + 0.07F, 0.02F);

  // Pelvis should be lowered significantly
  EXPECT_LT(pose.pelvisPos.y(), HumanProportions::WAIST_Y - 0.35F);
}

TEST_F(HumanoidPoseControllerTest, KneelZeroDepthKeepsStanding) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvisPos.y();

  controller.kneel(0.0F);

  // Zero depth should keep pelvis at original height
  EXPECT_NEAR(pose.pelvisPos.y(), original_pelvis_y, 0.01F);
}

TEST_F(HumanoidPoseControllerTest, LeanMovesUpperBody) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulderL;
  QVector3D const original_shoulder_r = pose.shoulderR;
  QVector3D const lean_direction(0.0F, 0.0F, 1.0F); // Forward

  controller.lean(lean_direction, 0.5F);

  // Shoulders should move forward when leaning forward
  EXPECT_GT(pose.shoulderL.z(), original_shoulder_l.z());
  EXPECT_GT(pose.shoulderR.z(), original_shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest, LeanZeroAmountNoChange) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulderL;
  QVector3D const lean_direction(1.0F, 0.0F, 0.0F); // Right

  controller.lean(lean_direction, 0.0F);

  // Zero amount should keep shoulders unchanged
  EXPECT_TRUE(approxEqual(pose.shoulderL, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtSetsHandPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);

  controller.placeHandAt(false, target_position); // Right hand

  // Hand should be at target position
  EXPECT_TRUE(approxEqual(pose.hand_r, target_position));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtComputesElbow) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);
  QVector3D const original_elbow = pose.elbowR;

  controller.placeHandAt(false, target_position); // Right hand

  // Elbow should be recomputed (different from original)
  EXPECT_FALSE(approxEqual(pose.elbowR, original_elbow));

  // Elbow should be between shoulder and hand
  float const shoulder_to_elbow_dist = (pose.elbowR - pose.shoulderR).length();
  float const elbow_to_hand_dist = (target_position - pose.elbowR).length();
  EXPECT_GT(shoulder_to_elbow_dist, 0.0F);
  EXPECT_GT(elbow_to_hand_dist, 0.0F);
}

TEST_F(HumanoidPoseControllerTest, SolveElbowIKReturnsValidPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const shoulder = pose.shoulderR;
  QVector3D const hand(0.35F, 1.15F, 0.75F);
  QVector3D const outward_dir(1.0F, 0.0F, 0.0F);

  QVector3D const elbow = controller.solveElbowIK(
      false, shoulder, hand, outward_dir, 0.45F, 0.15F, 0.0F, 1.0F);

  // Elbow should be somewhere between shoulder and hand
  EXPECT_GT(elbow.length(), 0.0F);

  // Distance from shoulder to elbow should be reasonable
  float const shoulder_elbow_dist = (elbow - shoulder).length();
  EXPECT_GT(shoulder_elbow_dist, 0.05F);
  EXPECT_LT(shoulder_elbow_dist, 0.50F);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKReturnsValidPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const hip(0.10F, 0.93F, 0.0F);
  QVector3D const foot(0.10F, 0.0F, 0.05F);
  float const height_scale = 1.0F;

  QVector3D const knee = controller.solveKneeIK(false, hip, foot, height_scale);

  // Knee should be between hip and foot (in Y)
  EXPECT_LT(knee.y(), hip.y());
  EXPECT_GT(knee.y(), foot.y());

  // Knee should not be below ground
  EXPECT_GE(knee.y(), HumanProportions::GROUND_Y);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKPreventsGroundPenetration) {
  HumanoidPoseController controller(pose, anim_ctx);

  // Set up a scenario where IK would put knee below ground
  QVector3D const hip(0.0F, 0.30F, 0.0F);   // Very low hip
  QVector3D const foot(0.50F, 0.0F, 0.50F); // Far foot
  float const height_scale = 1.0F;

  QVector3D const knee = controller.solveKneeIK(true, hip, foot, height_scale);

  // Knee should be at or above the floor threshold
  float const min_knee_y = HumanProportions::GROUND_Y + pose.footYOffset * 0.5F;
  EXPECT_GE(knee.y(), min_knee_y - 0.001F); // Small epsilon for floating point
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtLeftHandWorks) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(-0.40F, 1.30F, 0.60F);

  controller.placeHandAt(true, target_position); // Left hand

  // Left hand should be at target position
  EXPECT_TRUE(approxEqual(pose.handL, target_position));

  // Left elbow should be computed
  EXPECT_GT((pose.elbowL - pose.shoulderL).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest, KneelClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  // Test clamping of depth > 1.0
  controller.kneel(1.5F);
  float const max_kneel_pelvis_y = pose.pelvisPos.y();

  // Reset pose
  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  // Test depth = 1.0
  controller2.kneel(1.0F);

  // Should be same as clamped 1.5F
  EXPECT_NEAR(pose.pelvisPos.y(), max_kneel_pelvis_y, 0.001F);
}

TEST_F(HumanoidPoseControllerTest, LeanClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const lean_direction(0.0F, 0.0F, 1.0F);

  // Test clamping of amount > 1.0
  controller.lean(lean_direction, 1.5F);
  float const max_lean_z = pose.shoulderL.z();

  // Reset pose
  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  // Test amount = 1.0
  controller2.lean(lean_direction, 1.0F);

  // Should be same as clamped 1.5F
  EXPECT_NEAR(pose.shoulderL.z(), max_lean_z, 0.001F);
}
