#include "render/humanoid/humanoid_math.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/pose_controller.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class PoseControllerCompatibilityTest : public ::testing::Test {
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
    pose.foot_l = QVector3D(-0.14F, HP::FOOT_Y_OFFSET_DEFAULT, 0.06F);
    pose.foot_r = QVector3D(0.14F, HP::FOOT_Y_OFFSET_DEFAULT, -0.06F);
    pose.foot_y_offset = HP::FOOT_Y_OFFSET_DEFAULT;

    anim_ctx = HumanoidAnimationContext{};
    anim_ctx.variation = VariationParams::from_seed(12345);
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

TEST_F(PoseControllerCompatibilityTest, ElbowIKMatchesLegacyFunction) {

  QVector3D const shoulder(0.21F, 1.45F, 0.0F);
  QVector3D const hand(0.35F, 1.15F, 0.75F);
  QVector3D const outward_dir(1.0F, 0.0F, 0.0F);
  float const along_frac = 0.48F;
  float const lateral_offset = 0.12F;
  float const y_bias = 0.02F;
  float const outward_sign = 1.0F;

  QVector3D const legacy_elbow =
      elbow_bend_torso(shoulder, hand, outward_dir, along_frac, lateral_offset,
                       y_bias, outward_sign);

  HumanoidPoseController controller(pose, anim_ctx);
  QVector3D const controller_elbow =
      controller.solve_elbow_ik(false, shoulder, hand, outward_dir, along_frac,
                                lateral_offset, y_bias, outward_sign);

  EXPECT_TRUE(approxEqual(legacy_elbow, controller_elbow, 0.001F))
      << "Legacy: " << legacy_elbow.x() << ", " << legacy_elbow.y() << ", "
      << legacy_elbow.z() << "\n"
      << "Controller: " << controller_elbow.x() << ", " << controller_elbow.y()
      << ", " << controller_elbow.z();
}

TEST_F(PoseControllerCompatibilityTest, PlaceHandAtUsesCorrectElbowIK) {

  HumanoidPose legacy_pose = pose;

  QVector3D const target_hand(0.30F, 1.20F, 0.80F);

  legacy_pose.hand_r = target_hand;
  QVector3D right_axis = legacy_pose.shoulder_r - legacy_pose.shoulder_l;
  right_axis.setY(0.0F);
  right_axis.normalize();
  QVector3D const outward_r = right_axis;
  legacy_pose.elbow_r = elbow_bend_torso(legacy_pose.shoulder_r, target_hand,
                                         outward_r, 0.48F, 0.12F, 0.02F, 1.0F);

  HumanoidPoseController controller(pose, anim_ctx);
  controller.place_hand_at(false, target_hand);

  EXPECT_TRUE(approxEqual(pose.hand_r, target_hand, 0.001F));

  EXPECT_TRUE(approxEqual(pose.elbow_r, legacy_pose.elbow_r, 0.05F))
      << "Legacy elbow: " << legacy_pose.elbow_r.x() << ", "
      << legacy_pose.elbow_r.y() << ", " << legacy_pose.elbow_r.z() << "\n"
      << "Controller elbow: " << pose.elbow_r.x() << ", " << pose.elbow_r.y()
      << ", " << pose.elbow_r.z();
}

TEST_F(PoseControllerCompatibilityTest, KneeIKHandlesExtremeCases) {

  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const hip1(0.0F, 0.50F, 0.0F);
  QVector3D const foot1(0.05F, 0.45F, 0.05F);
  QVector3D const knee1 = controller.solve_knee_ik(true, hip1, foot1, 1.0F);
  EXPECT_GE(knee1.y(), HumanProportions::GROUND_Y);
  EXPECT_LE(knee1.y(), hip1.y());

  QVector3D const hip2(0.0F, 1.00F, 0.0F);
  QVector3D const foot2(0.80F, 0.0F, 0.80F);
  QVector3D const knee2 = controller.solve_knee_ik(false, hip2, foot2, 1.0F);
  EXPECT_GE(knee2.y(), HumanProportions::GROUND_Y);
  EXPECT_LE(knee2.y(), hip2.y());
}

