#include "render/horse/rig.h"
#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/mounted_pose_controller.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class MountedPoseControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    using HP = HumanProportions;

    // Initialize a default pose with basic standing configuration
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

    // Initialize animation context with default idle state
    anim_ctx = HumanoidAnimationContext{};
    anim_ctx.inputs.time = 0.0F;
    anim_ctx.inputs.is_moving = false;
    anim_ctx.inputs.is_attacking = false;
    anim_ctx.variation = VariationParams::fromSeed(12345);
    anim_ctx.gait.state = HumanoidMotionState::Idle;

    // Initialize a typical horse mount frame
    mount = MountedAttachmentFrame{};
    mount.saddle_center = QVector3D(0.0F, 1.20F, 0.0F);
    mount.seat_position = QVector3D(0.0F, 1.25F, 0.0F);
    mount.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
    mount.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
    mount.seat_up = QVector3D(0.0F, 1.0F, 0.0F);
    mount.ground_offset = QVector3D(0.0F, 0.0F, 0.0F);

    mount.stirrup_attach_left = QVector3D(-0.35F, 1.05F, 0.15F);
    mount.stirrup_attach_right = QVector3D(0.35F, 1.05F, 0.15F);
    mount.stirrup_bottom_left = QVector3D(-0.40F, 0.75F, 0.20F);
    mount.stirrup_bottom_right = QVector3D(0.40F, 0.75F, 0.20F);

    mount.rein_bit_left = QVector3D(-0.12F, 1.48F, 0.95F);
    mount.rein_bit_right = QVector3D(0.12F, 1.48F, 0.95F);
    mount.bridle_base = QVector3D(0.0F, 1.50F, 0.85F);
  }

  HumanoidPose pose;
  HumanoidAnimationContext anim_ctx;
  MountedAttachmentFrame mount;

  // Helper to check if a position is approximately equal
  bool approxEqual(const QVector3D &a, const QVector3D &b,
                   float epsilon = 0.01F) {
    return std::abs(a.x() - b.x()) < epsilon &&
           std::abs(a.y() - b.y()) < epsilon &&
           std::abs(a.z() - b.z()) < epsilon;
  }
};

TEST_F(MountedPoseControllerTest, ConstructorInitializesCorrectly) {
  MountedPoseController controller(pose, anim_ctx);
  // Constructor should not modify the pose
  EXPECT_FLOAT_EQ(pose.pelvis_pos.y(), HumanProportions::WAIST_Y);
}

TEST_F(MountedPoseControllerTest, MountOnHorsePositionsPelvisOnSaddle) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);

  // Pelvis should be at seat position
  EXPECT_TRUE(approxEqual(pose.pelvis_pos, mount.seat_position));
}

TEST_F(MountedPoseControllerTest, MountOnHorsePlacesFeetInStirrups) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);

  // Feet should be in stirrups
  EXPECT_TRUE(approxEqual(pose.foot_l, mount.stirrup_bottom_left));
  EXPECT_TRUE(approxEqual(pose.foot_r, mount.stirrup_bottom_right));
}

TEST_F(MountedPoseControllerTest, MountOnHorseLiftsUpperBody) {
  MountedPoseController controller(pose, anim_ctx);

  float const original_shoulder_y = pose.shoulder_l.y();

  controller.mount_on_horse(mount);

  // Shoulders should be lifted when mounted
  EXPECT_GT(pose.shoulder_l.y(), original_shoulder_y);
  EXPECT_GT(pose.shoulder_r.y(), original_shoulder_y);
}

TEST_F(MountedPoseControllerTest, DismountRestoresStandingPosition) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);
  controller.dismount();

  // Pelvis should be back at standing height
  EXPECT_NEAR(pose.pelvis_pos.y(), HumanProportions::WAIST_Y, 0.01F);
}

