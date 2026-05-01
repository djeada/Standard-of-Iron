

#include "render/creature/spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::CreatureSpec;
using Render::Creature::PrimitiveInstance;
using Render::GL::ISubmitter;
using Render::Humanoid::humanoid_creature_spec;
using Render::Humanoid::HumanoidBone;
using Render::Humanoid::k_bone_count;

class NullSubmitter : public ISubmitter {
public:
  std::size_t mesh_calls{0};
  std::size_t part_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
  }
  void part(Render::GL::Mesh *, Render::GL::Material *, const QMatrix4x4 &,
            const QVector3D &, Render::GL::Texture *, float, int) override {
    ++part_calls;
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

} // namespace

TEST(HumanoidSpecTest, SpeciesNameIsPopulated) {
  CreatureSpec const &s = humanoid_creature_spec();
  EXPECT_EQ(s.species_name, "humanoid");
}

TEST(HumanoidSpecTest, TopologyBoneCountMatchesEnum) {
  CreatureSpec const &s = humanoid_creature_spec();
  EXPECT_EQ(s.topology.bones.size(), k_bone_count);
  EXPECT_EQ(s.topology.bones.size(),
            static_cast<std::size_t>(HumanoidBone::Count));
}

TEST(HumanoidSpecTest, TopologyValidatesAsGenericSkeleton) {
  CreatureSpec const &s = humanoid_creature_spec();
  EXPECT_TRUE(Render::Creature::validate_topology(s.topology));
}

TEST(HumanoidSpecTest, ParentIndexIsAlwaysLessThanChild) {
  CreatureSpec const &s = humanoid_creature_spec();
  for (std::size_t i = 0; i < s.topology.bones.size(); ++i) {
    auto const parent = s.topology.bones[i].parent;
    if (parent == Render::Creature::kInvalidBone) {
      continue;
    }
    EXPECT_LT(static_cast<std::size_t>(parent), i)
        << "bone " << i << " has out-of-order parent " << parent;
  }
}

TEST(HumanoidSpecTest, RootHasNoParent) {
  CreatureSpec const &s = humanoid_creature_spec();
  ASSERT_FALSE(s.topology.bones.empty());
  EXPECT_EQ(s.topology.bones[0].parent, Render::Creature::kInvalidBone);
}

TEST(HumanoidSpecTest, FindBoneByNameAgreesWithEnumIndex) {
  CreatureSpec const &s = humanoid_creature_spec();
  auto check = [&](HumanoidBone expected, const char *name) {
    auto idx = Render::Creature::find_bone(s.topology, name);
    ASSERT_NE(idx, Render::Creature::kInvalidBone) << "no bone named " << name;
    EXPECT_EQ(idx, static_cast<std::uint16_t>(expected))
        << "mismatch for " << name;
  };
  check(HumanoidBone::Root, "Root");
  check(HumanoidBone::Pelvis, "Pelvis");
  check(HumanoidBone::Head, "Head");
  check(HumanoidBone::HandL, "HandL");
  check(HumanoidBone::HandR, "HandR");
  check(HumanoidBone::FootL, "FootL");
  check(HumanoidBone::FootR, "FootR");
}

TEST(HumanoidSpecTest, SocketsResolveToValidBones) {
  CreatureSpec const &s = humanoid_creature_spec();
  for (auto const &sock : s.topology.sockets) {
    EXPECT_LT(sock.bone, s.topology.bones.size())
        << "socket " << sock.name << " references out-of-range bone";
  }
}

TEST(HumanoidSpecTest, ValidateSpecAcceptsEmptyLodGraphs) {
  CreatureSpec const &s = humanoid_creature_spec();
  EXPECT_TRUE(Render::Creature::validate_creature_spec(s));
}

TEST(HumanoidSpecTest, BillboardLodProducesNoDraws) {
  CreatureSpec const &s = humanoid_creature_spec();

  std::array<QMatrix4x4, k_bone_count> palette;
  std::span<const QMatrix4x4> palette_view(palette);

  QMatrix4x4 identity;
  NullSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Billboard, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_EQ(stats.skipped_lod, 0U);
  EXPECT_EQ(stats.skipped_invalid, 0U);
  EXPECT_EQ(sub.mesh_calls, 0U);
  EXPECT_EQ(sub.part_calls, 0U);
}

