#include <QVector3D>

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <unordered_set>

#include "animation/ambient_pose_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/spear_pose_utils.h"
#include "tests/support/movement_test_access.h"

using namespace Render::GL;

namespace {

constexpr float k_step = 1.0F / 60.0F;

struct AmbientSim {
  Animation::HumanoidAmbientRuntimeState state{};
  float time{0.0F};
  float idle_duration{0.0F};
  std::uint32_t seed{0U};
  bool mounted{false};

  auto step(float dt, bool eligible) -> Animation::HumanoidAmbientScheduleSample {
    time += dt;
    idle_duration = eligible ? idle_duration + dt : 0.0F;
    Animation::HumanoidAmbientSelectionInputs inputs{};
    inputs.ambient_state = state;
    inputs.sample_time = time;
    inputs.mounted = mounted;
    inputs.seed = seed;
    inputs.has_locomotion = !eligible;
    inputs.idle_duration = idle_duration;
    auto const tick = Animation::resolve_humanoid_ambient_selection(inputs);
    state = tick.state;
    return tick.sample;
  }
};

} // namespace

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
    anim_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Idle;
    anim_ctx.inputs.is_attacking = false;
    anim_ctx.variation = VariationParams::from_seed(12345);
    anim_ctx.gait.state = HumanoidMotionState::Idle;
  }

  HumanoidPose pose;
  HumanoidAnimationContext anim_ctx;

  bool approx_equal(const QVector3D& a, const QVector3D& b, float epsilon = 0.01F) {
    return std::abs(a.x() - b.x()) < epsilon && std::abs(a.y() - b.y()) < epsilon &&
           std::abs(a.z() - b.z()) < epsilon;
  }
};