TEST_F(MountedPoseControllerTest, RidingIdleSetsHandsToRestPosition) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);

  // Hands should be in a resting position near pelvis
  EXPECT_LT(pose.hand_l.y(), mount.seat_position.y());
  EXPECT_LT(pose.hand_r.y(), mount.seat_position.y());
}

TEST_F(MountedPoseControllerTest, RidingLeaningForwardMovesTorso) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);
  QVector3D const original_shoulder_z = pose.shoulder_l;

  controller.riding_leaning(mount, 1.0F, 0.0F); // Full forward lean

  // Shoulders should move forward
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_z.z());
  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_z.z());
}

TEST_F(MountedPoseControllerTest, RidingLeaningSidewaysMovesTorso) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);
  QVector3D const original_shoulder_x = pose.shoulder_l;

  controller.riding_leaning(mount, 0.0F, 1.0F); // Full right lean

  // Shoulders should move to the right
  EXPECT_GT(pose.shoulder_r.x(), original_shoulder_x.x());
}

TEST_F(MountedPoseControllerTest, RidingLeaningClampsInputs) {
  MountedPoseController controller(pose, anim_ctx);

  // Should not crash with out-of-range inputs
  EXPECT_NO_THROW(controller.riding_leaning(mount, 2.0F, -2.0F));
}

TEST_F(MountedPoseControllerTest, RidingChargingLeansForward) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);
  QVector3D const original_shoulder = pose.shoulder_l;

  controller.riding_charging(mount, 1.0F);

  // Should lean forward when charging
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder.z());
  EXPECT_LT(pose.shoulder_l.y(), original_shoulder.y()); // Crouch
}

TEST_F(MountedPoseControllerTest, RidingReiningPullsHandsBack) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);
  float const idle_left_z = pose.hand_l.z();
  float const idle_right_z = pose.hand_r.z();

  controller.riding_reining(mount, 1.0F, 1.0F);

  // Hands should be pulled back when reining
  EXPECT_LT(pose.hand_l.z(), idle_left_z);
  EXPECT_LT(pose.hand_r.z(), idle_right_z);
}

TEST_F(MountedPoseControllerTest, RidingReiningLeansTorsoBack) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);
  QVector3D const original_shoulder = pose.shoulder_l;

  controller.riding_reining(mount, 1.0F, 1.0F);

  // Should lean back when reining hard
  EXPECT_LT(pose.shoulder_l.z(), original_shoulder.z());
}

TEST_F(MountedPoseControllerTest, RidingMeleeStrikeAnimatesCorrectly) {
  MountedPoseController controller(pose, anim_ctx);

  // Test chamber phase (raising weapon)
  controller.riding_melee_strike(mount, 0.15F);
  float const chamber_y = pose.hand_r.y();

  // Test apex phase (weapon at highest point)
  controller.riding_melee_strike(mount, 0.25F);
  float const apex_y = pose.hand_r.y();

  // Hand should be higher at apex than during chamber
  EXPECT_GT(apex_y, chamber_y);
}

TEST_F(MountedPoseControllerTest, RidingSpearThrustAnimatesCorrectly) {
  MountedPoseController controller(pose, anim_ctx);

  // Test couch phase (spear pulled back)
  controller.riding_spear_thrust(mount, 0.25F);
  float const couch_z = pose.hand_r.z();

  // Test mid-thrust phase (spear extending forward)
  controller.riding_spear_thrust(mount, 0.45F);
  float const thrust_z = pose.hand_r.z();

  // Hand should move forward during thrust
  EXPECT_GT(thrust_z, couch_z);
}

TEST_F(MountedPoseControllerTest, RidingBowShotAnimatesCorrectly) {
  MountedPoseController controller(pose, anim_ctx);

  // Test initial draw
  controller.riding_bow_shot(mount, 0.10F);
  QVector3D const draw_start = pose.hand_r;

  // Test full draw
  controller.riding_bow_shot(mount, 0.40F);
  QVector3D const draw_end = pose.hand_r;

  // Right hand should move back when drawing
  float const dist_moved = (draw_end - draw_start).length();
  EXPECT_GT(dist_moved, 0.05F);
}

