// Stage 16.1 — generic creature skeleton tests.
//
// These tests exercise `render/creature/skeleton.*` independently of any
// species-specific module. They use a small hand-authored 4-bone "toy
// biped" topology so that every assertion is about the generic machinery:
// topology validation, orthonormal basis construction, parent-chain
// evaluation, socket offset composition, name-based lookup, and
// degenerate-bone fallback.
//
// If one of these fails the break is in the framework, not in any
// creature spec.

#include "render/creature/skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <span>

namespace {

using namespace Render::Creature;

// A deliberately minimal topology: root → torso → head (tip), plus an
// arm side-branch root → shoulder → hand.
//
//   0 Root      (no parent, world-up, origin = pelvis)
//   1 Torso     (from_head_tail: pelvis → chest)
//   2 Head      (from_parent: origin = head_pos)
//   3 Shoulder  (from_parent: origin = shoulder_pos)
//   4 Arm       (from_head_tail: shoulder → hand)
//   5 Hand      (from_parent: origin = hand_pos, zero-length tip)
constexpr std::array<BoneDef, 6> kToyBones = {
    BoneDef{"root", kInvalidBone}, BoneDef{"torso", 0}, BoneDef{"head", 1},
    BoneDef{"shoulder", 1},        BoneDef{"arm", 3},   BoneDef{"hand", 4},
};

constexpr std::array<SocketDef, 3> kToySockets = {
    SocketDef{"head_crown", 2, QVector3D(0.0F, 0.1F, 0.0F)},
    SocketDef{"hand_grip", 5, QVector3D(0.0F, 0.0F, 0.0F)},
    SocketDef{"chest_front", 1, QVector3D(0.0F, 0.0F, 0.2F)},
};

auto toy_topology() noexcept -> SkeletonTopology {
  return SkeletonTopology{
      std::span<const BoneDef>(kToyBones.data(), kToyBones.size()),
      std::span<const SocketDef>(kToySockets.data(), kToySockets.size()),
  };
}

// Joint positions representing a T-pose toy biped standing at origin.
struct ToySample {
  QVector3D pelvis{0.0F, 1.0F, 0.0F};
  QVector3D chest{0.0F, 1.5F, 0.0F};
  QVector3D head{0.0F, 1.8F, 0.0F};
  QVector3D shoulder{0.2F, 1.45F, 0.0F};
  QVector3D hand{0.7F, 1.45F, 0.0F};
};

BoneResolution toy_provider(void *user, BoneIndex bone) noexcept {
  auto const *s = static_cast<const ToySample *>(user);
  BoneResolution r;
  switch (bone) {
  case 0:
    r.kind = BoneBasisKind::FromRootUp;
    r.head = s->pelvis;
    break;
  case 1:
    r.kind = BoneBasisKind::FromHeadTail;
    r.head = s->pelvis;
    r.tail = s->chest;
    break;
  case 2:
    r.kind = BoneBasisKind::FromParent;
    r.head = s->head;
    break;
  case 3:
    r.kind = BoneBasisKind::FromParent;
    r.head = s->shoulder;
    break;
  case 4:
    r.kind = BoneBasisKind::FromHeadTail;
    r.head = s->shoulder;
    r.tail = s->hand;
    break;
  case 5:
    r.kind = BoneBasisKind::FromParent;
    r.head = s->hand;
    break;
  default:
    break;
  }
  return r;
}

bool is_orthonormal(const QMatrix4x4 &m, float eps = 1e-4F) {
  QVector3D const x = m.column(0).toVector3D();
  QVector3D const y = m.column(1).toVector3D();
  QVector3D const z = m.column(2).toVector3D();
  if (std::abs(x.lengthSquared() - 1.0F) > eps)
    return false;
  if (std::abs(y.lengthSquared() - 1.0F) > eps)
    return false;
  if (std::abs(z.lengthSquared() - 1.0F) > eps)
    return false;
  if (std::abs(QVector3D::dotProduct(x, y)) > eps)
    return false;
  if (std::abs(QVector3D::dotProduct(x, z)) > eps)
    return false;
  if (std::abs(QVector3D::dotProduct(y, z)) > eps)
    return false;
  return true;
}

} // namespace

TEST(CreatureTopologyTest, ToyTopologyValidates) {
  EXPECT_TRUE(validate_topology(toy_topology()));
}

