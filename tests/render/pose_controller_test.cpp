#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/spear_pose_utils.h"
#include <QVector3D>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <unordered_set>

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

  bool approx_equal(const QVector3D &a, const QVector3D &b,
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

  EXPECT_TRUE(approx_equal(pose.pelvis_pos, original_pelvis));
  EXPECT_TRUE(approx_equal(pose.shoulder_l, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest,
       AmbientIdleSelectionDependsOnContinuousIdleTimeNotWorldTime) {
  std::uint32_t const seed = 1337U;
  float activation_time = -1.0F;
  for (float idle_duration = 5.0F; idle_duration <= 80.0F;
       idle_duration += 0.01F) {
    if (HumanoidPoseController::get_ambient_idle_type(
            10.0F, seed, idle_duration) != AmbientIdleType::None) {
      activation_time = idle_duration;
      break;
    }
  }

  ASSERT_GT(activation_time, 0.0F);
  auto const idle_type_now = HumanoidPoseController::get_ambient_idle_type(
      10.0F, seed, activation_time);
  auto const idle_type_later = HumanoidPoseController::get_ambient_idle_type(
      1000.0F, seed, activation_time);
  EXPECT_EQ(idle_type_now, idle_type_later);
  EXPECT_NE(idle_type_now, AmbientIdleType::None);

  float const start_phase =
      HumanoidPoseController::compute_ambient_idle_phase(activation_time, seed);
  float const progressed_phase =
      HumanoidPoseController::compute_ambient_idle_phase(activation_time + 1.0F,
                                                         seed);
  EXPECT_LT(start_phase, 0.05F);
  EXPECT_GT(progressed_phase, start_phase);
}

TEST_F(HumanoidPoseControllerTest,
       AmbientIdleSelectionVariesAcrossFormationSeeds) {
  constexpr std::uint32_t base_seed = 1234U;
  constexpr int soldier_count = 64;
  constexpr std::array<float, 4> sample_idle_durations{12.0F, 14.0F, 16.0F,
                                                       40.0F};

  std::unordered_set<int> selected_types;
  int peak_active = 0;
  for (float idle_duration : sample_idle_durations) {
    int active = 0;
    for (int idx = 0; idx < soldier_count; ++idx) {
      std::uint32_t const seed =
          base_seed ^ static_cast<std::uint32_t>(idx * 9176);
      auto const type = HumanoidPoseController::get_ambient_idle_type(
          10.0F, seed, idle_duration);
      if (type == AmbientIdleType::None) {
        continue;
      }
      ++active;
      selected_types.insert(static_cast<int>(type));
    }
    peak_active = std::max(peak_active, active);
  }

  EXPECT_GE(selected_types.size(), 4U);
  EXPECT_LT(peak_active, soldier_count / 3);
}

TEST_F(HumanoidPoseControllerTest, AirborneJumpAmbientIdleLiftsFeetAndPelvis) {
  HumanoidPoseController controller(pose, anim_ctx);
  anim_ctx.gait.is_airborne = true;

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_knee_l = pose.knee_l;
  QVector3D const original_knee_r = pose.knee_r;
  QVector3D const original_foot_l = pose.foot_l;
  QVector3D const original_foot_r = pose.foot_r;

  controller.apply_ambient_idle_explicit(AmbientIdleType::Jump, 0.5F);

  EXPECT_GT(pose.pelvis_pos.y(), original_pelvis.y() + 0.08F);
  EXPECT_GT(pose.foot_l.y(), original_foot_l.y() + 0.05F);
  EXPECT_GT(pose.foot_r.y(), original_foot_r.y() + 0.05F);
  EXPECT_GT(pose.knee_l.z(), original_knee_l.z() + 0.04F);
  EXPECT_GT(pose.knee_r.z(), original_knee_r.z() + 0.04F);
}

TEST(HumanoidAnimationInputs, IdleDurationTracksUninterruptedIdleTime) {
  Engine::Core::Entity entity(1);
  auto *movement = entity.add_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 0.0F);

  ctx.animation_time = 3.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 2.0F);

  movement->has_target = true;
  ctx.animation_time = 4.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 0.0F);

  movement->has_target = false;
  ctx.animation_time = 5.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 1.0F);

  ctx.animation_time = 7.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 3.0F);
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

  EXPECT_TRUE(approx_equal(pose.shoulder_l, original_shoulder_l));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtSetsHandPosition) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);

  controller.place_hand_at(false, target_position);

  EXPECT_TRUE(approx_equal(pose.hand_r, target_position));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtComputesElbow) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);
  QVector3D const original_elbow = pose.elbow_r;

  controller.place_hand_at(false, target_position);

  EXPECT_FALSE(approx_equal(pose.elbow_r, original_elbow));

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

  EXPECT_TRUE(approx_equal(pose.hand_l, target_position));

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

