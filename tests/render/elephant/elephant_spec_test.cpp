

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <vector>

#include "render/creature/spec.h"
#include "render/elephant/elephant_manifest.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/rigged_mesh_bake.h"
#include "render/submitter.h"

namespace {

using Render::GL::ISubmitter;

struct Call {
  Render::GL::Mesh* mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  int material_id{0};
};

class CapturingSubmitter : public ISubmitter {
public:
  std::vector<Call> calls;

  void mesh(Render::GL::Mesh* m,
            const QMatrix4x4& model,
            const QVector3D& col,
            Render::GL::Texture*,
            float,
            int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void part(Render::GL::Mesh* m,
            Render::GL::Material*,
            const QMatrix4x4& model,
            const QVector3D& col,
            Render::GL::Texture*,
            float,
            int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
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

Render::GL::ElephantDimensions make_dims() {
  Render::GL::ElephantDimensions d{};
  d.body_length = 3.2F;
  d.body_width = 1.1F;
  d.body_height = 1.6F;
  d.barrel_center_y = 2.0F;
  d.neck_length = 0.9F;
  d.neck_width = 0.42F;
  d.head_length = 1.1F;
  d.head_width = 0.75F;
  d.head_height = 0.78F;
  d.trunk_length = 1.5F;
  d.trunk_base_radius = 0.22F;
  d.trunk_tip_radius = 0.06F;
  d.ear_width = 1.0F;
  d.ear_height = 1.1F;
  d.ear_thickness = 0.06F;
  d.leg_length = 1.6F;
  d.leg_radius = 0.22F;
  d.foot_radius = 0.28F;
  d.tail_length = 0.8F;
  d.tusk_length = 0.55F;
  d.tusk_radius = 0.06F;
  return d;
}

Render::GL::ElephantVariant make_variant() {
  Render::GL::ElephantVariant v{};
  v.skin_color = QVector3D(0.45F, 0.40F, 0.38F);
  return v;
}

Render::GL::ElephantGait make_gait() {
  Render::GL::ElephantGait g{};
  g.cycle_time = 1.2F;
  g.front_leg_phase = 0.0F;
  g.rear_leg_phase = 0.5F;
  g.stride_swing = 0.30F;
  g.stride_lift = 0.10F;
  return g;
}

auto find_primitive(std::span<const Render::Creature::PrimitiveInstance> prims,
                    std::string_view name)
    -> const Render::Creature::PrimitiveInstance* {
  auto it = std::find_if(prims.begin(), prims.end(), [&](auto const& prim) {
    return prim.debug_name == name;
  });
  return it == prims.end() ? nullptr : &*it;
}

auto matrix_max_delta(const QMatrix4x4& lhs, const QMatrix4x4& rhs) -> float {
  float delta = 0.0F;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      delta = std::max(delta, std::abs(lhs(row, column) - rhs(row, column)));
    }
  }
  return delta;
}

auto mesh_axis_span(const Render::GL::Mesh& mesh, std::size_t axis) -> float {
  auto const& vertices = mesh.get_vertices();
  if (vertices.empty()) {
    return 0.0F;
  }
  float min_v = vertices.front().position[axis];
  float max_v = vertices.front().position[axis];
  for (auto const& vertex : vertices) {
    min_v = std::min(min_v, vertex.position[axis]);
    max_v = std::max(max_v, vertex.position[axis]);
  }
  return max_v - min_v;
}

auto mesh_vertex_count_for_role(const Render::GL::Mesh& mesh,
                                std::uint8_t role) -> std::size_t {
  auto const& vertices = mesh.get_vertices();
  return static_cast<std::size_t>(
      std::count_if(vertices.begin(), vertices.end(), [&](auto const& vertex) {
        return vertex.color_role == role;
      }));
}

auto baked_vertex_count_for_role(const Render::Creature::BakedRiggedMeshCpu& mesh,
                                 std::uint8_t role) -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      mesh.vertices.begin(), mesh.vertices.end(), [&](auto const& vertex) {
        return vertex.color_role == role;
      }));
}

auto shader_role_color(
    const std::array<QVector3D, Render::Elephant::k_elephant_role_count>& roles,
    std::uint8_t role) -> QVector3D {
  if (role == 0U) {
    return QVector3D();
  }
  std::size_t const index = static_cast<std::size_t>(role - 1U);
  if (index >= roles.size()) {
    return QVector3D();
  }
  return roles[index];
}

