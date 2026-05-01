

#include "render/creature/part_graph.h"
#include "render/creature/skeleton.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

namespace {

using namespace Render::Creature;
using Render::GL::ISubmitter;
using Render::GL::Material;
using Render::GL::Mesh;
using Render::GL::Texture;

struct RecordedCall {
  enum class Kind { Mesh, Part };
  Kind kind{Kind::Mesh};
  Mesh *mesh{nullptr};
  Material *material{nullptr};
  QMatrix4x4 model;
  QVector3D color;
  float alpha{1.0F};
  int material_id{0};
};

class RecordingSubmitter : public ISubmitter {
public:
  std::vector<RecordedCall> calls;

  void mesh(Mesh *m, const QMatrix4x4 &model, const QVector3D &color, Texture *,
            float alpha, int mat_id) override {
    calls.push_back(
        {RecordedCall::Kind::Mesh, m, nullptr, model, color, alpha, mat_id});
  }
  void part(Mesh *m, Material *mat, const QMatrix4x4 &model,
            const QVector3D &color, Texture *tex, float alpha,
            int mat_id) override {
    if (mat == nullptr) {
      this->mesh(m, model, color, tex, alpha, mat_id);
      return;
    }
    calls.push_back(
        {RecordedCall::Kind::Part, m, mat, model, color, alpha, mat_id});
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

constexpr std::array<BoneDef, 5> kBeastBones = {
    BoneDef{"root", kInvalidBone},
    BoneDef{"spine_base", 0},
    BoneDef{"neck", 1},
    BoneDef{"head", 2},
    BoneDef{"leg_foot", 0},
};

auto beast_topology() noexcept -> SkeletonTopology {
  return SkeletonTopology{
      std::span<const BoneDef>(kBeastBones.data(), kBeastBones.size()),
      {},
  };
}

struct BeastSample {};

BoneResolution beast_provider(void *, BoneIndex bone) noexcept {
  BoneResolution r;
  switch (bone) {
  case 0:
    r.kind = BoneBasisKind::FromRootUp;
    r.head = {0.0F, 0.0F, 0.0F};
    break;
  case 1:
    r.kind = BoneBasisKind::FromParent;
    r.head = {0.0F, 0.9F, 0.0F};
    break;
  case 2:
    r.kind = BoneBasisKind::FromParent;
    r.head = {0.0F, 1.5F, 0.0F};
    break;
  case 3:
    r.kind = BoneBasisKind::FromParent;
    r.head = {0.0F, 1.7F, 0.0F};
    break;
  case 4:
    r.kind = BoneBasisKind::FromParent;
    r.head = {0.3F, 0.0F, 0.0F};
    break;
  default:
    break;
  }
  return r;
}

std::array<QMatrix4x4, 5> evaluate_beast() {
  BeastSample s;
  std::array<QMatrix4x4, 5> palette;
  evaluate_skeleton(beast_topology(), &beast_provider, &s,
                    QVector3D(1.0F, 0.0F, 0.0F),
                    std::span<QMatrix4x4>(palette));
  return palette;
}

PrimitiveInstance make_head_sphere() {
  PrimitiveInstance p;
  p.debug_name = "head";
  p.shape = PrimitiveShape::Sphere;
  p.params.anchor_bone = 3;
  p.params.radius = 0.15F;
  p.color = {1.0F, 0.8F, 0.6F};
  return p;
}

PrimitiveInstance make_torso_cylinder() {
  PrimitiveInstance p;
  p.debug_name = "torso";
  p.shape = PrimitiveShape::Cylinder;
  p.params.anchor_bone = 0;
  p.params.tail_bone = 1;
  p.params.radius = 0.25F;
  p.color = {0.2F, 0.4F, 0.6F};
  return p;
}

PrimitiveInstance make_leg_capsule() {
  PrimitiveInstance p;
  p.debug_name = "leg";
  p.shape = PrimitiveShape::Capsule;
  p.params.anchor_bone = 0;
  p.params.tail_bone = 4;
  p.params.radius = 0.08F;
  p.color = {0.3F, 0.2F, 0.1F};
  return p;
}

PrimitiveInstance make_horn_cone() {
  PrimitiveInstance p;
  p.debug_name = "horn";
  p.shape = PrimitiveShape::Cone;
  p.params.anchor_bone = 2;
  p.params.tail_bone = 3;
  p.params.radius = 0.05F;
  p.color = {1.0F, 1.0F, 1.0F};
  return p;
}

PrimitiveInstance make_chest_box() {
  PrimitiveInstance p;
  p.debug_name = "chestplate";
  p.shape = PrimitiveShape::Box;
  p.params.anchor_bone = 1;
  p.params.head_offset = {0.0F, -0.2F, 0.1F};
  p.params.half_extents = {0.3F, 0.2F, 0.05F};
  p.color = {0.5F, 0.5F, 0.5F};
  return p;
}

} // namespace

TEST(PartGraphWalkerTest, ValidateRejectsInvalidBoneReference) {
  std::array<PrimitiveInstance, 1> primitives;
  primitives[0].shape = PrimitiveShape::Sphere;
  primitives[0].params.anchor_bone = 99;

  PartGraph g{std::span<const PrimitiveInstance>(primitives.data(), 1)};
  EXPECT_FALSE(validate_part_graph(beast_topology(), g));
}

TEST(PartGraphWalkerTest, ValidateRejectsCylinderMissingTail) {
  std::array<PrimitiveInstance, 1> primitives;
  primitives[0].shape = PrimitiveShape::Cylinder;
  primitives[0].params.anchor_bone = 0;
  primitives[0].params.tail_bone = kInvalidBone;

  PartGraph g{std::span<const PrimitiveInstance>(primitives.data(), 1)};
  EXPECT_FALSE(validate_part_graph(beast_topology(), g));
}

TEST(PartGraphWalkerTest, ValidateAcceptsWellFormedGraph) {
  std::array<PrimitiveInstance, 4> primitives = {
      make_head_sphere(),
      make_torso_cylinder(),
      make_leg_capsule(),
      make_chest_box(),
  };
  PartGraph g{
      std::span<const PrimitiveInstance>(primitives.data(), primitives.size())};
  EXPECT_TRUE(validate_part_graph(beast_topology(), g));
}

TEST(PartGraphWalkerTest, CylinderUsesCustomMeshOverrideWhenProvided) {
  auto palette = evaluate_beast();
  PrimitiveInstance p = make_torso_cylinder();
  p.custom_mesh = Render::GL::get_unit_cube();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_cube());
}

TEST(PartGraphWalkerTest, SubmitsOneDrawPerPrimitiveAtLodFull) {
  auto palette = evaluate_beast();
  std::array<PrimitiveInstance, 4> primitives = {
      make_head_sphere(),
      make_torso_cylinder(),
      make_leg_capsule(),
      make_chest_box(),
  };
  PartGraph g{
      std::span<const PrimitiveInstance>(primitives.data(), primitives.size())};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);