TEST_F(HumanoidPoseControllerTest, ConstructorInitializesCorrectly) {
  HumanoidPoseController const controller(pose, anim_ctx);

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

TEST_F(HumanoidPoseControllerTest, AmbientIdleNeverStartsBeforeMinimumIdleDuration) {
  AmbientSim sim{.seed = 4242U};
  for (float t = 0.0F; t < Animation::k_ambient_min_idle_duration - k_step;
       t += k_step) {
    EXPECT_FALSE(sim.step(k_step, true).active)
        << "ambient fired after only " << sim.idle_duration << "s of standing";
  }
}

TEST_F(HumanoidPoseControllerTest, AmbientIdleEasesInAndOutWithoutSnapping) {
  AmbientSim sim{.seed = 4242U};
  float previous_blend = 0.0F;
  float peak_blend = 0.0F;
  bool saw_active = false;
  bool returned_to_rest = false;

  float const max_delta = k_step / std::min(Animation::k_ambient_blend_in_duration,
                                            Animation::k_ambient_blend_out_duration) +
                          1.0e-4F;

  for (float t = 0.0F; t < 40.0F; t += k_step) {
    auto const sample = sim.step(k_step, true);
    float const blend = sample.active ? sample.blend : 0.0F;
    EXPECT_LE(std::abs(blend - previous_blend), max_delta)
        << "crossfade jumped from " << previous_blend << " to " << blend;
    if (sample.active) {
      saw_active = true;
      peak_blend = std::max(peak_blend, blend);
    } else if (saw_active) {
      returned_to_rest = true;
    }
    previous_blend = blend;
  }

  EXPECT_TRUE(saw_active) << "no ambient idle played in 40s of standing";
  EXPECT_NEAR(peak_blend, 1.0F, 1.0e-3F) << "ambient never reached full weight";
  EXPECT_TRUE(returned_to_rest) << "ambient never settled back into the idle loop";
}

TEST_F(HumanoidPoseControllerTest, AmbientIdleClipPhaseAdvancesMonotonically) {
  AmbientSim sim{.seed = 77U};
  float previous_phase = 0.0F;
  bool saw_active = false;

  for (float t = 0.0F; t < 40.0F; t += k_step) {
    auto const sample = sim.step(k_step, true);
    if (!sample.active) {
      previous_phase = 0.0F;
      continue;
    }
    if (saw_active) {
      EXPECT_GE(sample.phase, previous_phase) << "ambient clip phase ran backwards";
    }
    saw_active = true;
    previous_phase = sample.phase;
  }
  EXPECT_TRUE(saw_active);
}

TEST_F(HumanoidPoseControllerTest, AmbientIdleInterruptionBlendsOutInsteadOfCutting) {
  AmbientSim sim{.seed = 4242U};

  bool reached_hold = false;
  for (float t = 0.0F; t < 40.0F && !reached_hold; t += k_step) {
    auto const sample = sim.step(k_step, true);
    reached_hold = sample.active && sample.blend >= 0.999F;
  }
  ASSERT_TRUE(reached_hold) << "ambient never reached full weight";

  int fade_frames = 0;
  float previous_blend = 1.0F;
  for (int i = 0; i < 240; ++i) {
    auto const sample = sim.step(k_step, false);
    float const blend = sample.active ? sample.blend : 0.0F;
    EXPECT_LE(blend, previous_blend + 1.0e-4F) << "interrupted ambient re-strengthened";
    if (!sample.active) {
      break;
    }
    ++fade_frames;
    previous_blend = blend;
  }

  EXPECT_GT(fade_frames, 10) << "interrupted ambient snapped instead of blending out";
}

TEST_F(HumanoidPoseControllerTest, AmbientIdleRotatesThroughEveryVariant) {
  AmbientSim sim{.seed = 909U};
  std::vector<AmbientIdleType> plays;
  AmbientIdleType current = AmbientIdleType::None;

  for (float t = 0.0F; t < 400.0F; t += k_step) {
    auto const sample = sim.step(k_step, true);
    auto const type = sample.active ? sample.type : AmbientIdleType::None;
    if (type != AmbientIdleType::None && type != current) {
      plays.push_back(type);
    }
    current = type;
  }

  constexpr std::size_t k_infantry_ambient_variant_count = 4U;
  ASSERT_GE(plays.size(), k_infantry_ambient_variant_count * 2U)
      << "ambient idles did not complete two rotations over 400s";

  std::unordered_set<int> first_rotation;
  for (std::size_t i = 0; i < k_infantry_ambient_variant_count; ++i) {
    first_rotation.insert(static_cast<int>(plays[i]));
  }
  EXPECT_EQ(first_rotation.size(), k_infantry_ambient_variant_count);

  for (std::size_t i = 0; i < plays.size(); ++i) {
    if (i > 0) {
      EXPECT_NE(plays[i], plays[i - 1]) << "same ambient idle played back to back";
    }
    EXPECT_EQ(plays[i], plays[i % k_infantry_ambient_variant_count])
        << "ambient idle sequence stopped rotating at play " << i;
  }
}

TEST_F(HumanoidPoseControllerTest, AmbientIdleKeepsFormationParticipationSparse) {
  constexpr std::uint32_t base_seed = 1234U;
  constexpr int soldier_count = 64;

  std::vector<AmbientSim> squad;
  squad.reserve(soldier_count);
  for (int idx = 0; idx < soldier_count; ++idx) {
    squad.push_back(
        AmbientSim{.seed = base_seed ^ static_cast<std::uint32_t>(idx * 9176)});
  }

  int peak_concurrent = 0;
  std::unordered_set<int> ever_active;
  for (float t = 0.0F; t < 240.0F; t += k_step) {
    int active = 0;
    for (int idx = 0; idx < soldier_count; ++idx) {
      if (squad[static_cast<std::size_t>(idx)].step(k_step, true).active) {
        ++active;
        ever_active.insert(idx);
      }
    }
    peak_concurrent = std::max(peak_concurrent, active);
  }

  EXPECT_LE(peak_concurrent, soldier_count / 2)
      << peak_concurrent << " of " << soldier_count << " soldiers animated at once";
  EXPECT_GT(ever_active.size(), static_cast<std::size_t>(soldier_count * 3 / 4));
}

TEST_F(HumanoidPoseControllerTest, MountedUnitsDoNotScheduleAmbientIdles) {
  for (std::uint32_t seed = 1U; seed <= 24U; ++seed) {
    AmbientSim sim{.seed = seed, .mounted = true};
    for (float t = 0.0F; t < 200.0F; t += k_step) {
      auto const sample = sim.step(k_step, true);
      ASSERT_FALSE(sample.active)
          << "a mounted rider scheduled an ambient idle it cannot render (seed " << seed
          << ")";
      ASSERT_NE(sample.type, AmbientIdleType::SitDown)
          << "a mounted rider tried to squat on the ground (seed " << seed << ")";
    }
  }
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
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(motion, nullptr);

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;

  ctx.animation_time = 1.0F;
  auto anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 0.0F);

  ctx.animation_time = 3.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 2.0F);

  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  ctx.animation_time = 4.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 0.0F);

  motion->set_state(Engine::Core::MotionPresentationState::Idle);
  ctx.animation_time = 5.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 1.0F);

  ctx.animation_time = 7.0F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_FLOAT_EQ(anim.idle_duration, 3.0F);
}

