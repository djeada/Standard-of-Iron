#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/pose_controller.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class HumanoidPoseControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    using HP = HumanProportions;

    pose = HumanoidPose{};
    float const head_center_y = HP::HEAD_CENTER_Y;
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
    pose.foot_l = QVector3D(-0.14F, HP::FOOT_Y_OFFSET_DEFAULT, 0.06F);
    pose.foot_r = QVector3D(0.14F, HP::FOOT_Y_OFFSET_DEFAULT, -0.06F);
    pose.foot_y_offset = HP::FOOT_Y_OFFSET_DEFAULT;

    anim_ctx = HumanoidAnimationContext{};
    anim_ctx.inputs.time = 0.0F;
    anim_ctx.inputs.is_moving = false;
    anim_ctx.inputs.is_attacking = false;
    anim_ctx.variation = VariationParams::from_seed(12345);
    anim_ctx.gait.state = HumanoidMotionState::Idle;
  }

  HumanoidPose pose;
  HumanoidAnimationContext anim_ctx;

  bool approxEqual(const QVector3D &a, const QVector3D &b,
                   float epsilon = 0.01F) {
    return std::abs(a.x() - b.x()) < epsilon &&
           std::abs(a.y() - b.y()) < epsilon &&
           std::abs(a.z() - b.z()) < epsilon;
  }
};

TEST_F(HumanoidPoseControllerTest, ConstructorInitializesCorrectly) {
  HumanoidPoseController controller(pose, anim_ctx);

  EXPECT_FLOAT_EQ(pose.head_pos.y(), HumanProportions::HEAD_CENTER_Y);
  EXPECT_FLOAT_EQ(pose.pelvis_pos.y(), HumanProportions::WAIST_Y);
}

TEST_F(HumanoidPoseControllerTest, StandIdleDoesNotModifyPose) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_shoulder_l = pose.shoulder_l;

  controller.stand_idle();

  EXPECT_TRUE(approxEqual(pose.pelvis_pos, original_pelvis));
  EXPECT_TRUE(approxEqual(pose.shoulder_l, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, KneelLowersPelvis) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvis_pos.y();

  controller.kneel(0.5F);

  EXPECT_LT(pose.pelvis_pos.y(), original_pelvis_y);

  float const expected_offset = 0.5F * 0.40F;
  EXPECT_NEAR(pose.pelvis_pos.y(), HumanProportions::WAIST_Y - expected_offset,
              0.05F);
}

TEST_F(HumanoidPoseControllerTest, KneelFullDepthTouchesGroundWithKnee) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.kneel(1.0F);

  EXPECT_NEAR(pose.knee_l.y(), HumanProportions::GROUND_Y + 0.07F, 0.02F);

  EXPECT_LT(pose.pelvis_pos.y(), HumanProportions::WAIST_Y - 0.35F);
}

TEST_F(HumanoidPoseControllerTest, KneelZeroDepthKeepsStanding) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvis_pos.y();

  controller.kneel(0.0F);

  EXPECT_NEAR(pose.pelvis_pos.y(), original_pelvis_y, 0.01F);
}

TEST_F(HumanoidPoseControllerTest, LeanMovesUpperBody) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulder_l;
  QVector3D const original_shoulder_r = pose.shoulder_r;
  QVector3D const lean_direction(0.0F, 0.0F, 1.0F);

  controller.lean(lean_direction, 0.5F);

  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l.z());
  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest, LeanZeroAmountNoChange) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_shoulder_l = pose.shoulder_l;
  QVector3D const lean_direction(1.0F, 0.0F, 0.0F);

  controller.lean(lean_direction, 0.0F);

  EXPECT_TRUE(approxEqual(pose.shoulder_l, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtSetsHandPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);

  controller.place_hand_at(false, target_position);

  EXPECT_TRUE(approxEqual(pose.hand_r, target_position));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtComputesElbow) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);
  QVector3D const original_elbow = pose.elbow_r;

  controller.place_hand_at(false, target_position);

  EXPECT_FALSE(approxEqual(pose.elbow_r, original_elbow));

  float const shoulder_to_elbow_dist =
      (pose.elbow_r - pose.shoulder_r).length();
  float const elbow_to_hand_dist = (target_position - pose.elbow_r).length();
  EXPECT_GT(shoulder_to_elbow_dist, 0.0F);
  EXPECT_GT(elbow_to_hand_dist, 0.0F);
}

TEST_F(HumanoidPoseControllerTest, SolveElbowIKReturnsValidPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const shoulder = pose.shoulder_r;
  QVector3D const hand(0.35F, 1.15F, 0.75F);
  QVector3D const outward_dir(1.0F, 0.0F, 0.0F);

  QVector3D const elbow = controller.solve_elbow_ik(
      false, shoulder, hand, outward_dir, 0.45F, 0.15F, 0.0F, 1.0F);

  EXPECT_GT(elbow.length(), 0.0F);

  float const shoulder_elbow_dist = (elbow - shoulder).length();
  EXPECT_GT(shoulder_elbow_dist, 0.05F);
  EXPECT_LT(shoulder_elbow_dist, 0.50F);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKReturnsValidPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const hip(0.10F, 0.93F, 0.0F);
  QVector3D const foot(0.10F, 0.0F, 0.05F);
  float const height_scale = 1.0F;

  QVector3D const knee =
      controller.solve_knee_ik(false, hip, foot, height_scale);

  EXPECT_LT(knee.y(), hip.y());
  EXPECT_GT(knee.y(), foot.y());

  EXPECT_GE(knee.y(), HumanProportions::GROUND_Y);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKPreventsGroundPenetration) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const hip(0.0F, 0.30F, 0.0F);
  QVector3D const foot(0.50F, 0.0F, 0.50F);
  float const height_scale = 1.0F;

  QVector3D const knee =
      controller.solve_knee_ik(true, hip, foot, height_scale);

  float const min_knee_y =
      HumanProportions::GROUND_Y + pose.foot_y_offset * 0.5F;
  EXPECT_GE(knee.y(), min_knee_y - 0.001F);
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtLeftHandWorks) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(-0.40F, 1.30F, 0.60F);

  controller.place_hand_at(true, target_position);

  EXPECT_TRUE(approxEqual(pose.hand_l, target_position));

  EXPECT_GT((pose.elbow_l - pose.shoulder_l).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest, KneelClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.kneel(1.5F);
  float const max_kneel_pelvis_y = pose.pelvis_pos.y();

  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  controller2.kneel(1.0F);

  EXPECT_NEAR(pose.pelvis_pos.y(), max_kneel_pelvis_y, 0.001F);
}