  EXPECT_EQ(stats.submitted, 4U);
  EXPECT_EQ(stats.skipped_lod, 0U);
  EXPECT_EQ(stats.skipped_invalid, 0U);
  EXPECT_EQ(sub.calls.size(), 4U);
}

TEST(PartGraphWalkerTest, LodMaskFiltersPrimitives) {
  auto palette = evaluate_beast();

  auto only_full = make_head_sphere();
  only_full.lod_mask = kLodFull;

  auto all = make_torso_cylinder();
  all.lod_mask = kLodAll;

  auto minimal_only = make_chest_box();
  minimal_only.lod_mask = kLodMinimal;

  std::array<PrimitiveInstance, 3> primitives = {only_full, all, minimal_only};
  PartGraph g{std::span<const PrimitiveInstance>(primitives.data(), 3)};

  {
    RecordingSubmitter sub;
    QMatrix4x4 identity;
    auto stats = submit_part_graph(beast_topology(), g,
                                   std::span<const QMatrix4x4>(palette),
                                   CreatureLOD::Full, identity, sub);
    EXPECT_EQ(stats.submitted, 2U);
    EXPECT_EQ(stats.skipped_lod, 1U);
  }

  {
    RecordingSubmitter sub;
    QMatrix4x4 identity;
    auto stats = submit_part_graph(beast_topology(), g,
                                   std::span<const QMatrix4x4>(palette),
                                   CreatureLOD::Minimal, identity, sub);
    EXPECT_EQ(stats.submitted, 2U);
    EXPECT_EQ(stats.skipped_lod, 1U);
  }

  {
    RecordingSubmitter sub;
    QMatrix4x4 identity;
    auto stats = submit_part_graph(beast_topology(), g,
                                   std::span<const QMatrix4x4>(palette),
                                   CreatureLOD::Billboard, identity, sub);
    EXPECT_EQ(stats.submitted, 1U);
    EXPECT_EQ(stats.skipped_lod, 2U);
  }
}