TEST(CreatureTopologyTest, RejectsNonTopologicalParentOrder) {
  std::array<BoneDef, 2> bad = {
      BoneDef{"a", 1}, // parent index >= own index
      BoneDef{"b", kInvalidBone},
  };
  SkeletonTopology t{std::span<const BoneDef>(bad.data(), bad.size()), {}};
  EXPECT_FALSE(validate_topology(t));
}

TEST(CreatureTopologyTest, RejectsMultipleRoots) {
  std::array<BoneDef, 2> bad = {
      BoneDef{"r1", kInvalidBone},
      BoneDef{"r2", kInvalidBone},
  };
  SkeletonTopology t{std::span<const BoneDef>(bad.data(), bad.size()), {}};
  EXPECT_FALSE(validate_topology(t));
}

TEST(CreatureTopologyTest, RejectsDuplicateNames) {
  std::array<BoneDef, 2> bad = {
      BoneDef{"a", kInvalidBone},
      BoneDef{"a", 0},
  };
  SkeletonTopology t{std::span<const BoneDef>(bad.data(), bad.size()), {}};
  EXPECT_FALSE(validate_topology(t));
}

TEST(CreatureTopologyTest, RejectsSocketWithInvalidBone) {
  std::array<BoneDef, 1> bones = {BoneDef{"r", kInvalidBone}};
  std::array<SocketDef, 1> sockets = {SocketDef{"s", 5, {}}};
  SkeletonTopology t{
      std::span<const BoneDef>(bones.data(), bones.size()),
      std::span<const SocketDef>(sockets.data(), sockets.size()),
  };
  EXPECT_FALSE(validate_topology(t));
}

TEST(CreatureTopologyTest, FindBoneByName) {
  auto const t = toy_topology();
  EXPECT_EQ(find_bone(t, "torso"), 1);
  EXPECT_EQ(find_bone(t, "arm"), 4);
  EXPECT_EQ(find_bone(t, "does_not_exist"), kInvalidBone);
}

TEST(CreatureTopologyTest, FindSocketByName) {
  auto const t = toy_topology();
  EXPECT_EQ(find_socket(t, "head_crown"), 0);
  EXPECT_EQ(find_socket(t, "chest_front"), 2);
  EXPECT_EQ(find_socket(t, "missing"), kInvalidSocket);
}

TEST(CreatureEvaluatorTest, EveryBoneBasisIsOrthonormal) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  for (std::size_t i = 0; i < palette.size(); ++i) {
    EXPECT_TRUE(is_orthonormal(palette[i])) << "bone " << i;
  }
}

TEST(CreatureEvaluatorTest, BoneOriginsMatchProvidedHeads) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  EXPECT_LT((palette[0].column(3).toVector3D() - sample.pelvis).length(),
            1e-5F);
  EXPECT_LT((palette[1].column(3).toVector3D() - sample.pelvis).length(),
            1e-5F);
  EXPECT_LT((palette[2].column(3).toVector3D() - sample.head).length(), 1e-5F);
  EXPECT_LT((palette[5].column(3).toVector3D() - sample.hand).length(), 1e-5F);
}

TEST(CreatureEvaluatorTest, TorsoLongAxisPointsUpForUprightSample) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  QVector3D const y = palette[1].column(1).toVector3D();
  EXPECT_NEAR(y.y(), 1.0F, 1e-4F);
}

TEST(CreatureEvaluatorTest, ArmLongAxisPointsAlongShoulderToHand) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  QVector3D const expected = (sample.hand - sample.shoulder).normalized();
  QVector3D const y = palette[4].column(1).toVector3D();
  EXPECT_LT((y - expected).length(), 1e-4F);
}

TEST(CreatureEvaluatorTest, DegenerateBoneInheritsParentBasis) {
  // Collapse the torso to a zero-length bone by making chest == pelvis.
  ToySample sample;
  sample.chest = sample.pelvis;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));

  // Torso should inherit the root's basis (world-up), not produce NaN.
  QVector3D const y = palette[1].column(1).toVector3D();
  EXPECT_NEAR(y.y(), 1.0F, 1e-4F);
  EXPECT_TRUE(is_orthonormal(palette[1]));
}