TEST_F(HumanoidPoseControllerTest, LeanClampsBounds) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const lean_direction(0.0F, 0.0F, 1.0F);

  controller.lean(lean_direction, 1.5F);
  float const max_lean_z = pose.shoulder_l.z();

  SetUp();
  HumanoidPoseController controller2(pose, anim_ctx);

  controller2.lean(lean_direction, 1.0F);

  EXPECT_NEAR(pose.shoulder_l.z(), max_lean_z, 0.001F);
}

TEST_F(HumanoidPoseControllerTest, HoldSwordAndShieldPositionsHandsCorrectly) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.hold_sword_and_shield();

  EXPECT_GT(pose.hand_r.x(), 0.28F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y);
  EXPECT_GT(pose.hand_r.z(), 0.36F);

  EXPECT_LT(pose.hand_l.x(), -0.26F);
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y + 0.01F);
  EXPECT_GT(pose.hand_l.z(), 0.20F);

  EXPECT_GT((pose.elbow_r - pose.shoulder_r).length(), 0.0F);
  EXPECT_GT((pose.elbow_l - pose.shoulder_l).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest,
       HoldSwordAndShieldCarriesFartherForwardWhileMoving) {
  HumanoidPose idle_pose = pose;
  HumanoidAnimationContext idle_anim = anim_ctx;
  HumanoidPoseController idle_controller(idle_pose, idle_anim);
  idle_controller.hold_sword_and_shield();

  HumanoidPose moving_pose = pose;
  HumanoidAnimationContext moving_anim = anim_ctx;
  moving_anim.inputs.is_moving = true;
  moving_anim.gait.speed = 1.5F;
  HumanoidPoseController moving_controller(moving_pose, moving_anim);
  moving_controller.hold_sword_and_shield();

  EXPECT_GT(moving_pose.hand_r.z(), idle_pose.hand_r.z());
  EXPECT_LT(moving_pose.hand_r.y(), idle_pose.hand_r.y());
  EXPECT_GT(moving_pose.hand_l.z(), idle_pose.hand_l.z());
  EXPECT_LT(moving_pose.hand_l.y(), idle_pose.hand_l.y());
}

TEST_F(HumanoidPoseControllerTest, LookAtMovesHeadTowardTarget) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_head_pos = pose.head_pos;
  QVector3D const target(0.5F, pose.head_pos.y(), 2.0F);

  controller.look_at(target);

  EXPECT_GT(pose.head_pos.x(), original_head_pos.x());
  EXPECT_GT(pose.head_pos.z(), original_head_pos.z());
}

TEST_F(HumanoidPoseControllerTest, LookAtWithSamePositionDoesNothing) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_head_pos = pose.head_pos;

  controller.look_at(pose.head_pos);

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

  EXPECT_LT(kneeling_shoulder_y, original_shoulder_y);
}

TEST_F(HumanoidPoseControllerTest, MeleeStrikeAppliesTorsoTwistAtWindUp) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();
  float const original_shoulder_l_z = pose.shoulder_l.z();

  controller.melee_strike(0.24F);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);

  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l_z);
}

TEST_F(HumanoidPoseControllerTest, MeleeStrikeAppliesTorsoTwistAtStrike) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.melee_strike(0.55F);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
}

TEST_F(HumanoidPoseControllerTest, SpearThrustAppliesTorsoTwistAtChamber) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.spear_thrust(0.23F);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);

  EXPECT_GT(pose.shoulder_l.z(), pose.shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest, SpearThrustAppliesTorsoTwistAtExtension) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.spear_thrust(0.54F);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
}

TEST_F(HumanoidPoseControllerTest, SwordSlashAppliesTorsoTwistAtChamber) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();
  float const original_shoulder_l_z = pose.shoulder_l.z();

  controller.sword_slash(0.20F);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l_z);
}

TEST_F(HumanoidPoseControllerTest, SwordSlashAppliesTorsoTwistAtFollowThrough) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.sword_slash(0.55F);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
}

TEST_F(HumanoidPoseControllerTest, SwordSlashVariantAppliesTorsoTwist) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();
  float const original_shoulder_l_z = pose.shoulder_l.z();

  controller.sword_slash_variant(0.20F, 0);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l_z);
}

TEST_F(HumanoidPoseControllerTest,
       SwordSlashVariant1ReversesInitialTorsoTwist) {
  HumanoidPoseController controller_v0(pose, anim_ctx);
  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller_v0.sword_slash_variant(0.20F, 1);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
}

TEST_F(HumanoidPoseControllerTest, SpearThrustVariantAppliesTorsoTwist) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.spear_thrust_variant(0.23F, 0);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);

  EXPECT_GT(pose.shoulder_l.z(), pose.shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest, SpearThrustFromHoldAppliesTorsoTwist) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();
  float const original_shoulder_l_z = pose.shoulder_l.z();

  controller.spear_thrust_from_hold(0.18F, 0.5F);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l_z);
}