auto point_to_segment_distance(const QVector3D& point,
                               const QVector3D& a,
                               const QVector3D& b) -> float {
  QVector3D const ab = b - a;
  float const denom = QVector3D::dotProduct(ab, ab);
  if (denom <= 1.0e-6F) {
    return (point - a).length();
  }
  float const t = std::clamp(QVector3D::dotProduct(point - a, ab) / denom, 0.0F, 1.0F);
  return (point - (a + ab * t)).length();
}

} // namespace

TEST(ElephantSpecTest, CreatureSpecHasTwoLods) {
  auto const& spec = Render::Elephant::elephant_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 1U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 1U);
}

TEST(ElephantSpecTest, PoseKeepsHeavyBodyAndPillarLegReadability) {
  auto const dims = make_dims();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);
  QVector3D const shoulder_fl = pose.barrel_center + pose.shoulder_offset_fl;
  QVector3D const shoulder_bl = pose.barrel_center + pose.shoulder_offset_bl;
  float const front_mid_z = (shoulder_fl.z() + pose.foot_fl.z()) * 0.5F;
  float const rear_mid_z = (shoulder_bl.z() + pose.foot_bl.z()) * 0.5F;

  EXPECT_GT(pose.body_ellipsoid_x, dims.body_width);
  EXPECT_GT(pose.body_ellipsoid_z, dims.body_length);
  EXPECT_LT(pose.body_ellipsoid_x, dims.body_width * 1.3F);
  EXPECT_GE(pose.leg_radius, dims.leg_radius * 0.69F);
  EXPECT_LE(pose.shoulder_offset_fl.y(), -dims.body_height * 0.25F);
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y());
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y() - dims.leg_length * 0.6F);
  EXPECT_GT(pose.knee_fl.z(), front_mid_z);
  EXPECT_LT(pose.knee_bl.z(), rear_mid_z);
  EXPECT_GT(point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl),
            dims.body_length * 0.018F);
  EXPECT_GT(point_to_segment_distance(pose.knee_bl, shoulder_bl, pose.foot_bl),
            dims.body_length * 0.015F);
}

TEST(ElephantSpecTest, AnimatedPoseKeepsForwardTrunkAndLighterHeadRead) {
  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, Render::Elephant::ElephantPoseMotion{}, pose);

  EXPECT_LT(pose.head_half.x(), dims.head_width * 0.45F);
  EXPECT_LT(pose.head_half.y(), dims.head_height * 0.41F);
  EXPECT_GT(pose.trunk_end.z(), pose.head_center.z() + dims.trunk_length * 0.35F);
  EXPECT_LT(pose.trunk_end.y(), pose.head_center.y() - dims.trunk_length * 0.45F);
  EXPECT_LT(pose.pose_leg_radius, dims.leg_radius * 0.9F);
}

TEST(ElephantSpecTest, RoleColorsUseWhiteTusksAndBlackEyes) {
  auto const variant = Render::GL::make_elephant_variant(
      0U, QVector3D(0.2F, 0.3F, 0.4F), QVector3D(0.5F, 0.6F, 0.7F));
  std::array<QVector3D, Render::Elephant::k_elephant_role_count> roles{};
  Render::Elephant::fill_elephant_role_colors(variant, roles);

  EXPECT_EQ(shader_role_color(roles, 6U), QVector3D(1.0F, 1.0F, 1.0F));
  EXPECT_EQ(shader_role_color(roles, 7U), QVector3D(0.0F, 0.0F, 0.0F));
}

TEST(ElephantSpecTest, WholeMeshCompilationKeepsColoredFaceVertices) {
  auto const& spec = Render::Elephant::elephant_creature_spec();
  auto const* minimal_whole =
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.whole");
  auto const* full_body =
      find_primitive(spec.lod_full.primitives, "elephant.full.body");

  ASSERT_NE(minimal_whole, nullptr);
  ASSERT_NE(full_body, nullptr);
  ASSERT_NE(minimal_whole->custom_mesh, nullptr);
  ASSERT_NE(full_body->custom_mesh, nullptr);

  EXPECT_GT(mesh_vertex_count_for_role(*minimal_whole->custom_mesh, 6U), 0U);
  EXPECT_GT(mesh_vertex_count_for_role(*minimal_whole->custom_mesh, 7U), 0U);
  EXPECT_GT(mesh_vertex_count_for_role(*full_body->custom_mesh, 6U), 0U);
  EXPECT_GT(mesh_vertex_count_for_role(*full_body->custom_mesh, 7U), 0U);
}