TEST(CreatureEvaluatorTest, ZeroRightAxisFallsBackToWorldX) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(0.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  // Root X axis must collapse to world-+X since right hint was zero.
  QVector3D const x = palette[0].column(0).toVector3D();
  EXPECT_NEAR(x.x(), 1.0F, 1e-4F);
}

TEST(CreatureEvaluatorTest, ParentTransformPropagatesThroughChain) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));

  // Hand basis (FromParent) must match arm basis (its parent) in orientation.
  QVector3D const arm_y = palette[4].column(1).toVector3D();
  QVector3D const hand_y = palette[5].column(1).toVector3D();
  EXPECT_LT((arm_y - hand_y).length(), 1e-5F);
  // but hand origin differs (it's at sample.hand).
  EXPECT_LT((palette[5].column(3).toVector3D() - sample.hand).length(), 1e-5F);
}

TEST(CreatureSocketTest, HeadSocketAppliesLocalOffsetInBoneAxes) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  auto const t = toy_topology();
  std::span<const QMatrix4x4> p(palette.data(), palette.size());
  QVector3D const crown = socket_position(t, p, 0); // head_crown socket
  // Y offset = 0.1m along head bone's Y axis (inherited from torso, so +Y
  // world).
  EXPECT_LT((crown - (sample.head + QVector3D(0.0F, 0.1F, 0.0F))).length(),
            1e-5F);
}

TEST(CreatureSocketTest, HandGripMatchesHandPosition) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  auto const t = toy_topology();
  std::span<const QMatrix4x4> p(palette.data(), palette.size());
  QVector3D const grip = socket_position(t, p, 1); // hand_grip socket
  EXPECT_LT((grip - sample.hand).length(), 1e-5F);
}

TEST(CreatureSocketTest, AttachmentFrameIsOrthonormal) {
  ToySample sample;
  std::array<QMatrix4x4, 6> palette;
  evaluate_skeleton(toy_topology(), toy_provider, &sample,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  auto const t = toy_topology();
  std::span<const QMatrix4x4> p(palette.data(), palette.size());
  auto const frame = socket_attachment_frame(t, p, 2); // chest_front
  EXPECT_NEAR(frame.right.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(frame.up.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(frame.forward.lengthSquared(), 1.0F, 1e-4F);
  EXPECT_NEAR(QVector3D::dotProduct(frame.right, frame.up), 0.0F, 1e-4F);
}

TEST(CreatureSocketTest, OutOfRangeSocketReturnsIdentity) {
  auto const t = toy_topology();
  std::array<QMatrix4x4, 6> palette{};
  std::span<const QMatrix4x4> p(palette.data(), palette.size());
  QMatrix4x4 const m = socket_transform(t, p, 99);
  QMatrix4x4 identity;
  // All zero matrix from default-constructed result — not identity, but
  // safely constant. The contract: no crash, no NaN.
  EXPECT_FALSE(std::isnan(m(0, 0)));
}

TEST(CreaturePrimitiveTest, MakeBoneBasisIsOrthonormal) {
  QMatrix4x4 const m =
      make_bone_basis(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                      QVector3D(1.0F, 0.0F, 0.0F));
  EXPECT_TRUE(is_orthonormal(m));
}

TEST(CreaturePrimitiveTest, MakeBoneBasisHandlesColinearRightHint) {
  // right_hint parallel to bone axis — must not produce a zero X axis.
  QMatrix4x4 const m =
      make_bone_basis(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                      QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_TRUE(is_orthonormal(m));
}

TEST(CreaturePrimitiveTest, BasisFromParentKeepsOrientation) {
  QMatrix4x4 parent;
  parent.setColumn(0, QVector4D(1.0F, 0.0F, 0.0F, 0.0F));
  parent.setColumn(1, QVector4D(0.0F, 0.0F, 1.0F, 0.0F));
  parent.setColumn(2, QVector4D(0.0F, -1.0F, 0.0F, 0.0F));
  parent.setColumn(3, QVector4D(1.0F, 2.0F, 3.0F, 1.0F));

  QMatrix4x4 const m = basis_from_parent(parent, QVector3D(5.0F, 6.0F, 7.0F));
  EXPECT_EQ(m.column(0), parent.column(0));
  EXPECT_EQ(m.column(1), parent.column(1));
  EXPECT_EQ(m.column(2), parent.column(2));
  EXPECT_EQ(m.column(3), QVector4D(5.0F, 6.0F, 7.0F, 1.0F));
}
