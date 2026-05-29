

#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "render/creature/movement_state.h"
#include "render/creature/pose_intent.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"

using namespace Render::Humanoid;
using Render::GL::AnimationInputs;
using Render::GL::HumanoidPose;

namespace {

constexpr float k_eps = 1e-4F;

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

auto basis_is_orthonormal(const QMatrix4x4& m) -> bool {
  QVector3D const x = m.column(0).toVector3D();
  QVector3D const y = m.column(1).toVector3D();
  QVector3D const z = m.column(2).toVector3D();
  if (std::abs(x.length() - 1.0F) > k_eps) {
    return false;
  }
  if (std::abs(y.length() - 1.0F) > k_eps) {
    return false;
  }
  if (std::abs(z.length() - 1.0F) > k_eps) {
    return false;
  }
  if (std::abs(QVector3D::dotProduct(x, y)) > k_eps) {
    return false;
  }
  if (std::abs(QVector3D::dotProduct(x, z)) > k_eps) {
    return false;
  }
  if (std::abs(QVector3D::dotProduct(y, z)) > k_eps) {
    return false;
  }
  return true;
}

} // namespace

TEST(HumanoidSkeletonTest, ParentIndicesAreTopologicallySorted) {
  EXPECT_EQ(parent_of(HumanoidBone::Root), k_invalid_bone);
  for (std::size_t i = 1; i < k_bone_count; ++i) {
    std::uint8_t const parent = k_bone_parents[i];
    ASSERT_NE(parent, k_invalid_bone)
        << "bone " << bone_name(static_cast<HumanoidBone>(i)) << " has no parent";
    EXPECT_LT(parent, i) << "bone " << bone_name(static_cast<HumanoidBone>(i))
                         << " has out-of-order parent";
  }
}

TEST(HumanoidSkeletonTest, EveryBoneHasAName) {
  for (std::size_t i = 0; i < k_bone_count; ++i) {
    EXPECT_FALSE(bone_name(static_cast<HumanoidBone>(i)).empty());
  }
}

TEST(HumanoidSkeletonTest, EvaluatorFillsEveryBoneWithOrthonormalBasis) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  for (std::size_t i = 0; i < k_bone_count; ++i) {
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

  EXPECT_LT((origin_of(HumanoidBone::Pelvis) - pose.pelvis_pos).length(), k_eps);
  EXPECT_LT((origin_of(HumanoidBone::Head) - pose.head_pos).length(), k_eps);
  EXPECT_LT((origin_of(HumanoidBone::HandL) - pose.hand_l).length(), k_eps);
  EXPECT_LT((origin_of(HumanoidBone::HandR) - pose.hand_r).length(), k_eps);
  EXPECT_LT((origin_of(HumanoidBone::FootL) - pose.foot_l).length(), k_eps);
  EXPECT_LT((origin_of(HumanoidBone::FootR) - pose.foot_r).length(), k_eps);
}

TEST(HumanoidSkeletonTest, SpineLongAxisPointsUpForUprightPose) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  QVector3D const spine_y =
      palette[static_cast<std::size_t>(HumanoidBone::Spine)].column(1).toVector3D();
  EXPECT_GT(spine_y.y(), 0.9F);
}

TEST(HumanoidSkeletonTest, ForearmLongAxisPointsFromElbowToHand) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const expected = (pose.hand_r - pose.elbow_r).normalized();
  QVector3D const got =
      palette[static_cast<std::size_t>(HumanoidBone::ForearmR)].column(1).toVector3D();
  EXPECT_LT((got - expected).length(), k_eps);
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
  for (std::size_t i = 0; i < k_bone_count; ++i) {
    EXPECT_TRUE(basis_is_orthonormal(palette[i]));
  }
}

TEST(HumanoidSkeletonTest, ZeroLengthRightAxisFallsBackToWorldRight) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(0.0F, 0.0F, 0.0F), palette);
  for (std::size_t i = 0; i < k_bone_count; ++i) {
    EXPECT_TRUE(basis_is_orthonormal(palette[i]));
  }
}