TEST(ElephantSpecTest, AuthoredFaceHasTwoSymmetricBlackEyes) {
  auto const& spec = Render::Elephant::elephant_creature_spec();
  auto const* full_body =
      find_primitive(spec.lod_full.primitives, "elephant.full.body");
  ASSERT_NE(full_body, nullptr);
  ASSERT_NE(full_body->custom_mesh, nullptr);

  auto const bind = Render::Elephant::elephant_bind_palette();
  auto const root =
      bind[static_cast<std::size_t>(Render::Elephant::ElephantBone::Root)];
  std::size_t left_eye_vertices = 0U;
  std::size_t right_eye_vertices = 0U;
  for (auto const& vertex : full_body->custom_mesh->get_vertices()) {
    if (vertex.color_role != 7U) {
      continue;
    }
    QVector3D const rest =
        root.map(QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
    if (rest.z() <= 0.0F) {
      continue;
    }
    left_eye_vertices += rest.x() < 0.0F ? 1U : 0U;
    right_eye_vertices += rest.x() > 0.0F ? 1U : 0U;
  }

  EXPECT_GT(left_eye_vertices, 0U);
  EXPECT_EQ(left_eye_vertices, right_eye_vertices);
}

TEST(ElephantSpecTest, MinimalSnapshotBakeKeepsColoredFaceRoles) {
  auto const& spec = Render::Elephant::elephant_creature_spec();
  Render::Creature::BakeInput in{};
  in.graph = &spec.lod_minimal;
  in.bind_pose = Render::Elephant::elephant_bind_palette();

  auto const baked = Render::Creature::bake_rigged_mesh_cpu(in);

  EXPECT_FALSE(baked.vertices.empty());
  EXPECT_GT(baked_vertex_count_for_role(baked, 6U), 0U);
  EXPECT_GT(baked_vertex_count_for_role(baked, 7U), 0U);
}

TEST(ElephantSpecTest, MinimalSpecKeepsHeadAndTrunkIdentity) {
  auto const& spec = Render::Elephant::elephant_creature_spec();

  auto const* whole =
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.whole");

  ASSERT_NE(whole, nullptr);

  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 1),
            mesh_axis_span(*whole->custom_mesh, 2) * 0.8F);
  EXPECT_EQ(find_primitive(spec.lod_minimal.primitives, "elephant.minimal.leg.fl"),
            nullptr);
}

TEST(ElephantSpecTest, MovingPoseBendsKneesDuringStride) {
  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = 0.30F;
  motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, motion, pose);

  QVector3D const shoulder_fl = pose.barrel_center + pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_bl = pose.barrel_center + pose.shoulder_offset_pose_bl;
  float const front_mid_z = (shoulder_fl.z() + pose.foot_fl.z()) * 0.5F;
  float const rear_mid_z = (shoulder_bl.z() + pose.foot_bl.z()) * 0.5F;

  EXPECT_LT(pose.knee_fl.y(), shoulder_fl.y());
  EXPECT_GT(pose.knee_fl.y(), pose.foot_fl.y());
  EXPECT_GT(pose.knee_fl.z(), front_mid_z);
  EXPECT_GT(point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl),
            dims.body_length * 0.02F);

  EXPECT_LT(pose.knee_bl.y(), shoulder_bl.y());
  EXPECT_GT(pose.knee_bl.y(), pose.foot_bl.y());
  EXPECT_LT(pose.knee_bl.z(), rear_mid_z);
}

TEST(ElephantSpecTest, WalkFootLandsAheadOfShoulderDuringSwing) {

  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = 0.25F;
  motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, motion, pose);

  QVector3D const shoulder_fl = pose.barrel_center + pose.shoulder_offset_pose_fl;

  EXPECT_GT(pose.foot_fl.z(), shoulder_fl.z());

  EXPECT_GT(pose.foot_fl.z() - shoulder_fl.z(), dims.body_length * 0.02F);
}