TEST(HumanoidAnimationInputs, FpvCommanderGuardSetsGuardingWithoutHoldMode) {
  Engine::Core::Entity entity(1);
  auto* commander = entity.add_component<Engine::Core::CommanderComponent>();
  auto* guard = entity.add_component<Engine::Core::CommanderGuardComponent>();
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(guard, nullptr);
  commander->fpv_controlled = true;
  guard->active = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.animation_time = 1.0F;

  auto anim = Render::GL::sample_anim_state(ctx);

  EXPECT_TRUE(anim.is_guarding);
  EXPECT_FLOAT_EQ(anim.guard_pose_progress, 0.0F);
  EXPECT_FALSE(anim.is_in_hold_mode);

  ctx.animation_time = 1.75F;
  anim = Render::GL::sample_anim_state(ctx);
  EXPECT_TRUE(anim.is_guarding);
  EXPECT_NEAR(anim.guard_pose_progress, 5.0F / 12.0F, 1.0e-3F);
  EXPECT_FALSE(anim.is_in_hold_mode);
}

TEST(HumanoidAnimationInputs, FpvCommanderVelocityTriggersMovementAnimation) {
  Engine::Core::Entity entity(1);
  auto* commander = entity.add_component<Engine::Core::CommanderComponent>();
  auto* movement = entity.add_component<Engine::Core::MovementComponent>();
  auto* motion = entity.add_component<Engine::Core::MotionPresentationComponent>();
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(motion, nullptr);
  commander->fpv_controlled = true;
  MovementTestAccess::set_has_target(*movement, false);
  MovementTestAccess::set_vx(*movement, 1.4F);
  MovementTestAccess::set_vz(*movement, 0.0F);
  motion->set_state(Engine::Core::MotionPresentationState::Walk);
  motion->velocity_x = movement->get_vx();
  motion->has_velocity = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.animation_time = 1.0F;

  auto anim = Render::GL::sample_anim_state(ctx);

  EXPECT_TRUE(Render::Creature::is_moving_animation(anim.movement_state));
}

TEST_F(HumanoidPoseControllerTest, KneelLowersPelvis) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_pelvis_y = pose.pelvis_pos.y();

  controller.kneel(0.5F);

  EXPECT_LT(pose.pelvis_pos.y(), original_pelvis_y);

  float const expected_offset = 0.5F * 0.40F;
  EXPECT_NEAR(pose.pelvis_pos.y(), HumanProportions::WAIST_Y - expected_offset, 0.05F);
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

  QVector3D const target_position(0.30F, 1.20F, 0.35F);

  controller.place_hand_at(Side::Right, target_position);

  EXPECT_TRUE(approx_equal(pose.hand_r, target_position));
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtNeverStretchesTheArm) {
  using HP = HumanProportions;
  HumanoidPoseController controller(pose, anim_ctx);

  controller.place_hand_at(Side::Right, QVector3D(0.30F, 1.20F, 1.40F));

  EXPECT_LE((pose.hand_r - pose.shoulder_r).length(),
            HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN);
  EXPECT_NEAR((pose.elbow_r - pose.shoulder_r).length(), HP::UPPER_ARM_LEN, 0.002F);
  EXPECT_NEAR((pose.hand_r - pose.elbow_r).length(), HP::FORE_ARM_LEN, 0.002F);
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtComputesElbow) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(0.30F, 1.20F, 0.80F);
  QVector3D const original_elbow = pose.elbow_r;

  controller.place_hand_at(Side::Right, target_position);

  EXPECT_FALSE(approx_equal(pose.elbow_r, original_elbow));

  float const shoulder_to_elbow_dist = (pose.elbow_r - pose.shoulder_r).length();
  float const elbow_to_hand_dist = (target_position - pose.elbow_r).length();
  EXPECT_GT(shoulder_to_elbow_dist, 0.0F);
  EXPECT_GT(elbow_to_hand_dist, 0.0F);
}

