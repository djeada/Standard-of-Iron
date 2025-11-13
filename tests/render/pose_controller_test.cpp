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
    using HP = HumanProportions;

    // Initialize a default pose with basic standing configuration
    pose = HumanoidPose{};
    float const head_center_y = 0.5F * (HP::HEAD_TOP_Y + HP::CHIN_Y);
    float const half_shoulder = 0.5F * HP::SHOULDER_WIDTH;
    pose.head_pos = QVector3D(0.0F, head_center_y, 0.0F);
    pose.head_r = HP::HEAD_RADIUS;
    pose.neck_base = QVector3D(0.0F, HP::NECK_BASE_Y, 0.0F);
    pose.shoulder_l = QVector3D(-half_shoulder, HP::SHOULDER_Y, 0.0F);
    pose.shoulder_r = QVector3D(half_shoulder, HP::SHOULDER_Y, 0.0F);
    pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
    pose.hand_l = QVector3D(-0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);
    pose.hand_r = QVector3D(0.15F, HP::SHOULDER_Y + 0.15F, 0.20F);
    pose.elbow_l = QVector3D(-0.15F, HP::SHOULDER_Y - 0.15F, 0.25F);
    pose.elbow_r = QVector3D(0.25F, HP::SHOULDER_Y - 0.10F, 0.10F);
    pose.knee_l = QVector3D(-0.10F, HP::KNEE_Y, 0.05F);
    pose.knee_r = QVector3D(0.10F, HP::KNEE_Y, -0.05F);
    pose.foot_l = QVector3D(-0.14F, 0.022F, 0.06F);
    pose.foot_r = QVector3D(0.14F, 0.022F, -0.06F);
    pose.foot_y_offset = 0.022F;

    // Initialize animation context with default idle state
    anim_ctx = HumanoidAnimationContext{};
    anim_ctx.inputs.time = 0.0F;
    anim_ctx.inputs.is_moving = false;
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
  EXPECT_FLOAT_EQ(pose.head_pos.y(), 0.5F * (HumanProportions::HEAD_TOP_Y +
                                            HumanProportions::CHIN_Y));
  EXPECT_FLOAT_EQ(pose.pelvis_pos.y(), HumanProportions::WAIST_Y);
}

TEST_F(HumanoidPoseControllerTest, StandIdleDoesNotModifyPose) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_shoulder_l = pose.shoulder_l;

  controller.standIdle();

  // standIdle should be a no-op, keeping pose unchanged
  EXPECT_TRUE(approxEqual(pose.pelvis_pos, original_pelvis));
  EXPECT_TRUE(approxEqual(pose.shoulder_l, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, KneelLowersPelvis) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvis_pos.y();

  controller.kneel(0.5F);

  // Kneeling should lower the pelvis
  EXPECT_LT(pose.pelvis_pos.y(), original_pelvis_y);

  // Pelvis should be lowered by approximately depth * 0.40F
  float const expected_offset = 0.5F * 0.40F;
  EXPECT_NEAR(pose.pelvis_pos.y(), HumanProportions::WAIST_Y - expected_offset,
              0.05F);
}

TEST_F(HumanoidPoseControllerTest, KneelFullDepthTouchesGroundWithKnee) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.kneel(1.0F);

  // At full kneel, left knee should be very close to ground
  EXPECT_NEAR(pose.knee_l.y(), HumanProportions::GROUND_Y + 0.07F, 0.02F);

  // Pelvis should be lowered significantly
  EXPECT_LT(pose.pelvis_pos.y(), HumanProportions::WAIST_Y - 0.35F);
}

TEST_F(HumanoidPoseControllerTest, KneelZeroDepthKeepsStanding) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvis_pos.y();

  controller.kneel(0.0F);

  // Zero depth should keep pelvis at original height
  EXPECT_NEAR(pose.pelvis_pos.y(), original_pelvis_y, 0.01F);
}

TEST_F(HumanoidPoseControllerTest, LeanMovesUpperBody) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulder_l;
  QVector3D const original_shoulder_r = pose.shoulder_r;
  QVector3D const lean_direction(0.0F, 0.0F, 1.0F); // Forward

  controller.lean(lean_direction, 0.5F);

  // Shoulders should move forward when leaning forward
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l.z());
  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest, LeanZeroAmountNoChange) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulder_l;
  QVector3D const lean_direction(1.0F, 0.0F, 0.0F); // Right

  controller.lean(lean_direction, 0.0F);

  // Zero amount should keep shoulders unchanged
  EXPECT_TRUE(approxEqual(pose.shoulder_l, original_shoulder_l));
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
  QVector3D const original_elbow = pose.elbow_r;

  controller.placeHandAt(false, target_position); // Right hand

  // Elbow should be recomputed (different from original)
  EXPECT_FALSE(approxEqual(pose.elbow_r, original_elbow));

  // Elbow should be between shoulder and hand
  float const shoulder_to_elbow_dist = (pose.elbow_r - pose.shoulder_r).length();
  float const elbow_to_hand_dist = (target_position - pose.elbow_r).length();
  EXPECT_GT(shoulder_to_elbow_dist, 0.0F);
  EXPECT_GT(elbow_to_hand_dist, 0.0F);
}