TEST(ElephantSpecTest, WalkPoseKeepsRearThighAnchorsCloserToBody) {
  auto const dims = make_dims();
  auto const gait = make_gait();

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, Render::Elephant::ElephantPoseMotion{}, idle_pose);

  Render::Elephant::ElephantSpecPose walk_pose{};
  Render::Elephant::ElephantPoseMotion walk_motion{};
  walk_motion.phase = 0.25F;
  walk_motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, walk_motion, walk_pose);

  EXPECT_GT(walk_pose.shoulder_offset_pose_bl.z(),
            idle_pose.shoulder_offset_pose_bl.z() - dims.body_length * 0.05F);
  EXPECT_GT(walk_pose.shoulder_offset_pose_br.z(),
            idle_pose.shoulder_offset_pose_br.z() - dims.body_length * 0.05F);
}

TEST(ElephantSpecTest, FightPoseRaisesFeetHigherThanIdlePose) {
  auto const dims = make_dims();
  auto const gait = make_gait();

  Render::Elephant::ElephantSpecPose fight_pose{};
  Render::Elephant::ElephantPoseMotion fight_motion{};
  fight_motion.phase = 0.55F;
  fight_motion.anim_time = 0.55F * 0.95F;
  fight_motion.is_fighting = true;
  fight_motion.combat_phase = Render::GL::CombatAnimPhase::Idle;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, fight_motion, fight_pose);

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::ElephantPoseMotion idle_motion{};
  idle_motion.phase = 0.55F;
  idle_motion.is_fighting = false;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, idle_motion, idle_pose);

  EXPECT_GT(fight_pose.foot_fl.y() - idle_pose.foot_fl.y(), dims.leg_length * 0.10F);
}

TEST(ElephantSpecTest, MovementBobVariesAcrossWalkFrames) {
  auto dims = make_dims();
  dims.move_bob_amplitude = 0.06F;
  Render::GL::ElephantGait walk{1.2F, 0.25F, 0.0F, 0.30F, 0.10F};

  Render::Elephant::ElephantSpecPose pose_zero{};
  Render::Elephant::ElephantPoseMotion motion_zero{};
  motion_zero.phase = 0.0F;
  motion_zero.anim_time = 0.0F;
  motion_zero.is_moving = true;
  motion_zero.bob = 0.0F;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, walk, motion_zero, pose_zero);

  float const peak_bob = dims.move_bob_amplitude * 0.62F;
  Render::Elephant::ElephantSpecPose pose_peak{};
  Render::Elephant::ElephantPoseMotion motion_peak{};
  motion_peak.phase = 0.25F;
  motion_peak.anim_time = 0.25F * 1.2F;
  motion_peak.is_moving = true;
  motion_peak.bob = peak_bob;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, walk, motion_peak, pose_peak);

  EXPECT_GT(pose_peak.barrel_center.y(), pose_zero.barrel_center.y());
}

TEST(ElephantSpecTest, FightPoseTrunkRaisedAboveIdlePose) {
  auto const dims = make_dims();
  auto const gait = make_gait();

  Render::Elephant::ElephantSpecPose fight_pose{};
  Render::Elephant::ElephantPoseMotion fight_motion{};
  fight_motion.anim_time = 1.15F * 0.25F;
  fight_motion.is_fighting = true;
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, fight_motion, fight_pose);

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::ElephantPoseMotion idle_motion{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, idle_motion, idle_pose);

  EXPECT_GT(fight_pose.trunk_end.y(),
            idle_pose.trunk_end.y() + dims.trunk_length * 0.12F);
  EXPECT_GT(fight_pose.trunk_end.z(),
            idle_pose.trunk_end.z() + dims.trunk_length * 0.04F);
  EXPECT_GT(std::abs(fight_pose.trunk_end.x() - idle_pose.trunk_end.x()),
            dims.head_width * 0.10F);
  EXPECT_LT(fight_pose.trunk_end.y(),
            fight_pose.head_center.y() - dims.trunk_length * 0.08F);
  EXPECT_LT(std::abs(fight_pose.trunk_end.x() - idle_pose.trunk_end.x()),
            dims.head_width * 0.16F);
  EXPECT_NEAR(fight_pose.head_center.x(), idle_pose.head_center.x(), 0.0001F);
  EXPECT_NEAR(fight_pose.head_center.y(), idle_pose.head_center.y(), 0.0001F);
  EXPECT_NEAR(fight_pose.head_center.z(), idle_pose.head_center.z(), 0.0001F);
}