TEST_F(HumanoidPoseControllerTest, SolveElbowIKReturnsValidPosition) {
  HumanoidPoseController const controller(pose, anim_ctx);

  QVector3D const shoulder = pose.shoulder_r;
  QVector3D const hand(0.35F, 1.15F, 0.75F);
  QVector3D const outward_dir(1.0F, 0.0F, 0.0F);

  QVector3D const elbow = controller.solve_elbow_ik(
      Side::Right, shoulder, hand, outward_dir, 0.45F, 0.15F, 0.0F, 1.0F);

  EXPECT_GT(elbow.length(), 0.0F);

  float const shoulder_elbow_dist = (elbow - shoulder).length();
  EXPECT_GT(shoulder_elbow_dist, 0.05F);
  EXPECT_LT(shoulder_elbow_dist, 0.50F);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKReturnsValidPosition) {
  HumanoidPoseController const controller(pose, anim_ctx);

  QVector3D const hip(0.10F, 0.93F, 0.0F);
  QVector3D const foot(0.10F, 0.0F, 0.05F);
  float const height_scale = 1.0F;

  QVector3D const knee = controller.solve_knee_ik(Side::Right, hip, foot, height_scale);

  EXPECT_LT(knee.y(), hip.y());
  EXPECT_GT(knee.y(), foot.y());

  EXPECT_GE(knee.y(), HumanProportions::GROUND_Y);
}

TEST_F(HumanoidPoseControllerTest, SolveKneeIKPreventsGroundPenetration) {
  HumanoidPoseController const controller(pose, anim_ctx);

  QVector3D const hip(0.0F, 0.30F, 0.0F);
  QVector3D const foot(0.50F, 0.0F, 0.50F);
  float const height_scale = 1.0F;

  QVector3D const knee = controller.solve_knee_ik(Side::Left, hip, foot, height_scale);

  float const min_knee_y = HumanProportions::GROUND_Y + pose.foot_y_offset * 0.5F;
  EXPECT_GE(knee.y(), min_knee_y - 0.001F);
}

TEST_F(HumanoidPoseControllerTest, PlaceHandAtLeftHandWorks) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const target_position(-0.40F, 1.30F, 0.30F);

  controller.place_hand_at(Side::Left, target_position);

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

TEST_F(HumanoidPoseControllerTest, CarrySwordAndShieldPositionsHandsCorrectly) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.carry_sword_and_shield();

  EXPECT_GT(pose.hand_r.x(), 0.28F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y);
  EXPECT_GT(pose.hand_r.z(), 0.36F);

  EXPECT_LT(pose.hand_l.x(), -0.26F);
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y + 0.01F);
  EXPECT_GT(pose.hand_l.z(), 0.20F);

  EXPECT_GT((pose.elbow_r - pose.shoulder_r).length(), 0.0F);
  EXPECT_GT((pose.elbow_l - pose.shoulder_l).length(), 0.0F);
}