TEST_F(PoseControllerCompatibilityTest,
       KneelProducesSimilarPoseToExistingCode) {

  using HP = HumanProportions;

  HumanoidPose reference_pose = pose;
  float const kneel_depth = 0.45F;
  float const pelvis_y = HP::WAIST_Y - kneel_depth;
  reference_pose.pelvis_pos.setY(pelvis_y);
  reference_pose.shoulder_l.setY(HP::SHOULDER_Y - kneel_depth);
  reference_pose.shoulder_r.setY(HP::SHOULDER_Y - kneel_depth);
  reference_pose.neck_base.setY(HP::NECK_BASE_Y - kneel_depth);
  reference_pose.head_pos.setY(HP::HEAD_CENTER_Y - kneel_depth);

  HumanoidPoseController controller(pose, anim_ctx);
  controller.kneel(1.0F);

  EXPECT_NEAR(pose.pelvis_pos.y(), reference_pose.pelvis_pos.y(), 0.10F);
  EXPECT_LT(pose.shoulder_l.y(), HP::SHOULDER_Y);
  EXPECT_LT(pose.shoulder_r.y(), HP::SHOULDER_Y);
}

TEST_F(PoseControllerCompatibilityTest,
       LeanProducesReasonableUpperBodyDisplacement) {

  using HP = HumanProportions;

  QVector3D const original_shoulder_l = pose.shoulder_l;
  QVector3D const original_shoulder_r = pose.shoulder_r;
  QVector3D const original_head = pose.head_pos;

  QVector3D const lean_dir(0.0F, 0.0F, 1.0F);
  float const lean_amount = 0.8F;

  HumanoidPoseController controller(pose, anim_ctx);
  controller.lean(lean_dir, lean_amount);

  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l.z());
  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r.z());

  EXPECT_GT(pose.head_pos.z(), original_head.z());
  float const shoulder_displacement =
      pose.shoulder_l.z() - original_shoulder_l.z();
  float const head_displacement = pose.head_pos.z() - original_head.z();
  EXPECT_LT(head_displacement, shoulder_displacement);

  float const expected_magnitude = 0.12F * lean_amount;
  EXPECT_NEAR(shoulder_displacement, expected_magnitude, 0.02F);
}

TEST_F(PoseControllerCompatibilityTest, CanRecreateBowAimingPose) {

  using HP = HumanProportions;

  HumanoidPoseController controller(pose, anim_ctx);

  controller.kneel(1.0F);
  controller.lean(QVector3D(0.0F, 0.0F, 1.0F), 0.2F);

  float const lowered_shoulder_y = pose.shoulder_l.y();
  controller.place_hand_at(
      true, QVector3D(-0.15F, lowered_shoulder_y + 0.30F, 0.55F));
  controller.place_hand_at(
      false, QVector3D(0.12F, pose.shoulder_r.y() + 0.15F, 0.10F));

  EXPECT_LT(pose.pelvis_pos.y(), HP::WAIST_Y);
  EXPECT_GT(pose.hand_l.y(), pose.shoulder_l.y());
  EXPECT_GT(pose.hand_l.z(), 0.0F);
  EXPECT_LT(pose.hand_r.z(), pose.hand_l.z());
}

TEST_F(PoseControllerCompatibilityTest, CanRecreateMeleeAttackPose) {

  using HP = HumanProportions;

  HumanoidPoseController controller(pose, anim_ctx);

  controller.lean(QVector3D(0.0F, 0.0F, 1.0F), 0.5F);

  QVector3D const thrust_hand(0.32F, HP::SHOULDER_Y + 0.10F, 0.90F);
  controller.place_hand_at(false, thrust_hand);

  controller.place_hand_at(true,
                           QVector3D(-0.05F, HP::SHOULDER_Y + 0.03F, 0.53F));

  EXPECT_GT(pose.hand_r.z(), 0.80F);
  EXPECT_GT(pose.shoulder_l.z(), 0.0F);
  EXPECT_GT(pose.elbow_r.z(), pose.shoulder_r.z());
}