TEST_F(HumanoidPoseControllerTest, SolveElbowIKReturnsValidPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const shoulder = pose.shoulder_r;
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
  float const min_knee_y = HumanProportions::GROUND_Y + pose.foot_y_offset * 0.5F;
  EXPECT_GE(knee.y(), min_knee_y - 0.001F); // Small epsilon for floating point
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtLeftHandWorks) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(-0.40F, 1.30F, 0.60F);

  controller.placeHandAt(true, target_position); // Left hand

  // Left hand should be at target position
  EXPECT_TRUE(approxEqual(pose.hand_l, target_position));

  // Left elbow should be computed
  EXPECT_GT((pose.elbow_l - pose.shoulder_l).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest, KneelClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  // Test clamping of depth > 1.0
  controller.kneel(1.5F);
  float const max_kneel_pelvis_y = pose.pelvis_pos.y();

  // Reset pose
  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  // Test depth = 1.0
  controller2.kneel(1.0F);

  // Should be same as clamped 1.5F
  EXPECT_NEAR(pose.pelvis_pos.y(), max_kneel_pelvis_y, 0.001F);
}

TEST_F(HumanoidPoseControllerTest, LeanClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const lean_direction(0.0F, 0.0F, 1.0F);

  // Test clamping of amount > 1.0
  controller.lean(lean_direction, 1.5F);
  float const max_lean_z = pose.shoulder_l.z();

  // Reset pose
  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  // Test amount = 1.0
  controller2.lean(lean_direction, 1.0F);

  // Should be same as clamped 1.5F
  EXPECT_NEAR(pose.shoulder_l.z(), max_lean_z, 0.001F);
}

TEST_F(HumanoidPoseControllerTest, HoldSwordAndShieldPositionsHandsCorrectly) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.hold_sword_and_shield();

  // Right hand (sword hand) should be positioned for sword holding
  EXPECT_GT(pose.hand_r.x(), 0.0F); // To the right
  EXPECT_GT(pose.hand_r.z(), 0.0F); // In front

  // Left hand (shield hand) should be positioned for shield holding
  EXPECT_LT(pose.hand_l.x(), 0.0F); // To the left
  EXPECT_GT(pose.hand_l.z(), 0.0F); // In front

  // Both elbows should be computed
  EXPECT_GT((pose.elbow_r - pose.shoulder_r).length(), 0.0F);
  EXPECT_GT((pose.elbow_l - pose.shoulder_l).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest, LookAtMovesHeadTowardTarget) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_head_pos = pose.head_pos;
  QVector3D const target(0.5F, pose.head_pos.y(), 2.0F); // Target in front and to the right

  controller.look_at(target);

  // Head should move toward target (right and forward)
  EXPECT_GT(pose.head_pos.x(), original_head_pos.x());
  EXPECT_GT(pose.head_pos.z(), original_head_pos.z());
}

TEST_F(HumanoidPoseControllerTest, LookAtWithSamePositionDoesNothing) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_head_pos = pose.head_pos;
  
  controller.look_at(pose.head_pos); // Look at current position

  // Head should remain unchanged
  EXPECT_TRUE(approxEqual(pose.head_pos, original_head_pos));
}

TEST_F(HumanoidPoseControllerTest, GetShoulderYReturnsCorrectValues) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const left_y = controller.get_shoulder_y(true);
  float const right_y = controller.get_shoulder_y(false);

  EXPECT_FLOAT_EQ(left_y, pose.shoulder_l.y());
  EXPECT_FLOAT_EQ(right_y, pose.shoulder_r.y());
}

TEST_F(HumanoidPoseControllerTest, GetPelvisYReturnsCorrectValue) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const pelvis_y = controller.get_pelvis_y();

  EXPECT_FLOAT_EQ(pelvis_y, pose.pelvis_pos.y());
}

TEST_F(HumanoidPoseControllerTest, GetShoulderYReflectsKneeling) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_y = controller.get_shoulder_y(true);
  
  controller.kneel(0.5F);
  
  float const kneeling_shoulder_y = controller.get_shoulder_y(true);

  // After kneeling, shoulder should be lower
  EXPECT_LT(kneeling_shoulder_y, original_shoulder_y);
}