TEST_F(MountedPoseControllerTest, RidingShieldDefenseRaisesHand) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_shield_defense(mount, false);
  float const lowered_y = pose.hand_l.y();

  controller.riding_shield_defense(mount, true);
  float const raised_y = pose.hand_l.y();

  // Shield should be higher when raised
  EXPECT_GT(raised_y, lowered_y);
}

TEST_F(MountedPoseControllerTest, HoldReinsPositionsHandsCorrectly) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);
  controller.hold_reins(mount, 0.5F, 0.5F, 0.3F, 0.3F);

  // Hands should stay near the saddle area with a slight forward bias
  EXPECT_LT(std::abs(pose.hand_l.x()), mount.seat_position.x() + 0.30F);
  EXPECT_LT(std::abs(pose.hand_r.x()), mount.seat_position.x() + 0.30F);
  EXPECT_LT(pose.hand_l.y(), mount.seat_position.y());
  EXPECT_LT(pose.hand_r.y(), mount.seat_position.y());
}

TEST_F(MountedPoseControllerTest, HoldReinsSlackAffectsHandPosition) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);
  controller.hold_reins(mount, 0.0F, 0.0F, 1.0F, 1.0F);
  QVector3D const tight_left = pose.hand_l;

  controller.hold_reins(mount, 1.0F, 1.0F, 0.0F, 0.0F);
  QVector3D const slack_left = pose.hand_l;

  // Slack reins should lower hands
  EXPECT_LT(slack_left.y(), tight_left.y());
}

TEST_F(MountedPoseControllerTest, HoldSpearOverhandRaisesHand) {
  MountedPoseController controller(pose, anim_ctx);

  controller.hold_spear_mounted(mount, SpearGrip::OVERHAND);

  // Right hand should be high for overhead grip
  EXPECT_GT(pose.hand_r.y(), mount.seat_position.y() + 0.40F);
}

TEST_F(MountedPoseControllerTest, HoldSpearCouchedLowersHand) {
  MountedPoseController controller(pose, anim_ctx);

  controller.hold_spear_mounted(mount, SpearGrip::COUCHED);

  // Right hand should be low for couched grip
  EXPECT_LT(pose.hand_r.y(), mount.seat_position.y() + 0.20F);
}

TEST_F(MountedPoseControllerTest, HoldSpearTwoHandedUsesBothHands) {
  MountedPoseController controller(pose, anim_ctx);

  controller.hold_spear_mounted(mount, SpearGrip::TWO_HANDED);

  // Both hands should be on spear shaft
  float const hand_separation = (pose.hand_r - pose.hand_l).length();
  EXPECT_GT(hand_separation, 0.15F);
  EXPECT_LT(hand_separation, 0.35F);
}

TEST_F(MountedPoseControllerTest, HoldBowMountedPositionsHandsCorrectly) {
  MountedPoseController controller(pose, anim_ctx);

  controller.hold_bow_mounted(mount);

  // Left hand should hold bow forward
  EXPECT_GT(pose.hand_l.z(), mount.seat_position.z());

  // Right hand should be near bow for arrow nocking
  float const hand_separation = (pose.hand_r - pose.hand_l).length();
  EXPECT_LT(hand_separation, 0.25F);
}

TEST_F(MountedPoseControllerTest, KneePositionValidForMountedRiding) {
  MountedPoseController controller(pose, anim_ctx);

  controller.mount_on_horse(mount);

  // Knees should be between pelvis and feet
  EXPECT_LT(pose.knee_l.y(), pose.pelvis_pos.y());
  EXPECT_GT(pose.knee_l.y(), pose.foot_l.y());

  EXPECT_LT(pose.knee_r.y(), pose.pelvis_pos.y());
  EXPECT_GT(pose.knee_r.y(), pose.foot_r.y());
}

