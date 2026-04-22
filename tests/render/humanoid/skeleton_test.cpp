// Stage 15 — tests for humanoid skeleton, state machine, clip driver,
// and bone-socket API.
//
// Covers:
//   * Skeleton topology invariants (parent indices are strictly less
//     than child, every non-root has a parent).
//   * Skeleton evaluator produces sane bone transforms from a known
//     HumanoidPose and keeps orthonormal bases.
//   * Socket transforms place equipment at the expected bone origin
//     plus local offset, and track when the underlying pose moves.
//   * Humanoid state machine maps AnimationInputs to the correct
//     discrete state under every documented priority rule.
//   * Clip driver samples non-zero overlay values in Idle/Walk/Run
//     and zero during Death.

#include "render/humanoid/clip_driver_cache.h"
#include "render/humanoid/humanoid_clip_registry.h"
#include "render/humanoid/humanoid_state_machine.h"
#include "render/humanoid/skeleton.h"

#include "render/gl/humanoid/humanoid_types.h"

#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::Humanoid;
using Render::GL::AnimationInputs;
using Render::GL::HumanoidPose;

namespace {

constexpr float kEps = 1e-4F;

// Minimal well-formed pose: standing upright, arms at sides.
auto make_upright_pose() -> HumanoidPose {
  HumanoidPose p{};
  p.pelvis_pos = QVector3D(0.0F, 1.00F, 0.0F);
  p.neck_base = QVector3D(0.0F, 1.60F, 0.0F);
  p.head_pos = QVector3D(0.0F, 1.75F, 0.0F);
  p.head_r = 0.12F;

  p.shoulder_l = QVector3D(-0.20F, 1.55F, 0.0F);
  p.shoulder_r = QVector3D(0.20F, 1.55F, 0.0F);
  p.elbow_l = QVector3D(-0.25F, 1.25F, 0.0F);
  p.elbow_r = QVector3D(0.25F, 1.25F, 0.0F);
  p.hand_l = QVector3D(-0.27F, 0.95F, 0.0F);
  p.hand_r = QVector3D(0.27F, 0.95F, 0.0F);

  p.knee_l = QVector3D(-0.12F, 0.55F, 0.0F);
  p.knee_r = QVector3D(0.12F, 0.55F, 0.0F);
  p.foot_l = QVector3D(-0.12F, 0.05F, 0.0F);
  p.foot_r = QVector3D(0.12F, 0.05F, 0.0F);
  return p;
}

auto basis_is_orthonormal(const QMatrix4x4 &m) -> bool {
  QVector3D const x = m.column(0).toVector3D();
  QVector3D const y = m.column(1).toVector3D();
  QVector3D const z = m.column(2).toVector3D();
  if (std::abs(x.length() - 1.0F) > kEps)
    return false;
  if (std::abs(y.length() - 1.0F) > kEps)
    return false;
  if (std::abs(z.length() - 1.0F) > kEps)
    return false;
  if (std::abs(QVector3D::dotProduct(x, y)) > kEps)
    return false;
  if (std::abs(QVector3D::dotProduct(x, z)) > kEps)
    return false;
  if (std::abs(QVector3D::dotProduct(y, z)) > kEps)
    return false;
  return true;
}

} // namespace

TEST(HumanoidSkeletonTest, ParentIndicesAreTopologicallySorted) {
  EXPECT_EQ(parent_of(HumanoidBone::Root), kInvalidBone);
  for (std::size_t i = 1; i < kBoneCount; ++i) {
    std::uint8_t const parent = kBoneParents[i];
    ASSERT_NE(parent, kInvalidBone)
        << "bone " << bone_name(static_cast<HumanoidBone>(i))
        << " has no parent";
    EXPECT_LT(parent, i) << "bone " << bone_name(static_cast<HumanoidBone>(i))
                         << " has out-of-order parent";
  }
}

TEST(HumanoidSkeletonTest, EveryBoneHasAName) {
  for (std::size_t i = 0; i < kBoneCount; ++i) {
    EXPECT_FALSE(bone_name(static_cast<HumanoidBone>(i)).empty());
  }
}

TEST(HumanoidSkeletonTest, EvaluatorFillsEveryBoneWithOrthonormalBasis) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  for (std::size_t i = 0; i < kBoneCount; ++i) {
    EXPECT_TRUE(basis_is_orthonormal(palette[i]))
        << "bone " << bone_name(static_cast<HumanoidBone>(i))
        << " has non-orthonormal basis";
  }
}