TEST_F(HumanoidPoseControllerTest,
       BraceSpearForHoldPositionsHandsForwardAndLow) {
  anim_ctx.inputs.is_in_hold_mode = true;
  anim_ctx.inputs.hold_entry_progress = 1.0F;
  HumanoidPoseController controller(pose, anim_ctx);

  controller.brace_spear_for_hold();

  EXPECT_GT(pose.hand_r.z(), 0.48F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y);
  EXPECT_GT(pose.hand_l.z(), 0.20F);
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y);
  EXPECT_GT((pose.hand_r - pose.hand_l).length(), 0.18F);
}

TEST_F(HumanoidPoseControllerTest, HoldBowReadyKeepsHandsInLowerReadyPose) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.hold_bow_ready();

  EXPECT_GT(pose.hand_r.x(), 0.03F);
  EXPECT_GT(pose.hand_l.x(), -0.02F);
  EXPECT_GT(pose.hand_r.z(), 0.28F);
  EXPECT_GT(pose.hand_l.z(), 0.52F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y);
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y + 0.04F);
  EXPECT_GT((pose.hand_r - pose.hand_l).length(), 0.18F);
}

TEST_F(HumanoidPoseControllerTest,
       BraceSwordAndShieldForHoldRaisesShieldComparedToMarchHold) {
  HumanoidPose march_pose = pose;
  HumanoidPoseController march_controller(march_pose, anim_ctx);
  march_controller.hold_sword_and_shield();

  HumanoidPose brace_pose = pose;
  HumanoidPoseController brace_controller(brace_pose, anim_ctx);
  brace_controller.brace_sword_and_shield_for_hold();

  EXPECT_GT(brace_pose.hand_l.y(), march_pose.hand_l.y());
  EXPECT_GT(brace_pose.hand_l.z(), march_pose.hand_l.z());
  EXPECT_LT(brace_pose.hand_r.y(), march_pose.hand_r.y());
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

  EXPECT_TRUE(approx_equal(pose.head_pos, original_head_pos));
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

TEST_F(HumanoidPoseControllerTest, KneelWithDepthMultiplierLowersTorso) {
  using HP = HumanProportions;

  HumanoidPose spear_pose = pose;
  HumanoidPoseController spear_ctrl(spear_pose, anim_ctx);
  spear_ctrl.kneel(0.875F);

  HumanoidPose archer_pose = pose;
  HumanoidPoseController archer_ctrl(archer_pose, anim_ctx);
  archer_ctrl.kneel(1.125F);

  EXPECT_LT(spear_pose.pelvis_pos.y(), HP::WAIST_Y);
  EXPECT_LT(archer_pose.pelvis_pos.y(), HP::WAIST_Y);
  EXPECT_GT(spear_pose.pelvis_pos.y(), archer_pose.pelvis_pos.y())
      << "Spearman should kneel less than archer";

  EXPECT_LT(spear_pose.shoulder_l.y(), HP::SHOULDER_Y);
  EXPECT_LT(archer_pose.shoulder_l.y(), HP::SHOULDER_Y);
}

TEST_F(HumanoidPoseControllerTest, KneelEntryProgressPartialKneel) {
  using HP = HumanProportions;

  float const full_kneel_depth = 1.0F;

  HumanoidPose full_pose = pose;
  HumanoidPoseController full_ctrl(full_pose, anim_ctx);
  full_ctrl.kneel(full_kneel_depth);

  HumanoidPose half_pose = pose;
  HumanoidPoseController half_ctrl(half_pose, anim_ctx);
  half_ctrl.kneel(0.5F * full_kneel_depth);

  HumanoidPose standing_pose = pose;
  float const standing_pelvis_y = standing_pose.pelvis_pos.y();

  EXPECT_LT(full_pose.pelvis_pos.y(), half_pose.pelvis_pos.y())
      << "Full kneel should be lower than half kneel";
  EXPECT_LT(half_pose.pelvis_pos.y(), standing_pelvis_y)
      << "Half kneel should be lower than standing";
}

TEST_F(HumanoidPoseControllerTest, KneelExitProgressReturnsTowardsStanding) {
  using HP = HumanProportions;

  float const kneel_depth = 1.0F;
  float const standing_pelvis_y = pose.pelvis_pos.y();

  HumanoidPose fully_kneeled_pose = pose;
  HumanoidPoseController kneeled_ctrl(fully_kneeled_pose, anim_ctx);
  kneeled_ctrl.kneel(kneel_depth);

  HumanoidPose half_exit_pose = pose;
  HumanoidPoseController half_exit_ctrl(half_exit_pose, anim_ctx);
  float const exit_progress_half = 0.5F;
  half_exit_ctrl.kneel((1.0F - exit_progress_half) * kneel_depth);

  HumanoidPose full_exit_pose = pose;
  HumanoidPoseController full_exit_ctrl(full_exit_pose, anim_ctx);
  full_exit_ctrl.kneel((1.0F - 1.0F) * kneel_depth);

  EXPECT_LT(fully_kneeled_pose.pelvis_pos.y(), half_exit_pose.pelvis_pos.y())
      << "Full kneel should be lower than half-way through exit";
  EXPECT_NEAR(full_exit_pose.pelvis_pos.y(), standing_pelvis_y, 0.001F)
      << "Completed exit should restore original standing height";
}

TEST_F(HumanoidPoseControllerTest, SpearDirectionBlendsDuringHoldEntry) {
  AnimationInputs standing_inputs{};
  QVector3D const standing_dir = compute_spear_direction(standing_inputs);

  AnimationInputs partial_hold_inputs{};
  partial_hold_inputs.is_in_hold_mode = true;
  partial_hold_inputs.hold_entry_progress = 0.5F;
  QVector3D const partial_hold_dir =
      compute_spear_direction(partial_hold_inputs);

  AnimationInputs full_hold_inputs{};
  full_hold_inputs.is_in_hold_mode = true;
  full_hold_inputs.hold_entry_progress = 1.0F;
  QVector3D const full_hold_dir = compute_spear_direction(full_hold_inputs);

  EXPECT_LT(partial_hold_dir.y(), standing_dir.y());
  EXPECT_GT(partial_hold_dir.y(), full_hold_dir.y());
  EXPECT_GT(partial_hold_dir.z(), standing_dir.z());
  EXPECT_LT(partial_hold_dir.z(), full_hold_dir.z());
}

TEST_F(HumanoidPoseControllerTest, SpearDirectionMatchesExitHoldDepth) {
  AnimationInputs entry_inputs{};
  entry_inputs.is_in_hold_mode = true;
  entry_inputs.hold_entry_progress = 0.75F;
  QVector3D const entry_dir = compute_spear_direction(entry_inputs);

  AnimationInputs exit_inputs{};
  exit_inputs.is_exiting_hold = true;
  exit_inputs.hold_exit_progress = 0.25F;
  QVector3D const exit_dir = compute_spear_direction(exit_inputs);

  EXPECT_TRUE(approx_equal(entry_dir, exit_dir, 0.001F));
}

TEST_F(HumanoidPoseControllerTest,
       KneelSwordsmanShallowestSpearmanMiddleArcherDeepest) {
  HumanoidPose spear_pose = pose;
  HumanoidPoseController spear_ctrl(spear_pose, anim_ctx);
  spear_ctrl.kneel(0.875F);

  HumanoidPose sword_pose = pose;
  HumanoidPoseController sword_ctrl(sword_pose, anim_ctx);
  sword_ctrl.kneel(0.825F);

  HumanoidPose archer_pose = pose;
  HumanoidPoseController archer_ctrl(archer_pose, anim_ctx);
  archer_ctrl.kneel(1.125F);

  EXPECT_GT(sword_pose.pelvis_pos.y(), archer_pose.pelvis_pos.y())
      << "Swordsman (0.825) should kneel less deeply than archer (1.125)";
  EXPECT_GT(sword_pose.pelvis_pos.y(), spear_pose.pelvis_pos.y())
      << "Swordsman (0.825) should kneel less deeply than spearman (0.875)";
  EXPECT_GT(spear_pose.pelvis_pos.y(), archer_pose.pelvis_pos.y())
      << "Spearman (0.875) should kneel less deeply than archer (1.125)";
}