TEST(HumanoidSpecTest, SpecReferenceIsStable) {

  auto const &a = humanoid_creature_spec();
  auto const &b = humanoid_creature_spec();
  EXPECT_EQ(&a, &b);
}

#include "render/creature/part_graph.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/humanoid/humanoid_math.h"
#include "render/humanoid/humanoid_specs.h"

#include <vector>

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::PrimitiveInstance;
using Render::GL::HumanoidPose;
using HP = Render::GL::HumanProportions;

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  struct PartCall {
    Render::GL::Mesh *mesh{nullptr};
    Render::GL::Material *material{nullptr};
    QMatrix4x4 model{};
    QVector3D color{};
    Render::GL::Texture *texture{nullptr};
    float alpha{1.0F};
    int material_id{0};
  };
  std::vector<PartCall> parts;
  std::size_t mesh_calls{0};

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {
    ++mesh_calls;
  }
  void part(Render::GL::Mesh *m, Render::GL::Material *mat,
            const QMatrix4x4 &mdl, const QVector3D &c, Render::GL::Texture *t,
            float a, int mid) override {
    parts.push_back({m, mat, mdl, c, t, a, mid});
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

auto make_upright_pose() -> HumanoidPose {
  HumanoidPose p{};
  p.pelvis_pos = QVector3D(0.0F, 1.0F, 0.0F);
  p.neck_base = QVector3D(0.0F, 1.5F, 0.0F);
  p.head_pos = QVector3D(0.0F, 1.8F, 0.0F);
  p.head_r = HP::HEAD_RADIUS;

  p.shoulder_l = QVector3D(-0.25F, 1.5F, 0.0F);
  p.shoulder_r = QVector3D(0.25F, 1.5F, 0.0F);
  p.elbow_l = QVector3D(-0.25F, 1.2F, 0.0F);
  p.elbow_r = QVector3D(0.25F, 1.2F, 0.0F);
  p.hand_l = QVector3D(-0.25F, 0.95F, 0.0F);
  p.hand_r = QVector3D(0.25F, 0.95F, 0.0F);

  p.knee_l = QVector3D(-0.15F, 0.5F, 0.0F);
  p.knee_r = QVector3D(0.15F, 0.5F, 0.0F);
  p.foot_l = QVector3D(-0.15F, 0.0F, 0.0F);
  p.foot_r = QVector3D(0.15F, 0.0F, 0.0F);
  return p;
}

auto find_primitive(std::span<const PrimitiveInstance> primitives,
                    std::string_view name) -> const PrimitiveInstance * {
  for (auto const &primitive : primitives) {
    if (!primitive.debug_name.empty() && primitive.debug_name == name) {
      return &primitive;
    }
  }
  return nullptr;
}

} // namespace

TEST(HumanoidSpecTest, MinimalLodEmitsExactlyOneCapsule) {
  CreatureSpec const &s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);
  std::span<const QMatrix4x4> palette_view(palette);

  QMatrix4x4 identity;
  RecordingSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Minimal, identity, sub);

  EXPECT_EQ(stats.submitted, 1U);
  EXPECT_EQ(stats.skipped_invalid, 0U);
  EXPECT_EQ(sub.parts.size(), 1U);
  EXPECT_EQ(sub.mesh_calls, 0U);
  ASSERT_FALSE(sub.parts.empty());
  EXPECT_EQ(sub.parts[0].mesh, Render::GL::get_unit_capsule());
}

TEST(HumanoidSpecTest, MinimalLodOtherLodsEmitNothing) {
  CreatureSpec const &s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);
  std::span<const QMatrix4x4> palette_view(palette);

  QMatrix4x4 identity;

  RecordingSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Billboard, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_TRUE(sub.parts.empty());
}

