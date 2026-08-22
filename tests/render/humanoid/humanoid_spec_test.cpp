

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string_view>

#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/primitive_geometry.h"
#include "render/creature/spec.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/submitter.h"

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::CreatureSpec;
using Render::Creature::PrimitiveInstance;
using Render::GL::ISubmitter;
using Render::Humanoid::humanoid_creature_spec;
using Render::Humanoid::HumanoidBone;
using Render::Humanoid::k_bone_count;
using Render::Humanoid::skeleton_humanoid_creature_spec;

class NullSubmitter : public ISubmitter {
public:
  std::size_t mesh_calls{0};
  std::size_t part_calls{0};

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++mesh_calls;
  }
  void part(Render::GL::Mesh*,
            Render::GL::Material*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++part_calls;
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

} // namespace

TEST(HumanoidSpecTest, SpeciesNameIsPopulated) {
  CreatureSpec const& s = humanoid_creature_spec();
  EXPECT_EQ(s.species_name, "humanoid");
}

TEST(HumanoidSpecTest, TopologyBoneCountMatchesEnum) {
  CreatureSpec const& s = humanoid_creature_spec();
  EXPECT_EQ(s.topology.bones.size(), k_bone_count);
  EXPECT_EQ(s.topology.bones.size(), static_cast<std::size_t>(HumanoidBone::Count));
}

TEST(HumanoidSpecTest, TopologyValidatesAsGenericSkeleton) {
  CreatureSpec const& s = humanoid_creature_spec();
  EXPECT_TRUE(Render::Creature::validate_topology(s.topology));
}

TEST(HumanoidSpecTest, ParentIndexIsAlwaysLessThanChild) {
  CreatureSpec const& s = humanoid_creature_spec();
  for (std::size_t i = 0; i < s.topology.bones.size(); ++i) {
    auto const parent = s.topology.bones[i].parent;
    if (parent == Render::Creature::k_invalid_bone) {
      continue;
    }
    EXPECT_LT(static_cast<std::size_t>(parent), i)
        << "bone " << i << " has out-of-order parent " << parent;
  }
}

TEST(HumanoidSpecTest, RootHasNoParent) {
  CreatureSpec const& s = humanoid_creature_spec();
  ASSERT_FALSE(s.topology.bones.empty());
  EXPECT_EQ(s.topology.bones[0].parent, Render::Creature::k_invalid_bone);
}

TEST(HumanoidSpecTest, FindBoneByNameAgreesWithEnumIndex) {
  CreatureSpec const& s = humanoid_creature_spec();
  auto check = [&](HumanoidBone expected, const char* name) {
    auto idx = Render::Creature::find_bone(s.topology, name);
    ASSERT_NE(idx, Render::Creature::k_invalid_bone) << "no bone named " << name;
    EXPECT_EQ(idx, static_cast<std::uint16_t>(expected)) << "mismatch for " << name;
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
  CreatureSpec const& s = humanoid_creature_spec();
  for (auto const& sock : s.topology.sockets) {
    EXPECT_LT(sock.bone, s.topology.bones.size())
        << "socket " << sock.name << " references out-of-range bone";
  }
}

TEST(HumanoidSpecTest, ValidateSpecAcceptsEmptyLodGraphs) {
  CreatureSpec const& s = humanoid_creature_spec();
  EXPECT_TRUE(Render::Creature::validate_creature_spec(s));
}

TEST(HumanoidSpecTest, BillboardLodProducesNoDraws) {
  CreatureSpec const& s = humanoid_creature_spec();

  std::array<QMatrix4x4, k_bone_count> palette;
  std::span<const QMatrix4x4> const palette_view(palette);

  QMatrix4x4 const identity;
  NullSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Culled, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_EQ(stats.skipped_lod, 0U);
  EXPECT_EQ(stats.skipped_invalid, 0U);
  EXPECT_EQ(sub.mesh_calls, 0U);
  EXPECT_EQ(sub.part_calls, 0U);
}

TEST(HumanoidSpecTest, SpecReferenceIsStable) {

  auto const& a = humanoid_creature_spec();
  auto const& b = humanoid_creature_spec();
  EXPECT_EQ(&a, &b);
}

#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/creature/part_graph.h"
#include "render/geom/transforms.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/humanoid/runtime/humanoid_math.h"

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::PrimitiveInstance;
using Render::GL::HumanoidPose;
using HP = Render::GL::HumanProportions;

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  struct PartCall {
    Render::GL::Mesh* mesh{nullptr};
    Render::GL::Material* material{nullptr};
    QMatrix4x4 model;
    QVector3D color;
    Render::GL::Texture* texture{nullptr};
    float alpha{1.0F};
    int material_id{0};
  };
  std::vector<PartCall> parts;
  std::size_t mesh_calls{0};

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++mesh_calls;
  }
  void part(Render::GL::Mesh* m,
            Render::GL::Material* mat,
            const QMatrix4x4& mdl,
            const QVector3D& c,
            Render::GL::Texture* t,
            float a,
            int mid) override {
    parts.push_back({m, mat, mdl, c, t, a, mid});
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
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
                    std::string_view name) -> const PrimitiveInstance* {
  for (auto const& primitive : primitives) {
    if (!primitive.debug_name.empty() && primitive.debug_name == name) {
      return &primitive;
    }
  }
  return nullptr;
}

} // namespace