TEST(HumanoidSkeletonTest, TorsoChainOriginsMatchPose) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  auto origin_of = [&](HumanoidBone b) {
    return palette[static_cast<std::size_t>(b)].column(3).toVector3D();
  };

  EXPECT_LT((origin_of(HumanoidBone::Pelvis) - pose.pelvis_pos).length(), kEps);
  EXPECT_LT((origin_of(HumanoidBone::Head) - pose.head_pos).length(), kEps);
  EXPECT_LT((origin_of(HumanoidBone::HandL) - pose.hand_l).length(), kEps);
  EXPECT_LT((origin_of(HumanoidBone::HandR) - pose.hand_r).length(), kEps);
  EXPECT_LT((origin_of(HumanoidBone::FootL) - pose.foot_l).length(), kEps);
  EXPECT_LT((origin_of(HumanoidBone::FootR) - pose.foot_r).length(), kEps);
}

TEST(HumanoidSkeletonTest, SpineLongAxisPointsUpForUprightPose) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  QVector3D const spine_y =
      palette[static_cast<std::size_t>(HumanoidBone::Spine)]
          .column(1)
          .toVector3D();
  EXPECT_GT(spine_y.y(), 0.9F);
}

TEST(HumanoidSkeletonTest, ForearmLongAxisPointsFromElbowToHand) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const expected = (pose.hand_r - pose.elbow_r).normalized();
  QVector3D const got =
      palette[static_cast<std::size_t>(HumanoidBone::ForearmR)]
          .column(1)
          .toVector3D();
  EXPECT_LT((got - expected).length(), kEps);
}

TEST(HumanoidSkeletonTest, DegenerateBoneFallsBackToWorldUp) {
  HumanoidPose p{};
  p.pelvis_pos = p.neck_base = p.head_pos = QVector3D(0.0F, 1.0F, 0.0F);
  p.shoulder_l = p.shoulder_r = QVector3D(0.0F, 1.0F, 0.0F);
  p.elbow_l = p.elbow_r = QVector3D(0.0F, 1.0F, 0.0F);
  p.hand_l = p.hand_r = QVector3D(0.0F, 1.0F, 0.0F);
  p.knee_l = p.knee_r = QVector3D(0.0F, 1.0F, 0.0F);
  p.foot_l = p.foot_r = QVector3D(0.0F, 1.0F, 0.0F);

  BonePalette palette;
  evaluate_skeleton(p, QVector3D(1.0F, 0.0F, 0.0F), palette);
  for (std::size_t i = 0; i < kBoneCount; ++i) {
    EXPECT_TRUE(basis_is_orthonormal(palette[i]));
  }
}

TEST(HumanoidSkeletonTest, ZeroLengthRightAxisFallsBackToWorldRight) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(0.0F, 0.0F, 0.0F), palette);
  for (std::size_t i = 0; i < kBoneCount; ++i) {
    EXPECT_TRUE(basis_is_orthonormal(palette[i]));
  }
}

TEST(HumanoidSocketTest, HeadSocketTracksHeadPosition) {
  auto pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  QVector3D const head_origin = socket_position(palette, HumanoidSocket::Head);
  EXPECT_LT((head_origin - pose.head_pos).length(), kEps);

  pose.head_pos += QVector3D(0.0F, 0.2F, 0.15F);
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const moved = socket_position(palette, HumanoidSocket::Head);
  EXPECT_LT((moved - pose.head_pos).length(), kEps);
}

TEST(HumanoidSocketTest, HandSocketsMatchHandPositions) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  EXPECT_LT(
      (socket_position(palette, HumanoidSocket::HandR) - pose.hand_r).length(),
      kEps);
  EXPECT_LT(
      (socket_position(palette, HumanoidSocket::HandL) - pose.hand_l).length(),
      kEps);
}

TEST(HumanoidSocketTest, BackSocketIsBehindChest) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const back = socket_position(palette, HumanoidSocket::Back);
  QVector3D const chest_origin =
      palette[static_cast<std::size_t>(HumanoidBone::Chest)]
          .column(3)
          .toVector3D();
  // Back socket offsets along -Z in bone-local space. With +X as right
  // and +Y up the spine, Z = X×Y = +Z world; so -Z local => -Z world.
  EXPECT_LT(back.z(), chest_origin.z());
}