TEST(HumanoidSpecTest, FullSpecPreservesShoulderWaistTaperAndHeadHierarchy) {
  auto const &spec = humanoid_creature_spec();

  auto const *chest =
      find_primitive(spec.lod_full.primitives, "humanoid_full_chest");
  auto const *abdomen =
      find_primitive(spec.lod_full.primitives, "humanoid_full_abdomen");
  auto const *pelvis =
      find_primitive(spec.lod_full.primitives, "humanoid_full_pelvis_block");
  auto const *neck =
      find_primitive(spec.lod_full.primitives, "humanoid_full_neck");
  auto const *cranium =
      find_primitive(spec.lod_full.primitives, "humanoid_full_cranium");
  auto const *jaw =
      find_primitive(spec.lod_full.primitives, "humanoid_full_jaw");
  auto const *nose =
      find_primitive(spec.lod_full.primitives, "humanoid_full_nose");

  ASSERT_NE(chest, nullptr);
  ASSERT_NE(abdomen, nullptr);
  ASSERT_NE(pelvis, nullptr);
  ASSERT_NE(neck, nullptr);
  ASSERT_NE(cranium, nullptr);
  ASSERT_NE(jaw, nullptr);
  ASSERT_NE(nose, nullptr);

  EXPECT_EQ(pelvis->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_GT(chest->params.radius, pelvis->params.half_extents.x());
  EXPECT_GT(pelvis->params.half_extents.x(), abdomen->params.radius);
  EXPECT_GT(chest->params.depth_radius, abdomen->params.depth_radius);
  EXPECT_LT(neck->params.radius, jaw->params.half_extents.x());
  EXPECT_LT(jaw->params.half_extents.x(), cranium->params.half_extents.x());
  EXPECT_GT(nose->params.head_offset.z(), jaw->params.head_offset.z());
}

TEST(HumanoidSpecTest, FullSpecKeepsArmsAndLegsTaperedTowardExtremities) {
  auto const &spec = humanoid_creature_spec();

  auto const *upper_arm_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_upper_arm_l_top");
  auto const *upper_arm_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_upper_arm_l_bot");
  auto const *forearm_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_forearm_l_top");
  auto const *forearm_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_forearm_l_bot");
  auto const *thigh_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_thigh_l_top");
  auto const *thigh_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_thigh_l_bot");
  auto const *calf_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_calf_l_top");
  auto const *calf_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_calf_l_bot");
  auto const *knee =
      find_primitive(spec.lod_full.primitives, "humanoid_full_knee_l");
  auto const *ankle =
      find_primitive(spec.lod_full.primitives, "humanoid_full_ankle_l");
  auto const *foot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_foot_l");

  ASSERT_NE(upper_arm_top, nullptr);
  ASSERT_NE(upper_arm_bot, nullptr);
  ASSERT_NE(forearm_top, nullptr);
  ASSERT_NE(forearm_bot, nullptr);
  ASSERT_NE(thigh_top, nullptr);
  ASSERT_NE(thigh_bot, nullptr);
  ASSERT_NE(calf_top, nullptr);
  ASSERT_NE(calf_bot, nullptr);
  ASSERT_NE(knee, nullptr);
  ASSERT_NE(ankle, nullptr);
  ASSERT_NE(foot, nullptr);

  EXPECT_EQ(foot->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_GT(upper_arm_top->params.radius, upper_arm_bot->params.radius);
  EXPECT_GT(forearm_top->params.radius, forearm_bot->params.radius);
  EXPECT_GT(upper_arm_bot->params.radius, forearm_bot->params.radius);
  EXPECT_GT(thigh_top->params.radius, thigh_bot->params.radius);
  EXPECT_GT(calf_top->params.radius, calf_bot->params.radius);
  EXPECT_GT(thigh_bot->params.radius, calf_bot->params.radius);
  EXPECT_GT(knee->params.radius, ankle->params.radius);
  EXPECT_GT(foot->params.half_extents.z(), foot->params.half_extents.x());
}

TEST(HumanoidSpecTest, MinimalLodMatchesLegacyCapsuleEndpointsInUprightPose) {
  CreatureSpec const &s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);
  std::span<const QMatrix4x4> palette_view(palette);

  QMatrix4x4 identity;
  RecordingSubmitter sub;
  Render::Creature::submit_creature(s, palette_view, CreatureLOD::Minimal,
                                    identity, sub);
  ASSERT_EQ(sub.parts.size(), 1U);
  QMatrix4x4 const &v2_model = sub.parts[0].model;

  QVector3D const expected_top =
      pose.head_pos + QVector3D(0.0F, HP::HEAD_RADIUS, 0.0F);
  QVector3D const expected_bot = pose.foot_l;
  QMatrix4x4 const expected_model = Render::Geom::capsule_between(
      identity, expected_top, expected_bot, HP::TORSO_TOP_R);

  const float *a = v2_model.constData();
  const float *b = expected_model.constData();
  for (int i = 0; i < 16; ++i) {
    EXPECT_NEAR(a[i], b[i], 1.0e-4F) << "model[" << i << "] mismatch";
  }
}

TEST(HumanoidSpecTest, MinimalLodTopEndpointIsHeadCrownInUprightPose) {

  CreatureSpec const &s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);
  std::span<const QMatrix4x4> palette_view(palette);

  QMatrix4x4 identity;
  RecordingSubmitter sub;
  Render::Creature::submit_creature(s, palette_view, CreatureLOD::Minimal,
                                    identity, sub);
  ASSERT_EQ(sub.parts.size(), 1U);
  QMatrix4x4 const &v2_model = sub.parts[0].model;

  QVector3D const ep_a = v2_model.map(QVector3D(0.0F, 0.5F, 0.0F));
  QVector3D const ep_b = v2_model.map(QVector3D(0.0F, -0.5F, 0.0F));
  QVector3D const expected_head_crown =
      pose.head_pos + QVector3D(0.0F, HP::HEAD_RADIUS, 0.0F);
  QVector3D const expected_foot = pose.foot_l;

  auto near = [](const QVector3D &a, const QVector3D &b) {
    return (a - b).lengthSquared() < 1.0e-7F;
  };
  bool const matched =
      (near(ep_a, expected_head_crown) && near(ep_b, expected_foot)) ||
      (near(ep_b, expected_head_crown) && near(ep_a, expected_foot));
  EXPECT_TRUE(matched) << "Endpoints (" << ep_a.x() << "," << ep_a.y() << ","
                       << ep_a.z() << ") and (" << ep_b.x() << "," << ep_b.y()
                       << "," << ep_b.z() << ") don't cover head_crown=("
                       << expected_head_crown.x() << ","
                       << expected_head_crown.y() << ","
                       << expected_head_crown.z() << ") and foot=("
                       << expected_foot.x() << "," << expected_foot.y() << ","
                       << expected_foot.z() << ")";
}