TEST(HumanoidSpecTest, MinimalLodDrawsAWholeBody) {
  CreatureSpec const& s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  std::span<const QMatrix4x4> const palette_view(palette);

  QMatrix4x4 const identity;
  RecordingSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Minimal, identity, sub);

  EXPECT_EQ(stats.skipped_invalid, 0U);
  EXPECT_EQ(stats.submitted, s.lod_minimal.primitives.size());
  EXPECT_EQ(sub.parts.size(), s.lod_minimal.primitives.size());
  EXPECT_EQ(sub.mesh_calls, 0U);

  for (auto const* name : {"humanoid_full_cranium",
                           "humanoid_full_chest",
                           "humanoid_full_pelvis_block",
                           "humanoid_minimal_upper_arm_l",
                           "humanoid_minimal_upper_arm_r",
                           "humanoid_minimal_forearm_l",
                           "humanoid_minimal_forearm_r",
                           "humanoid_minimal_thigh_l",
                           "humanoid_minimal_thigh_r",
                           "humanoid_minimal_calf_l",
                           "humanoid_minimal_calf_r",
                           "humanoid_full_foot_l",
                           "humanoid_full_foot_r"}) {
    EXPECT_NE(find_primitive(s.lod_minimal.primitives, name), nullptr)
        << "minimal body is missing " << name;
  }
}

TEST(HumanoidSpecTest, MinimalLodIsCheaperThanTheFullBody) {
  CreatureSpec const& s = humanoid_creature_spec();

  EXPECT_LT(s.lod_minimal.primitives.size(), s.lod_full.primitives.size());

  auto const* chest = find_primitive(s.lod_minimal.primitives, "humanoid_full_chest");
  ASSERT_NE(chest, nullptr);
  auto* full_mesh = Render::Creature::primitive_unit_mesh(*chest, CreatureLOD::Full);
  auto* minimal_mesh =
      Render::Creature::primitive_unit_mesh(*chest, CreatureLOD::Minimal);
  ASSERT_NE(full_mesh, nullptr);
  ASSERT_NE(minimal_mesh, nullptr);
  EXPECT_NE(full_mesh, minimal_mesh);
  EXPECT_LT(minimal_mesh->get_indices().size(), full_mesh->get_indices().size());
}

TEST(HumanoidSpecTest, MinimalLodOtherLodsEmitNothing) {
  CreatureSpec const& s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  std::span<const QMatrix4x4> const palette_view(palette);

  QMatrix4x4 const identity;

  RecordingSubmitter sub;
  auto stats = Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Culled, identity, sub);
  EXPECT_EQ(stats.submitted, 0U);
  EXPECT_TRUE(sub.parts.empty());
}