TEST_F(HumanoidPoseControllerTest, SwordAndShieldCarryMovesForwardWhileMoving) {
  HumanoidPose idle_pose = pose;
  HumanoidAnimationContext const idle_anim = anim_ctx;
  HumanoidPoseController idle_controller(idle_pose, idle_anim);
  idle_controller.carry_sword_and_shield();

  HumanoidPose moving_pose = pose;
  HumanoidAnimationContext moving_anim = anim_ctx;
  moving_anim.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
  moving_anim.gait.speed = 1.5F;
  HumanoidPoseController moving_controller(moving_pose, moving_anim);
  moving_controller.carry_sword_and_shield();

  EXPECT_GT(moving_pose.hand_r.z(), idle_pose.hand_r.z());
  EXPECT_LT(moving_pose.hand_r.y(), idle_pose.hand_r.y());
  EXPECT_GT(moving_pose.hand_l.z(), idle_pose.hand_l.z());
  EXPECT_LT(moving_pose.hand_l.y(), idle_pose.hand_l.y());
}

TEST_F(HumanoidPoseControllerTest, BraceSpearForHoldAnglesTheHeadUpTheApproach) {
  anim_ctx.inputs.is_in_hold_mode = true;
  anim_ctx.inputs.hold_entry_progress = 1.0F;
  HumanoidPoseController controller(pose, anim_ctx);

  controller.brace_spear_for_hold();

  EXPECT_GT(pose.hand_r.z(), 0.36F);
  EXPECT_GT(pose.hand_r.x(), 0.5F * HumanProportions::SHOULDER_WIDTH);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y + 0.06F);
  EXPECT_GT(pose.hand_l.z(), 0.10F);
  EXPECT_LT(pose.hand_l.z(), pose.hand_r.z());
  EXPECT_GT(pose.hand_l.x(), 0.14F);
  EXPECT_LT(pose.hand_l.x(), pose.hand_r.x());
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y + 0.06F);
  EXPECT_GT(pose.hand_l.y(), HumanProportions::WAIST_Y);
  EXPECT_GT((pose.hand_r - pose.hand_l).length(), 0.18F);

  EXPECT_GT(resolve_spear_direction(anim_ctx.inputs).y(), 0.30F);
  EXPECT_LT(resolve_spear_direction(anim_ctx.inputs).y(), 0.55F);
}

TEST_F(HumanoidPoseControllerTest, HoldSpearIdleMovesSpearOutsideBodyWithTwoHands) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.hold_spear_idle();

  EXPECT_GT(pose.hand_r.x(), 0.30F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y + 0.02F);
  EXPECT_GT(pose.hand_r.z(), 0.25F);
  EXPECT_GT(pose.hand_l.x(), 0.08F);
  EXPECT_GT(pose.hand_l.y(), pose.hand_r.y());
  EXPECT_GT(pose.hand_l.z(), pose.hand_r.z());
  EXPECT_GT((pose.hand_l - pose.hand_r).length(), 0.20F);
}

TEST_F(HumanoidPoseControllerTest, HoldBowReadyKeepsHandsInLowerReadyPose) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.hold_bow_ready();

  EXPECT_GT(pose.hand_r.x(), 0.02F);
  EXPECT_GT(pose.hand_r.x(), pose.hand_l.x());
  EXPECT_GT(pose.hand_l.x(), -0.20F);
  EXPECT_GT(pose.hand_r.z(), 0.55F);
  EXPECT_GT(pose.hand_r.z(), pose.hand_l.z());
  EXPECT_LT(pose.hand_l.z(), 0.45F);
  EXPECT_GT(pose.hand_l.z(), 0.18F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y);
  EXPECT_LT(pose.hand_l.y(), HumanProportions::SHOULDER_Y + 0.05F);
  EXPECT_LT(pose.hand_r.y(), HumanProportions::SHOULDER_Y + 0.12F);
  EXPECT_GT((pose.hand_r - pose.hand_l).length(), 0.15F);
}

TEST_F(HumanoidPoseControllerTest, AimBowFullDrawLoadsShouldersAndPullsBackStringHand) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_shoulder_r = pose.shoulder_r;
  QVector3D const original_head = pose.head_pos;

  controller.aim_bow(0.40F);

  EXPECT_LT(pose.hand_l.z(), 0.18F);
  EXPECT_GT(pose.hand_r.z(), 0.64F);
  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r.z() + 0.12F);
  EXPECT_LT(pose.pelvis_pos.z(), original_pelvis.z());
  EXPECT_GT(pose.head_pos.z(), original_head.z());
}