TEST_F(MountedPoseControllerTest, ElbowPositionValidForAllActions) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_idle(mount);

  // Elbows should be between shoulders and hands
  float const left_shoulder_elbow = (pose.elbow_l - pose.shoulder_l).length();
  float const left_elbow_hand = (pose.hand_l - pose.elbow_l).length();

  EXPECT_GT(left_shoulder_elbow, 0.05F);
  EXPECT_GT(left_elbow_hand, 0.05F);
  EXPECT_LT(left_shoulder_elbow, 0.50F);
  EXPECT_LT(left_elbow_hand, 0.50F);
}

TEST_F(MountedPoseControllerTest, AllMethodsHandleEdgeCases) {
  MountedPoseController controller(pose, anim_ctx);

  // Should not crash with various inputs
  EXPECT_NO_THROW(controller.mount_on_horse(mount));
  EXPECT_NO_THROW(controller.dismount());
  EXPECT_NO_THROW(controller.riding_idle(mount));
  EXPECT_NO_THROW(controller.riding_leaning(mount, 0.0F, 0.0F));
  EXPECT_NO_THROW(controller.riding_charging(mount, 0.0F));
  EXPECT_NO_THROW(controller.riding_reining(mount, 0.0F, 0.0F));
  EXPECT_NO_THROW(controller.riding_melee_strike(mount, 0.5F));
  EXPECT_NO_THROW(controller.riding_spear_thrust(mount, 0.5F));
  EXPECT_NO_THROW(controller.riding_bow_shot(mount, 0.5F));
  EXPECT_NO_THROW(controller.riding_shield_defense(mount, true));
  EXPECT_NO_THROW(controller.hold_reins(mount, 0.5F, 0.5F, 0.4F, 0.4F));
  EXPECT_NO_THROW(controller.hold_spear_mounted(mount, SpearGrip::OVERHAND));
  EXPECT_NO_THROW(controller.hold_bow_mounted(mount));
}

TEST_F(MountedPoseControllerTest, AttackPhaseClamping) {
  MountedPoseController controller(pose, anim_ctx);

  // Test clamping of attack phase > 1.0
  EXPECT_NO_THROW(controller.riding_melee_strike(mount, 1.5F));
  EXPECT_NO_THROW(controller.riding_spear_thrust(mount, 2.0F));
  EXPECT_NO_THROW(controller.riding_bow_shot(mount, -0.5F));
}

TEST_F(MountedPoseControllerTest, RidingChargingIntensityClamping) {
  MountedPoseController controller(pose, anim_ctx);

  controller.riding_charging(mount, 1.5F);
  QVector3D const max_lean = pose.shoulder_l;

  // Reset
  SetUp();
  MountedPoseController controller2(pose, anim_ctx);
  controller2.riding_charging(mount, 1.0F);

  // Should be same as clamped 1.5F
  EXPECT_TRUE(approxEqual(pose.shoulder_l, max_lean));
}

TEST_F(MountedPoseControllerTest, FullRidingSequence) {
  MountedPoseController controller(pose, anim_ctx);

  // Simulate a full riding sequence
  controller.mount_on_horse(mount);
  EXPECT_TRUE(approxEqual(pose.pelvis_pos, mount.seat_position));

  controller.riding_idle(mount);
  QVector3D const idle_hands = pose.hand_l;

  controller.hold_reins(mount, 0.5F, 0.5F, 0.3F, 0.3F);
  controller.riding_charging(mount, 1.0F);

  controller.riding_spear_thrust(mount, 0.35F);
  // Verify animation in progress
  EXPECT_GT(pose.hand_r.z(), mount.seat_position.z());

  controller.riding_idle(mount);
  controller.dismount();

  // Should be back near standing position
  EXPECT_NEAR(pose.pelvis_pos.y(), HumanProportions::WAIST_Y, 0.01F);
}