TEST(HumanoidSpecTest, FullSpecPreservesShoulderWaistTaperAndHeadHierarchy) {
  auto const& spec = humanoid_creature_spec();

  auto const* chest = find_primitive(spec.lod_full.primitives, "humanoid_full_chest");
  auto const* abdomen =
      find_primitive(spec.lod_full.primitives, "humanoid_full_abdomen");
  auto const* pelvis =
      find_primitive(spec.lod_full.primitives, "humanoid_full_pelvis_block");
  auto const* neck = find_primitive(spec.lod_full.primitives, "humanoid_full_neck");
  auto const* cranium =
      find_primitive(spec.lod_full.primitives, "humanoid_full_cranium");
  auto const* jaw = find_primitive(spec.lod_full.primitives, "humanoid_full_jaw");
  auto const* nose = find_primitive(spec.lod_full.primitives, "humanoid_full_nose");

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
  auto const& spec = humanoid_creature_spec();

  auto const* upper_arm_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_upper_arm_l_top");
  auto const* upper_arm_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_upper_arm_l_bot");
  auto const* forearm_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_forearm_l_top");
  auto const* forearm_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_forearm_l_bot");
  auto const* thigh_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_thigh_l_top");
  auto const* thigh_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_thigh_l_bot");
  auto const* calf_top =
      find_primitive(spec.lod_full.primitives, "humanoid_full_calf_l_top");
  auto const* calf_bot =
      find_primitive(spec.lod_full.primitives, "humanoid_full_calf_l_bot");
  auto const* knee = find_primitive(spec.lod_full.primitives, "humanoid_full_knee_l");
  auto const* ankle = find_primitive(spec.lod_full.primitives, "humanoid_full_ankle_l");
  auto const* foot = find_primitive(spec.lod_full.primitives, "humanoid_full_foot_l");

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

  auto const distal_radius = [](const Render::Creature::PrimitiveInstance* prim) {
    return prim->params.tail_radius > 0.0F ? prim->params.tail_radius
                                           : prim->params.radius;
  };

  EXPECT_EQ(foot->shape, Render::Creature::PrimitiveShape::OrientedSphere);

  for (auto const* segment : {upper_arm_top,
                              upper_arm_bot,
                              forearm_top,
                              forearm_bot,
                              thigh_top,
                              thigh_bot,
                              calf_top,
                              calf_bot}) {
    EXPECT_EQ(segment->shape, Render::Creature::PrimitiveShape::TaperedCylinder);
    EXPECT_GT(segment->params.tail_radius, 0.0F);
  }

  EXPECT_GT(upper_arm_top->params.radius, distal_radius(upper_arm_bot));
  EXPECT_GT(forearm_top->params.radius, distal_radius(forearm_bot));
  EXPECT_GT(thigh_top->params.radius, distal_radius(thigh_bot));
  EXPECT_GT(calf_top->params.radius, distal_radius(calf_bot));

  EXPECT_FLOAT_EQ(distal_radius(upper_arm_top), upper_arm_bot->params.radius);
  EXPECT_FLOAT_EQ(distal_radius(forearm_top), forearm_bot->params.radius);
  EXPECT_FLOAT_EQ(distal_radius(thigh_top), thigh_bot->params.radius);
  EXPECT_FLOAT_EQ(distal_radius(calf_top), calf_bot->params.radius);

  EXPECT_GT(distal_radius(upper_arm_bot), distal_radius(forearm_bot));
  EXPECT_GT(distal_radius(thigh_bot), distal_radius(calf_bot));
  EXPECT_GT(knee->params.radius, ankle->params.radius);
  EXPECT_GT(foot->params.half_extents.z(), foot->params.half_extents.x());
}