TEST_F(HumanoidPoseControllerTest,
       SwordAndShieldDefenseRaisesShieldComparedToMarchCarry) {
  HumanoidPose march_pose = pose;
  HumanoidPoseController march_controller(march_pose, anim_ctx);
  march_controller.carry_sword_and_shield();

  HumanoidPose brace_pose = pose;
  HumanoidPoseController brace_controller(brace_pose, anim_ctx);
  brace_controller.guard_sword_and_shield_for_defense();

  EXPECT_GT(brace_pose.hand_l.y(), march_pose.hand_l.y());
  EXPECT_GT(brace_pose.hand_l.z(), march_pose.hand_l.z());
}

TEST_F(HumanoidPoseControllerTest,
       GuardShieldFormationRomanTopRaisesShieldHigherThanRomanFront) {
  HumanoidPose roman_front_pose = pose;
  HumanoidPoseController roman_front_controller(roman_front_pose, anim_ctx);
  roman_front_controller.guard_sword_and_shield_formation(
      ShieldFormationPose::RomanFront, 1.0F);

  HumanoidPose roman_top_pose = pose;
  HumanoidPoseController roman_top_controller(roman_top_pose, anim_ctx);
  roman_top_controller.guard_sword_and_shield_formation(ShieldFormationPose::RomanTop,
                                                        1.0F);

  EXPECT_GT(roman_top_pose.hand_l.y(), roman_front_pose.hand_l.y());
  EXPECT_LT(roman_top_pose.hand_l.z(), roman_front_pose.hand_l.z());
}

TEST_F(HumanoidPoseControllerTest,
       GuardShieldFormationCarthageFrontRaisesShieldHigherThanRomanFront) {
  HumanoidPose roman_front_pose = pose;
  HumanoidPoseController roman_front_controller(roman_front_pose, anim_ctx);
  roman_front_controller.guard_sword_and_shield_formation(
      ShieldFormationPose::RomanFront, 1.0F);

  HumanoidPose carthage_front_pose = pose;
  HumanoidPoseController carthage_front_controller(carthage_front_pose, anim_ctx);
  carthage_front_controller.guard_sword_and_shield_formation(
      ShieldFormationPose::CarthageFront, 1.0F);

  EXPECT_GT(carthage_front_pose.hand_l.y(), roman_front_pose.hand_l.y());
  EXPECT_LT(carthage_front_pose.hand_l.x(), roman_front_pose.hand_l.x());
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
  HumanoidPoseController const controller(pose, anim_ctx);

  float const left_y = controller.get_shoulder_y(Side::Left);
  float const right_y = controller.get_shoulder_y(Side::Right);

  EXPECT_FLOAT_EQ(left_y, pose.shoulder_l.y());
  EXPECT_FLOAT_EQ(right_y, pose.shoulder_r.y());
}

TEST_F(HumanoidPoseControllerTest, GetPelvisYReturnsCorrectValue) {
  HumanoidPoseController const controller(pose, anim_ctx);

  float const pelvis_y = controller.get_pelvis_y();

  EXPECT_FLOAT_EQ(pelvis_y, pose.pelvis_pos.y());
}

TEST_F(HumanoidPoseControllerTest, GetShoulderYReflectsKneeling) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_y = controller.get_shoulder_y(Side::Left);

  controller.kneel(0.5F);

  float const kneeling_shoulder_y = controller.get_shoulder_y(Side::Left);

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

TEST_F(HumanoidPoseControllerTest, SpearThrustMovesShouldersForwardTogether) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.spear_thrust(0.23F);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_NEAR(pose.shoulder_l.z(), pose.shoulder_r.z(), 0.0001F);
}