TEST(HumanoidSpecTest, MinimalLodRespectsWorldFromUnit) {

  CreatureSpec const &s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);
  std::span<const QMatrix4x4> palette_view(palette);

  RecordingSubmitter base_sub;
  QMatrix4x4 identity;
  Render::Creature::submit_creature(s, palette_view, CreatureLOD::Minimal,
                                    identity, base_sub);
  ASSERT_EQ(base_sub.parts.size(), 1U);

  QMatrix4x4 world;
  world.translate(10.0F, 0.0F, 0.0F);
  RecordingSubmitter moved_sub;
  Render::Creature::submit_creature(s, palette_view, CreatureLOD::Minimal,
                                    world, moved_sub);
  ASSERT_EQ(moved_sub.parts.size(), 1U);

  for (float y : {0.5F, -0.5F}) {
    QVector3D const base =
        base_sub.parts[0].model.map(QVector3D(0.0F, y, 0.0F));
    QVector3D const moved =
        moved_sub.parts[0].model.map(QVector3D(0.0F, y, 0.0F));
    EXPECT_NEAR(moved.x() - base.x(), 10.0F, 1.0e-4F);
    EXPECT_NEAR(moved.y() - base.y(), 0.0F, 1.0e-4F);
    EXPECT_NEAR(moved.z() - base.z(), 0.0F, 1.0e-4F);
  }
}