TEST(HumanoidSpecTest, SkeletonSpecReusesHumanoidTopologyWithBoneGraph) {
  auto const& base = humanoid_creature_spec();
  auto const& skeleton = skeleton_humanoid_creature_spec();

  EXPECT_EQ(skeleton.species_name, "skeleton_humanoid");
  EXPECT_EQ(skeleton.topology.bones.size(), base.topology.bones.size());
  EXPECT_TRUE(Render::Creature::validate_creature_spec(skeleton));
  EXPECT_GT(skeleton.lod_full.primitives.size(), base.lod_full.primitives.size());
  EXPECT_GT(skeleton.lod_minimal.primitives.size(), 8U);

  auto const* minimal_spine =
      find_primitive(skeleton.lod_minimal.primitives, "skeleton_minimal_spine");
  auto const* minimal_rib =
      find_primitive(skeleton.lod_minimal.primitives, "skeleton_minimal_rib_l_high");
  auto const* minimal_skull =
      find_primitive(skeleton.lod_minimal.primitives, "skeleton_minimal_skull");

  auto const* spine = find_primitive(skeleton.lod_full.primitives, "skeleton_spine");
  auto const* upper_spine =
      find_primitive(skeleton.lod_full.primitives, "skeleton_upper_spine");
  auto const* high_rib =
      find_primitive(skeleton.lod_full.primitives, "skeleton_rib_l_high");
  auto const* rib = find_primitive(skeleton.lod_full.primitives, "skeleton_rib_l_mid");
  auto const* low_rib =
      find_primitive(skeleton.lod_full.primitives, "skeleton_rib_l_floating");
  auto const* neck_base =
      find_primitive(skeleton.lod_full.primitives, "skeleton_neck_base");
  auto const* neck = find_primitive(skeleton.lod_full.primitives, "skeleton_neck");
  auto const* sternum =
      find_primitive(skeleton.lod_full.primitives, "skeleton_sternum");
  auto const* ribcage_blob =
      find_primitive(skeleton.lod_full.primitives, "skeleton_ribcage_front_l");
  auto const* sternum_blob =
      find_primitive(skeleton.lod_full.primitives, "skeleton_sternum_top");
  auto const* skull = find_primitive(skeleton.lod_full.primitives, "skeleton_skull");
  auto const* eye =
      find_primitive(skeleton.lod_full.primitives, "skeleton_eye_socket_l");
  auto const* nasal_cavity =
      find_primitive(skeleton.lod_full.primitives, "skeleton_nasal_cavity");
  auto const* cheek = find_primitive(skeleton.lod_full.primitives, "skeleton_cheek_l");
  auto const* thigh = find_primitive(skeleton.lod_full.primitives, "skeleton_thigh_l");

  ASSERT_NE(minimal_spine, nullptr);
  ASSERT_NE(minimal_rib, nullptr);
  ASSERT_NE(minimal_skull, nullptr);
  ASSERT_NE(spine, nullptr);
  ASSERT_NE(upper_spine, nullptr);
  ASSERT_NE(high_rib, nullptr);
  ASSERT_NE(rib, nullptr);
  ASSERT_NE(low_rib, nullptr);
  ASSERT_NE(neck_base, nullptr);
  ASSERT_NE(neck, nullptr);
  ASSERT_NE(sternum, nullptr);
  ASSERT_NE(skull, nullptr);
  ASSERT_NE(eye, nullptr);
  ASSERT_NE(nasal_cavity, nullptr);
  ASSERT_NE(cheek, nullptr);
  ASSERT_NE(thigh, nullptr);

  EXPECT_EQ(spine->params.anchor_bone,
            static_cast<std::uint16_t>(HumanoidBone::Pelvis));
  EXPECT_EQ(skeleton.topology.bones.size(), Render::Humanoid::k_bone_count);
  EXPECT_EQ(spine->params.tail_bone, static_cast<std::uint16_t>(HumanoidBone::Chest));
  EXPECT_EQ(upper_spine->params.tail_bone,
            static_cast<std::uint16_t>(HumanoidBone::Neck));
  EXPECT_GT(neck_base->params.radius, neck->params.radius);
  EXPECT_GT(neck_base->params.head_offset.y(), 0.0F);
  EXPECT_LT(neck->params.radius, upper_spine->params.radius);
  EXPECT_GT(std::abs(sternum->params.head_offset.y() - sternum->params.tail_offset.y()),
            std::abs(neck->params.head_offset.y() - neck->params.tail_offset.y()) *
                3.0F);
  EXPECT_GT(
      std::abs(high_rib->params.head_offset.y() - low_rib->params.head_offset.y()),
      0.28F);
  EXPECT_GT(std::abs(high_rib->params.tail_offset.x()), 0.19F);
  EXPECT_GT(std::abs(minimal_rib->params.tail_offset.x()), 0.19F);
  EXPECT_LT(sternum->params.tail_offset.y(), 0.0F);
  EXPECT_GT(std::abs(rib->params.tail_offset.x() - rib->params.head_offset.x()),
            std::abs(rib->params.tail_offset.y() - rib->params.head_offset.y()));
  EXPECT_EQ(ribcage_blob, nullptr);
  EXPECT_EQ(sternum_blob, nullptr);
  EXPECT_EQ(rib->shape, Render::Creature::PrimitiveShape::OrientedCylinder);
  EXPECT_EQ(minimal_rib->shape, Render::Creature::PrimitiveShape::OrientedCylinder);
  EXPECT_EQ(minimal_skull->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(skull->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(eye->color_role, 0U);
  EXPECT_EQ(nasal_cavity->color_role, 0U);
  EXPECT_EQ(cheek->color_role, 2U);
  EXPECT_LT(thigh->params.radius,
            find_primitive(base.lod_full.primitives, "humanoid_full_thigh_l_bot")
                ->params.radius);
}

TEST(HumanoidSpecTest, SkeletonProportionLayerSeatsSkullCloserToRibCage) {
  HumanoidPose pose = make_upright_pose();
  float const before = (pose.head_pos - pose.neck_base).length();
  Render::Humanoid::apply_skeleton_proportion_pose(pose);

  float const after = (pose.head_pos - pose.neck_base).length();
  EXPECT_NEAR(after, before * 0.70F, 0.0001F);
  EXPECT_LT(after, before - 0.08F);
}

TEST(HumanoidSpecTest, MinimalLodSpansHeadToFootInUprightPose) {
  CreatureSpec const& s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  std::span<const QMatrix4x4> const palette_view(palette);

  QMatrix4x4 const identity;
  RecordingSubmitter sub;
  Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Minimal, identity, sub);
  ASSERT_FALSE(sub.parts.empty());

  float lowest = std::numeric_limits<float>::max();
  float highest = std::numeric_limits<float>::lowest();
  float leftmost = std::numeric_limits<float>::max();
  float rightmost = std::numeric_limits<float>::lowest();
  for (auto const& part : sub.parts) {
    for (float const y : {0.5F, -0.5F}) {
      QVector3D const endpoint = part.model.map(QVector3D(0.0F, y, 0.0F));
      lowest = std::min(lowest, endpoint.y());
      highest = std::max(highest, endpoint.y());
      leftmost = std::min(leftmost, endpoint.x());
      rightmost = std::max(rightmost, endpoint.x());
    }
  }

  EXPECT_LE(lowest, pose.foot_l.y() + 0.05F);
  EXPECT_GE(highest, pose.head_pos.y() - 0.05F);
  EXPECT_LE(leftmost, pose.hand_l.x() + 0.05F);
  EXPECT_GE(rightmost, pose.hand_r.x() - 0.05F);
}

TEST(HumanoidSpecTest, MinimalLodRespectsWorldFromUnit) {

  CreatureSpec const& s = humanoid_creature_spec();
  HumanoidPose const pose = make_upright_pose();

  std::array<QMatrix4x4, k_bone_count> palette;
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);
  std::span<const QMatrix4x4> const palette_view(palette);

  RecordingSubmitter base_sub;
  QMatrix4x4 const identity;
  Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Minimal, identity, base_sub);
  ASSERT_FALSE(base_sub.parts.empty());

  QMatrix4x4 world;
  world.translate(10.0F, 0.0F, 0.0F);
  RecordingSubmitter moved_sub;
  Render::Creature::submit_creature(
      s, palette_view, CreatureLOD::Minimal, world, moved_sub);
  ASSERT_EQ(moved_sub.parts.size(), base_sub.parts.size());

  for (std::size_t i = 0; i < base_sub.parts.size(); ++i) {
    for (float const y : {0.5F, -0.5F}) {
      QVector3D const base = base_sub.parts[i].model.map(QVector3D(0.0F, y, 0.0F));
      QVector3D const moved = moved_sub.parts[i].model.map(QVector3D(0.0F, y, 0.0F));
      EXPECT_NEAR(moved.x() - base.x(), 10.0F, 1.0e-4F);
      EXPECT_NEAR(moved.y() - base.y(), 0.0F, 1.0e-4F);
      EXPECT_NEAR(moved.z() - base.z(), 0.0F, 1.0e-4F);
    }
  }
}