TEST_F(HumanoidPoseControllerTest, SpearThrustReadyGripKeepsHandsClearAndAligned) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.spear_thrust(0.0F);

  EXPECT_GT(pose.hand_r.x(), 0.5F * HumanProportions::SHOULDER_WIDTH);
  EXPECT_GT(pose.hand_r.z(), 0.10F);
  EXPECT_GT(pose.hand_l.x(), 0.15F);
  EXPECT_LT(pose.hand_l.x(), pose.hand_r.x());
  EXPECT_GT(pose.hand_l.y(), HumanProportions::SHOULDER_Y - 0.04F);
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

  controller.combat_sword_slash_variant(0.20F, 0);

  EXPECT_LT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_GT(pose.shoulder_l.z(), original_shoulder_l_z);
}

TEST_F(HumanoidPoseControllerTest, SwordSlashVariant1ReversesInitialTorsoTwist) {
  HumanoidPoseController controller_v0(pose, anim_ctx);
  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller_v0.combat_sword_slash_variant(0.20F, 1);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
}

TEST_F(HumanoidPoseControllerTest, SwordSlashVariantAppliesBaselineBodyDrive) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_head = pose.head_pos;

  controller.combat_sword_slash_variant(0.62F, 0);

  EXPECT_LT(pose.pelvis_pos.y(), original_pelvis.y());
  EXPECT_GT(pose.pelvis_pos.z(), original_pelvis.z());
  EXPECT_GT(pose.head_pos.z(), original_head.z());
}

TEST_F(HumanoidPoseControllerTest, CommanderAttackEmphasisAmplifiesSwordSlashVariant) {
  HumanoidPose base_pose = pose;
  HumanoidAnimationContext const base_ctx = anim_ctx;
  HumanoidPoseController base_controller(base_pose, base_ctx);
  base_controller.combat_sword_slash_variant(0.56F, 0);

  HumanoidPose emphasized_pose = pose;
  HumanoidAnimationContext emphasized_ctx = anim_ctx;
  emphasized_ctx.amplified_attack = true;
  emphasized_ctx.attack_emphasis = 1.35F;
  emphasized_ctx.finisher_attack = true;
  HumanoidPoseController emphasized_controller(emphasized_pose, emphasized_ctx);
  emphasized_controller.combat_sword_slash_variant(0.56F, 0);

  EXPECT_GT(emphasized_pose.hand_r.z(), base_pose.hand_r.z());
  EXPECT_GT(emphasized_pose.pelvis_pos.z(), base_pose.pelvis_pos.z());
  EXPECT_LT(emphasized_pose.pelvis_pos.y(), base_pose.pelvis_pos.y());
}

TEST_F(HumanoidPoseControllerTest, SwordSlashVariantReducedReachKeepsStrikeCloser) {
  HumanoidPose default_pose = pose;
  HumanoidPoseController default_controller(default_pose, anim_ctx);
  default_controller.combat_sword_slash_variant(0.56F, 0);

  HumanoidPose reduced_pose = pose;
  HumanoidPoseController reduced_controller(reduced_pose, anim_ctx);
  reduced_controller.combat_sword_slash_variant(0.56F, 0, 0.88F);

  EXPECT_LT(reduced_pose.hand_r.z(), default_pose.hand_r.z());
  EXPECT_GT(reduced_pose.hand_r.z(), reduced_pose.shoulder_r.z());
}

TEST_F(HumanoidPoseControllerTest,
       CombatSwordSlashVariantDrivesFartherThanBuilderSlash) {
  HumanoidPose builder_pose = pose;
  HumanoidPoseController builder_controller(builder_pose, anim_ctx);
  builder_controller.sword_slash_variant(0.62F, 0);

  HumanoidPose combat_pose = pose;
  HumanoidPoseController combat_controller(combat_pose, anim_ctx);
  combat_controller.combat_sword_slash_variant(0.62F, 0);

  EXPECT_GT(combat_pose.hand_r.z(), builder_pose.hand_r.z());
  EXPECT_GT(combat_pose.pelvis_pos.z(), builder_pose.pelvis_pos.z());
  EXPECT_LT(combat_pose.pelvis_pos.y(), builder_pose.pelvis_pos.y());
}