TEST(ElephantSpecTest, BakedFightUsesAuthoredTrunkAndLegAttack) {
  auto const& manifest = Render::Elephant::elephant_manifest();
  auto const idle_it =
      std::find_if(manifest.clips.begin(), manifest.clips.end(), [](auto const& clip) {
        return clip.name == "idle";
      });
  auto const fight_it =
      std::find_if(manifest.clips.begin(), manifest.clips.end(), [](auto const& clip) {
        return clip.name == "fight";
      });
  ASSERT_NE(idle_it, manifest.clips.end());
  ASSERT_NE(fight_it, manifest.clips.end());
  ASSERT_NE(manifest.bake_clip_palette, nullptr);

  std::vector<QMatrix4x4> idle;
  std::vector<QMatrix4x4> fight;
  constexpr std::uint32_t frame = 6U;
  manifest.bake_clip_palette(
      static_cast<std::size_t>(idle_it - manifest.clips.begin()), frame, idle);
  manifest.bake_clip_palette(
      static_cast<std::size_t>(fight_it - manifest.clips.begin()), frame, fight);
  ASSERT_EQ(idle.size(), Render::Elephant::k_elephant_bone_count);
  ASSERT_EQ(fight.size(), Render::Elephant::k_elephant_bone_count);

  constexpr std::array<Render::Elephant::ElephantBone, 12> leg_bones{{
      Render::Elephant::ElephantBone::ShoulderFL,
      Render::Elephant::ElephantBone::KneeFL,
      Render::Elephant::ElephantBone::FootFL,
      Render::Elephant::ElephantBone::ShoulderFR,
      Render::Elephant::ElephantBone::KneeFR,
      Render::Elephant::ElephantBone::FootFR,
      Render::Elephant::ElephantBone::ShoulderBL,
      Render::Elephant::ElephantBone::KneeBL,
      Render::Elephant::ElephantBone::FootBL,
      Render::Elephant::ElephantBone::ShoulderBR,
      Render::Elephant::ElephantBone::KneeBR,
      Render::Elephant::ElephantBone::FootBR,
  }};
  auto const bone_defs = Render::Elephant::elephant_source_bone_defs();
  int articulated_leg_bones = 0;
  for (auto const bone : leg_bones) {
    auto const index = static_cast<std::size_t>(bone);
    auto const parent = static_cast<std::size_t>(bone_defs[index].parent);
    ASSERT_LT(parent, idle.size());
    QMatrix4x4 const idle_local = idle[parent].inverted() * idle[index];
    QMatrix4x4 const fight_local = fight[parent].inverted() * fight[index];
    if (matrix_max_delta(idle_local, fight_local) > 0.001F) {
      ++articulated_leg_bones;
    }
  }
  EXPECT_GE(articulated_leg_bones, 4);

  auto const trunk_tip =
      static_cast<std::size_t>(Render::Elephant::ElephantBone::TrunkTip);
  EXPECT_GT(matrix_max_delta(idle[trunk_tip], fight[trunk_tip]), 0.01F);
}

TEST(ElephantSpecTest, MovingFrontLegLiftExceedsRearAtPeakSwing) {
  auto const dims = make_dims();
  Render::GL::ElephantGait gait{};
  gait.cycle_time = 1.2F;
  gait.front_leg_phase = 0.0F;
  gait.rear_leg_phase = 0.5F;
  gait.stride_swing = 0.30F;
  gait.stride_lift = 0.12F;

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, Render::Elephant::ElephantPoseMotion{}, idle_pose);

  Render::Elephant::ElephantSpecPose swing_pose{};
  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = 0.25F;
  motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, motion, swing_pose);

  float const front_lift = swing_pose.foot_pose_fl.y() - idle_pose.foot_pose_fl.y();
  float const rear_lift = swing_pose.foot_pose_br.y() - idle_pose.foot_pose_br.y();

  EXPECT_GT(front_lift, 0.0F);
  EXPECT_GT(rear_lift, 0.0F);

  EXPECT_GT(front_lift, rear_lift);
}