TEST(PartGraphWalkerTest, SphereModelOriginMatchesBoneWithOffset) {
  auto palette = evaluate_beast();
  auto p = make_head_sphere();
  p.params.head_offset = {0.0F, 0.3F, 0.0F};
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);

  QVector3D const origin = sub.calls[0].model.column(3).toVector3D();

  EXPECT_LT((origin - QVector3D(0.0F, 2.0F, 0.0F)).length(), 1e-4F);
}

TEST(PartGraphWalkerTest, CylinderSpansBetweenBoneOrigins) {
  auto palette = evaluate_beast();
  auto p = make_torso_cylinder();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);

  QMatrix4x4 const m = sub.calls[0].model;
  QVector3D const bottom = m.map(QVector3D(0.0F, -0.5F, 0.0F));
  QVector3D const top = m.map(QVector3D(0.0F, 0.5F, 0.0F));
  EXPECT_LT((bottom - QVector3D(0.0F, 0.0F, 0.0F)).length(), 1e-3F);
  EXPECT_LT((top - QVector3D(0.0F, 0.9F, 0.0F)).length(), 1e-3F);
}

TEST(PartGraphWalkerTest, WorldFromUnitMatrixIsAppliedOnTheLeft) {
  auto palette = evaluate_beast();
  auto p = make_head_sphere();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  QMatrix4x4 world_from_unit;
  world_from_unit.translate(10.0F, 20.0F, 30.0F);

  RecordingSubmitter sub;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, world_from_unit, sub);
  ASSERT_EQ(sub.calls.size(), 1U);

  QVector3D const origin = sub.calls[0].model.column(3).toVector3D();

  EXPECT_LT((origin - QVector3D(10.0F, 21.7F, 30.0F)).length(), 1e-3F);
}

TEST(PartGraphWalkerTest, InvalidAnchorIsSkippedAndCounted) {
  auto palette = evaluate_beast();
  auto p = make_head_sphere();
  p.params.anchor_bone = 99;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_EQ(stats.skipped_invalid, 1U);
  EXPECT_TRUE(sub.calls.empty());
}

TEST(PartGraphWalkerTest, MeshShapeWithoutCustomMeshIsSkipped) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::Mesh;
  p.params.anchor_bone = 0;
  p.custom_mesh = nullptr;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_EQ(stats.skipped_invalid, 1U);
}

TEST(PartGraphWalkerTest, MeshShapeWithCustomMeshIsSubmitted) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::Mesh;
  p.params.anchor_bone = 0;
  p.params.half_extents = {2.0F, 2.0F, 2.0F};
  p.custom_mesh = Render::GL::get_unit_cube();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);
  EXPECT_EQ(stats.submitted, 1U);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_cube());
}

TEST(PartGraphWalkerTest, ColorAndAlphaAreForwarded) {
  auto palette = evaluate_beast();
  auto p = make_head_sphere();
  p.color = {0.1F, 0.2F, 0.3F};
  p.alpha = 0.75F;
  p.material_id = 42;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_LT((sub.calls[0].color - QVector3D(0.1F, 0.2F, 0.3F)).length(), 1e-6F);
  EXPECT_NEAR(sub.calls[0].alpha, 0.75F, 1e-6F);
  EXPECT_EQ(sub.calls[0].material_id, 42);
}

TEST(PartGraphWalkerTest, EmptyGraphReturnsZeroStats) {
  auto palette = evaluate_beast();
  PartGraph g{};
  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_EQ(stats.skipped_lod, 0U);
  EXPECT_EQ(stats.skipped_invalid, 0U);
}

TEST(PartGraphWalkerTest, BoxModelRespectsBoneLocalOffsetAndScale) {
  auto palette = evaluate_beast();
  auto p = make_chest_box();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);

  QMatrix4x4 const m = sub.calls[0].model;

  QVector3D const origin = m.column(3).toVector3D();
  EXPECT_LT((origin - QVector3D(0.0F, 0.7F, 0.1F)).length(), 1e-3F);

  QVector3D const corner = m.map(QVector3D(0.5F, 0.5F, 0.5F));
  QVector3D const expected = origin + QVector3D(0.3F, 0.2F, 0.05F);
  EXPECT_LT((corner - expected).length(), 1e-3F);
}

TEST(PartGraphWalkerTest, ConeUsesUnitConeMesh) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::Cone;
  p.params.anchor_bone = 0;
  p.params.tail_bone = 1;
  p.params.radius = 0.3F;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_cone());
}

TEST(PartGraphWalkerTest, CapsuleUsesUnitCapsuleMesh) {
  auto palette = evaluate_beast();
  auto p = make_leg_capsule();
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_capsule());
}