TEST_F(HumanoidPoseControllerTest, SpearThrustVariantMovesShouldersForwardTogether) {
  HumanoidPoseController controller(pose, anim_ctx);

  float const original_shoulder_r_z = pose.shoulder_r.z();

  controller.spear_thrust_variant(0.23F, 0);

  EXPECT_GT(pose.shoulder_r.z(), original_shoulder_r_z);
  EXPECT_NEAR(pose.shoulder_l.z(), pose.shoulder_r.z(), 0.0001F);
}

TEST_F(HumanoidPoseControllerTest, SpearThrustVariantReadyGripMatchesNormalStance) {
  HumanoidPoseController controller(pose, anim_ctx);

  controller.spear_thrust_variant(0.0F, 0);

  EXPECT_GT(pose.hand_r.x(), 0.5F * HumanProportions::SHOULDER_WIDTH);
  EXPECT_GT(pose.hand_r.z(), 0.10F);
  EXPECT_GT(pose.hand_l.x(), 0.15F);
  EXPECT_LT(pose.hand_l.x(), pose.hand_r.x());
  EXPECT_GT(pose.hand_l.y(), HumanProportions::SHOULDER_Y - 0.04F);
}

TEST_F(HumanoidPoseControllerTest, SpearThrustVariantDrivesPelvisAndHeadAtExtension) {
  HumanoidPoseController controller(pose, anim_ctx);

  QVector3D const original_pelvis = pose.pelvis_pos;
  QVector3D const original_head = pose.head_pos;

  controller.spear_thrust_variant(0.60F, 0);

  EXPECT_FLOAT_EQ(pose.pelvis_pos.y(), original_pelvis.y());
  EXPECT_GT(pose.pelvis_pos.z(), original_pelvis.z());
  EXPECT_GT(pose.head_pos.z(), original_head.z());
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

  HumanoidPose const standing_pose = pose;
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
  AnimationInputs const standing_inputs{};
  QVector3D const standing_dir = resolve_spear_direction(standing_inputs);

  AnimationInputs partial_hold_inputs{};
  partial_hold_inputs.is_in_hold_mode = true;
  partial_hold_inputs.hold_entry_progress = 0.5F;
  QVector3D const partial_hold_dir = resolve_spear_direction(partial_hold_inputs);

  AnimationInputs full_hold_inputs{};
  full_hold_inputs.is_in_hold_mode = true;
  full_hold_inputs.hold_entry_progress = 1.0F;
  QVector3D const full_hold_dir = resolve_spear_direction(full_hold_inputs);

  EXPECT_LT(partial_hold_dir.y(), standing_dir.y());
  EXPECT_GT(partial_hold_dir.y(), full_hold_dir.y());
  EXPECT_GT(standing_dir.y(), 0.0F);
  EXPECT_GT(full_hold_dir.y(), 0.30F);
  EXPECT_GT(partial_hold_dir.z(), standing_dir.z());
}

TEST_F(HumanoidPoseControllerTest, SpearDirectionMatchesExitHoldDepth) {
  AnimationInputs entry_inputs{};
  entry_inputs.is_in_hold_mode = true;
  entry_inputs.hold_entry_progress = 0.75F;
  QVector3D const entry_dir = resolve_spear_direction(entry_inputs);

  AnimationInputs exit_inputs{};
  exit_inputs.is_exiting_hold = true;
  exit_inputs.hold_exit_progress = 0.25F;
  QVector3D const exit_dir = resolve_spear_direction(exit_inputs);

  EXPECT_TRUE(approx_equal(entry_dir, exit_dir, 0.001F));
}

TEST_F(HumanoidPoseControllerTest, ArcherHoldKneelsDeeperThanSpearmanHold) {
  HumanoidPose spear_pose = pose;
  HumanoidPoseController spear_ctrl(spear_pose, anim_ctx);
  spear_ctrl.kneel(0.875F);

  HumanoidPose archer_pose = pose;
  HumanoidPoseController archer_ctrl(archer_pose, anim_ctx);
  archer_ctrl.kneel(1.125F);

  EXPECT_GT(spear_pose.pelvis_pos.y(), archer_pose.pelvis_pos.y())
      << "Spearman (0.875) should kneel less deeply than archer (1.125)";
}