TEST(HumanoidSocketTest, AttachmentFrameMatchesSocketTransform) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  auto const frame = socket_attachment_frame(palette, HumanoidSocket::HandR);
  QMatrix4x4 const m = socket_transform(palette, HumanoidSocket::HandR);

  EXPECT_LT((frame.origin - m.column(3).toVector3D()).length(), 1e-5F);
  EXPECT_LT((frame.right - m.column(0).toVector3D()).length(), 1e-5F);
  EXPECT_LT((frame.up - m.column(1).toVector3D()).length(), 1e-5F);
  EXPECT_LT((frame.forward - m.column(2).toVector3D()).length(), 1e-5F);

  // Orthonormal basis — the grip directions equipment renderers rely on.
  EXPECT_NEAR(frame.right.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(frame.up.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(frame.forward.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(QVector3D::dotProduct(frame.right, frame.up), 0.0F, 1e-4F);
}

TEST(HumanoidSocketTest, FootSocketsStayLevelInsteadOfFollowingShinTilt) {
  auto pose = make_upright_pose();
  pose.knee_l = QVector3D(-0.10F, 0.62F, 0.08F);
  pose.knee_r = QVector3D(0.10F, 0.60F, -0.06F);
  pose.foot_l = QVector3D(-0.14F, 0.02F, 0.06F);
  pose.foot_r = QVector3D(0.14F, 0.02F, -0.05F);

  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  auto const foot_l = socket_attachment_frame(palette, HumanoidSocket::FootL);
  auto const foot_r = socket_attachment_frame(palette, HumanoidSocket::FootR);

  EXPECT_GT(foot_l.up.y(), 0.95F);
  EXPECT_GT(foot_r.up.y(), 0.95F);
  EXPECT_LT(std::abs(foot_l.up.x()), 0.10F);
  EXPECT_LT(std::abs(foot_r.up.x()), 0.10F);
  EXPECT_LT(std::abs(foot_l.up.z()), 0.10F);
  EXPECT_LT(std::abs(foot_r.up.z()), 0.10F);
}

TEST(HumanoidStateMachineTest, PriorityHitOverridesAttack) {
  AnimationInputs inputs{};
  inputs.is_attacking = true;
  inputs.is_melee = true;
  inputs.is_hit_reacting = true;
  EXPECT_EQ(select_state(inputs), HumanoidState::HitReaction);
}

TEST(HumanoidStateMachineTest, DeadOverridesEverything) {
  AnimationInputs inputs{};
  inputs.is_attacking = true;
  inputs.is_hit_reacting = true;
  inputs.is_moving = true;
  EXPECT_EQ(select_state(inputs, /*dead_flag=*/true), HumanoidState::Death);
}

TEST(HumanoidStateMachineTest, RunningBeatsWalking) {
  AnimationInputs inputs{};
  inputs.is_moving = true;
  inputs.is_running = true;
  EXPECT_EQ(select_state(inputs), HumanoidState::Run);
  inputs.is_running = false;
  EXPECT_EQ(select_state(inputs), HumanoidState::Walk);
  inputs.is_moving = false;
  EXPECT_EQ(select_state(inputs), HumanoidState::Idle);
}

TEST(HumanoidStateMachineTest, HoldBeatsLocomotion) {
  AnimationInputs inputs{};
  inputs.is_in_hold_mode = true;
  inputs.is_moving = true;
  EXPECT_EQ(select_state(inputs), HumanoidState::Hold);
}

TEST(HumanoidStateMachineTest, AttackRangedVsMelee) {
  AnimationInputs inputs{};
  inputs.is_attacking = true;
  inputs.is_melee = false;
  EXPECT_EQ(select_state(inputs), HumanoidState::AttackRanged);
  inputs.is_melee = true;
  EXPECT_EQ(select_state(inputs), HumanoidState::AttackMelee);
}

TEST(HumanoidStateMachineTest, TickTransitionsAndBlends) {
  HumanoidStateMachine sm;
  EXPECT_EQ(sm.current(), HumanoidState::Idle);

  AnimationInputs walk{};
  walk.is_moving = true;

  sm.tick(0.0F, walk);
  EXPECT_EQ(sm.current(), HumanoidState::Walk);
  EXPECT_TRUE(sm.is_blending());
  EXPECT_EQ(sm.previous(), HumanoidState::Idle);

  sm.tick(1.0F, walk);
  EXPECT_FALSE(sm.is_blending());
  EXPECT_NEAR(sm.blend_weight(), 1.0F, 1e-5F);
}

TEST(HumanoidStateMachineTest, DeathSnapsWithoutBlend) {
  HumanoidStateMachine sm;
  AnimationInputs inputs{};
  sm.tick(0.0F, inputs, /*dead_flag=*/true);
  EXPECT_EQ(sm.current(), HumanoidState::Death);
  EXPECT_FALSE(sm.is_blending());
}

TEST(HumanoidClipDriverTest, IdleOverlaysAreNonZero) {
  HumanoidClipDriver driver;
  driver.snap_to(HumanoidState::Idle);
  float max_sway = 0.0F;
  float max_breath = 0.0F;
  for (float t = 0.0F; t < 3.0F; t += 0.1F) {
    auto const o = driver.sample(t);
    max_sway = std::max(max_sway, std::abs(o.torso_sway_x));
    max_breath = std::max(max_breath, std::abs(o.breathing_y));
  }
  EXPECT_GT(max_sway, 0.005F);
  EXPECT_GT(max_breath, 0.005F);
}

TEST(HumanoidClipDriverTest, DeathOverlaysAreZero) {
  HumanoidClipDriver driver;
  driver.snap_to(HumanoidState::Death);
  for (float t = 0.0F; t < 2.0F; t += 0.2F) {
    auto const o = driver.sample(t);
    EXPECT_NEAR(o.torso_sway_x, 0.0F, 1e-5F);
    EXPECT_NEAR(o.breathing_y, 0.0F, 1e-5F);
    EXPECT_NEAR(o.hand_jitter_amp, 0.0F, 1e-5F);
  }
}

TEST(HumanoidClipDriverTest, RunProducesLargerSwayThanIdle) {
  HumanoidClipDriver idle;
  idle.snap_to(HumanoidState::Idle);
  HumanoidClipDriver run;
  run.snap_to(HumanoidState::Run);
  float idle_peak = 0.0F;
  float run_peak = 0.0F;
  for (float t = 0.0F; t < 3.0F; t += 0.05F) {
    idle_peak = std::max(idle_peak, std::abs(idle.sample(t).torso_sway_x));
    run_peak = std::max(run_peak, std::abs(run.sample(t).torso_sway_x));
  }
  EXPECT_GT(run_peak, idle_peak);
}

TEST(HumanoidClipDriverTest, TickDrivesStateFromInputs) {
  HumanoidClipDriver driver;
  AnimationInputs inputs{};
  inputs.is_moving = true;
  inputs.is_running = true;
  driver.tick(0.0F, inputs);
  EXPECT_EQ(driver.state(), HumanoidState::Run);

  inputs.is_moving = false;
  inputs.is_running = false;
  driver.tick(1.0F, inputs);
  EXPECT_EQ(driver.state(), HumanoidState::Idle);
}

TEST(ClipDriverCacheTest, GetOrCreateReturnsSameEntry) {
  ClipDriverCache cache;
  cache.advance_frame(1);
  auto &a = cache.get_or_create(42U);
  a.last_time = 3.5F;
  auto &b = cache.get_or_create(42U);
  EXPECT_EQ(&a, &b);
  EXPECT_FLOAT_EQ(b.last_time, 3.5F);
}

TEST(ClipDriverCacheTest, EvictsStaleEntries) {
  ClipDriverCache cache;
  cache.advance_frame(1);
  cache.get_or_create(7U);
  EXPECT_EQ(cache.size(), 1U);
  // advance past max_age; entry must be evicted.
  cache.advance_frame(200, /*max_age=*/120);
  EXPECT_EQ(cache.size(), 0U);
}

TEST(ClipDriverCacheTest, KeepsRecentlySeenEntries) {
  ClipDriverCache cache;
  for (std::uint32_t f = 1; f <= 200; ++f) {
    cache.advance_frame(f, /*max_age=*/120);
    cache.get_or_create(11U); // touch every frame
  }
  EXPECT_EQ(cache.size(), 1U);
}

TEST(ApplyOverlaysTest, ShiftsUpperTorsoOnly) {
  Render::GL::HumanoidPose pose{};
  pose.pelvis_pos = QVector3D(0.0F, 1.0F, 0.0F);
  pose.neck_base = QVector3D(0.0F, 1.6F, 0.0F);
  pose.head_pos = QVector3D(0.0F, 1.75F, 0.0F);
  pose.shoulder_l = QVector3D(-0.2F, 1.55F, 0.0F);
  pose.shoulder_r = QVector3D(0.2F, 1.55F, 0.0F);
  pose.knee_l = QVector3D(-0.12F, 0.55F, 0.0F);
  pose.knee_r = QVector3D(0.12F, 0.55F, 0.0F);
  pose.foot_l = QVector3D(-0.12F, 0.05F, 0.0F);
  pose.foot_r = QVector3D(0.12F, 0.05F, 0.0F);

  HumanoidOverlays overlays;
  overlays.torso_sway_x = 0.05F;
  overlays.breathing_y = 0.02F;

  auto const before_pelvis = pose.pelvis_pos;
  auto const before_foot_l = pose.foot_l;
  auto const before_shoulder_l = pose.shoulder_l;

  apply_overlays_to_pose(pose, overlays, QVector3D(1.0F, 0.0F, 0.0F));

  // Pelvis and feet must not move (legs untouched).
  EXPECT_EQ(pose.pelvis_pos, before_pelvis);
  EXPECT_EQ(pose.foot_l, before_foot_l);
  // Upper torso shifted laterally + vertically at the neck/head, laterally
  // only at shoulders.
  EXPECT_NEAR(pose.head_pos.x() - 0.05F, 0.0F, 1e-5F);
  EXPECT_NEAR(pose.head_pos.y() - 1.77F, 0.0F, 1e-5F);
  EXPECT_NEAR(pose.shoulder_l.x() - before_shoulder_l.x() - 0.05F, 0.0F, 1e-5F);
  EXPECT_NEAR(pose.shoulder_l.y() - before_shoulder_l.y(), 0.0F, 1e-5F);
}