TEST(PartGraphWalkerTest, OrientedCylinderRequiresTailAndRejectsWhenMissing) {
  PrimitiveInstance p;
  p.shape = PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = 0;
  p.params.tail_bone = kInvalidBone;
  p.params.radius = 0.3F;
  p.params.depth_radius = 0.1F;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};
  EXPECT_FALSE(validate_part_graph(beast_topology(), g));
}

TEST(PartGraphWalkerTest,
     OrientedCylinderSpansEndpointsAndUsesEllipticalRadii) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = 0;
  p.params.tail_bone = 1;
  p.params.radius = 0.4F;
  p.params.depth_radius = 0.1F;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  auto stats = submit_part_graph(beast_topology(), g,
                                 std::span<const QMatrix4x4>(palette),
                                 CreatureLOD::Full, identity, sub);
  EXPECT_EQ(stats.submitted, 1U);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_cylinder());

  QMatrix4x4 const m = sub.calls[0].model;
  QVector3D const bottom = m.map(QVector3D(0.0F, -0.5F, 0.0F));
  QVector3D const top = m.map(QVector3D(0.0F, 0.5F, 0.0F));
  auto near = [](const QVector3D &a, const QVector3D &b) {
    return (a - b).length() < 1e-3F;
  };
  EXPECT_TRUE((near(bottom, QVector3D(0.0F, 0.0F, 0.0F)) &&
               near(top, QVector3D(0.0F, 0.9F, 0.0F))) ||
              (near(bottom, QVector3D(0.0F, 0.9F, 0.0F)) &&
               near(top, QVector3D(0.0F, 0.0F, 0.0F))));

  float const rx = m.column(0).toVector3D().length();
  float const rz = m.column(2).toVector3D().length();
  EXPECT_NEAR(rx, 0.4F, 1e-4F);
  EXPECT_NEAR(rz, 0.1F, 1e-4F);
}

TEST(PartGraphWalkerTest,
     OrientedCylinderZeroDepthFallsBackToCircularCrossSection) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = 0;
  p.params.tail_bone = 1;
  p.params.radius = 0.25F;
  p.params.depth_radius = 0.0F;
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  QMatrix4x4 const m = sub.calls[0].model;
  float const rx = m.column(0).toVector3D().length();
  float const rz = m.column(2).toVector3D().length();
  EXPECT_NEAR(rx, 0.25F, 1e-4F);
  EXPECT_NEAR(rz, 0.25F, 1e-4F);
}

TEST(PartGraphWalkerTest, OrientedSphereBuildsEllipsoidInBoneBasis) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = 3;
  p.params.half_extents = {0.2F, 0.3F, 0.15F};
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  RecordingSubmitter sub;
  QMatrix4x4 identity;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, identity, sub);
  ASSERT_EQ(sub.calls.size(), 1U);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_sphere());

  QMatrix4x4 const m = sub.calls[0].model;

  EXPECT_NEAR(m.column(0).toVector3D().length(), 0.4F, 1e-4F);
  EXPECT_NEAR(m.column(1).toVector3D().length(), 0.6F, 1e-4F);
  EXPECT_NEAR(m.column(2).toVector3D().length(), 0.3F, 1e-4F);

  EXPECT_LT((m.column(3).toVector3D() - QVector3D(0.0F, 1.7F, 0.0F)).length(),
            1e-4F);
}

TEST(PartGraphWalkerTest,
     OrientedSphereRespectsHeadOffsetAndWorldFromUnitMatrix) {
  auto palette = evaluate_beast();
  PrimitiveInstance p;
  p.shape = PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = 3;
  p.params.head_offset = {0.1F, 0.2F, 0.3F};
  p.params.half_extents = {0.1F, 0.1F, 0.1F};
  std::array<PrimitiveInstance, 1> prims = {p};
  PartGraph g{std::span<const PrimitiveInstance>(prims.data(), 1)};

  QMatrix4x4 world_from_unit;
  world_from_unit.translate(100.0F, 0.0F, 0.0F);
  RecordingSubmitter sub;
  submit_part_graph(beast_topology(), g, std::span<const QMatrix4x4>(palette),
                    CreatureLOD::Full, world_from_unit, sub);
  ASSERT_EQ(sub.calls.size(), 1U);

  QVector3D const origin = sub.calls[0].model.column(3).toVector3D();

  EXPECT_LT((origin - QVector3D(100.1F, 1.9F, 0.3F)).length(), 1e-3F);
}