TEST(HumanoidSocketTest, HeadSocketTracksHeadPosition) {
  auto pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  QVector3D const head_origin = socket_position(palette, HumanoidSocket::Head);
  EXPECT_LT((head_origin - pose.head_pos).length(), k_eps);

  pose.head_pos += QVector3D(0.0F, 0.2F, 0.15F);
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const moved = socket_position(palette, HumanoidSocket::Head);
  EXPECT_LT((moved - pose.head_pos).length(), k_eps);
}

TEST(HumanoidSocketTest, HandSocketsMatchHandPositions) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  EXPECT_LT((socket_position(palette, HumanoidSocket::HandR) - pose.hand_r).length(),
            k_eps);
  EXPECT_LT((socket_position(palette, HumanoidSocket::HandL) - pose.hand_l).length(),
            k_eps);
}

TEST(HumanoidSocketTest, GripSocketFollowsHandSwingAxis) {
  auto pose = make_upright_pose();
  pose.elbow_r = QVector3D(0.18F, 1.28F, 0.05F);
  pose.hand_r = QVector3D(0.42F, 1.02F, 0.42F);

  QVector3D expected = pose.hand_r - pose.elbow_r;
  ASSERT_GT(expected.lengthSquared(), 1e-6F);
  expected.normalize();

  QVector3D right_hint(1.0F, 0.0F, 0.0F);
  right_hint -= expected * QVector3D::dotProduct(right_hint, expected);
  ASSERT_GT(right_hint.lengthSquared(), 1e-6F);
  right_hint.normalize();
  QVector3D const forward = QVector3D::crossProduct(right_hint, expected).normalized();

  pose.body_frames.hand_r.origin = pose.hand_r;
  pose.body_frames.hand_r.right = right_hint;
  pose.body_frames.hand_r.up = expected;
  pose.body_frames.hand_r.forward = forward;
  pose.body_frames.hand_r.radius = 0.05F;

  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  auto const grip = socket_attachment_frame(palette, HumanoidSocket::GripR);
  QVector3D actual = grip.up;
  ASSERT_GT(actual.lengthSquared(), 1e-6F);
  actual.normalize();

  EXPECT_GT(QVector3D::dotProduct(actual, expected), 0.9F);
}

TEST(HumanoidSocketTest, BackSocketIsBehindChest) {
  auto const pose = make_upright_pose();
  BonePalette palette;
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  QVector3D const back = socket_position(palette, HumanoidSocket::Back);
  QVector3D const chest_origin =
      palette[static_cast<std::size_t>(HumanoidBone::Chest)].column(3).toVector3D();

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

TEST(HumanoidSocketTest, BindLeftShieldFrameMatchesHandSocketFrame) {
  auto const& bind_frames = humanoid_bind_body_frames();

  auto const expected_shield_l =
      socket_attachment_frame(bind_frames.hand_l, HumanoidSocket::GripL);

  EXPECT_LT((bind_frames.shield_l.origin - expected_shield_l.origin).length(), k_eps);
  EXPECT_LT((bind_frames.shield_l.right - expected_shield_l.right).length(), k_eps);
  EXPECT_LT((bind_frames.shield_l.up - expected_shield_l.up).length(), k_eps);
  EXPECT_LT((bind_frames.shield_l.forward - expected_shield_l.forward).length(), k_eps);
}

TEST(HumanoidSocketTest, BindLeftGripFrameKeepsBodySideCarryBasis) {
  auto const& bind_frames = humanoid_bind_body_frames();

  EXPECT_LT((bind_frames.grip_l.origin - bind_frames.hand_l.origin).length(), k_eps);
  EXPECT_LT((bind_frames.grip_l.right + bind_frames.torso.right).length(), k_eps);
  EXPECT_LT((bind_frames.grip_l.up - bind_frames.torso.up).length(), k_eps);
  EXPECT_LT((bind_frames.grip_l.forward + bind_frames.torso.forward).length(), k_eps);
}
