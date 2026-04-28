// Stage 16.5 — elephant rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_elephant_lod`) was removed
// when the imperative LOD path was retired; the rig now exclusively
// calls the rigged wrappers. With a non-Renderer submitter we exercise
// the software fallback inside `submit_elephant_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (7/41 for Minimal/Full).

#include "render/creature/spec.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/mesh.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Render::GL::ISubmitter;

struct Call {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  int material_id{0};
};

class CapturingSubmitter : public ISubmitter {
public:
  std::vector<Call> calls;

  void mesh(Render::GL::Mesh *m, const QMatrix4x4 &model, const QVector3D &col,
            Render::GL::Texture *, float, int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void part(Render::GL::Mesh *m, Render::GL::Material *,
            const QMatrix4x4 &model, const QVector3D &col,
            Render::GL::Texture *, float, int mat) override {
    calls.push_back({m, model, col, mat});
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
    -> const Render::Creature::PrimitiveInstance * {
  auto it = std::find_if(prims.begin(), prims.end(), [&](auto const &prim) {
    return prim.debug_name == name;
  });
  return it == prims.end() ? nullptr : &*it;
}

auto mesh_axis_span(const Render::GL::Mesh &mesh, std::size_t axis) -> float {
  auto const &vertices = mesh.get_vertices();
  if (vertices.empty()) {
    return 0.0F;
  }
  float min_v = vertices.front().position[axis];
  float max_v = vertices.front().position[axis];
  for (auto const &vertex : vertices) {
    min_v = std::min(min_v, vertex.position[axis]);
    max_v = std::max(max_v, vertex.position[axis]);
  }
  return max_v - min_v;
}

auto point_to_segment_distance(const QVector3D &point, const QVector3D &a,
                               const QVector3D &b) -> float {
  QVector3D const ab = b - a;
  float const denom = QVector3D::dotProduct(ab, ab);
  if (denom <= 1.0e-6F) {
    return (point - a).length();
  }
  float const t =
      std::clamp(QVector3D::dotProduct(point - a, ab) / denom, 0.0F, 1.0F);
  return (point - (a + ab * t)).length();
}

} // namespace

TEST(ElephantSpecTest, CreatureSpecHasTwoLods) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 1U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 1U);
}

TEST(ElephantSpecTest, PoseKeepsHeavyBodyAndPillarLegReadability) {
  auto const dims = make_dims();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  EXPECT_GT(pose.body_ellipsoid_x, dims.body_width);
  EXPECT_GT(pose.body_ellipsoid_z, dims.body_length);
  EXPECT_LT(pose.body_ellipsoid_x, dims.body_width * 1.3F);
  EXPECT_GE(pose.leg_radius, dims.leg_radius * 0.69F);
  EXPECT_LE(pose.shoulder_offset_fl.y(), -dims.body_height * 0.25F);
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y());
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y() - dims.leg_length * 0.6F);
}

TEST(ElephantSpecTest, AnimatedPoseKeepsForwardTrunkAndLighterHeadRead) {
  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, Render::Elephant::ElephantPoseMotion{}, pose);

  EXPECT_LT(pose.head_half.x(), dims.head_width * 0.45F);
  EXPECT_LT(pose.head_half.y(), dims.head_height * 0.41F);
  EXPECT_GT(pose.trunk_end.z(),
            pose.head_center.z() + dims.trunk_length * 0.35F);
  EXPECT_LT(pose.trunk_end.y(),
            pose.head_center.y() - dims.trunk_length * 0.45F);
  EXPECT_LT(pose.pose_leg_radius, dims.leg_radius * 0.9F);
}

TEST(ElephantSpecTest, CreatureSpecUsesFacetedLowPolyBodyAndHeadMeshes) {
  auto const &spec = Render::Elephant::elephant_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "elephant.full.whole");

  ASSERT_NE(whole, nullptr);
  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 2),
            mesh_axis_span(*whole->custom_mesh, 1) * 1.2F);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 0),
            mesh_axis_span(*whole->custom_mesh, 1) * 0.35F);
  EXPECT_EQ(
      find_primitive(spec.lod_full.primitives, "elephant.full.body.chest"),
      nullptr);
  EXPECT_EQ(
      find_primitive(spec.lod_full.primitives, "elephant.full.head.cheek.l"),
      nullptr);
}

TEST(ElephantSpecTest, MinimalSpecKeepsHeadAndTrunkIdentity) {
  auto const &spec = Render::Elephant::elephant_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.whole");

  ASSERT_NE(whole, nullptr);

  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 2),
            mesh_axis_span(*whole->custom_mesh, 1) * 1.2F);
  EXPECT_EQ(
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.leg.fl"),
      nullptr);
}

TEST(ElephantSpecTest, FullSpecUsesFacetedSegmentMeshesForTrunkAndLegs) {
  auto const &spec = Render::Elephant::elephant_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "elephant.full.whole");

  ASSERT_NE(whole, nullptr);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_EQ(
      find_primitive(spec.lod_full.primitives, "elephant.full.trunk.seg.0"),
      nullptr);
  EXPECT_EQ(
      find_primitive(spec.lod_full.primitives, "elephant.full.leg.fl.lower"),
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

  QVector3D const shoulder_fl =
      pose.barrel_center + pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_bl =
      pose.barrel_center + pose.shoulder_offset_pose_bl;
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

TEST(ElephantSpecTest, FightPoseRaisesFeetHigherThanIdlePose) {
  auto const dims = make_dims();
  auto const gait = make_gait();

  // Phase 0.55 is in the "hold high" window (0.45-0.65) of the fight cycle
  Render::Elephant::ElephantSpecPose fight_pose{};
  Render::Elephant::ElephantPoseMotion fight_motion{};
  fight_motion.phase = 0.55F;
  fight_motion.anim_time = 0.55F * 1.15F;
  fight_motion.is_fighting = true;
  fight_motion.combat_phase = Render::GL::CombatAnimPhase::Idle;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, fight_motion,
                                                     fight_pose);

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::ElephantPoseMotion idle_motion{};
  idle_motion.phase = 0.55F;
  idle_motion.is_fighting = false;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, idle_motion,
                                                     idle_pose);

  // Feet must be raised during fight peak
  EXPECT_GT(fight_pose.foot_fl.y() - idle_pose.foot_fl.y(),
            dims.leg_length * 0.10F);
}

TEST(ElephantSpecTest, MovementBobVariesAcrossWalkFrames) {
  auto dims = make_dims();
  dims.move_bob_amplitude = 0.06F;
  Render::GL::ElephantGait walk{1.2F, 0.25F, 0.0F, 0.30F, 0.10F};

  // Phase 0.0 → sin = 0, so bob = 0
  Render::Elephant::ElephantSpecPose pose_zero{};
  Render::Elephant::ElephantPoseMotion motion_zero{};
  motion_zero.phase = 0.0F;
  motion_zero.anim_time = 0.0F;
  motion_zero.is_moving = true;
  motion_zero.bob = 0.0F;
  Render::Elephant::make_elephant_spec_pose_animated(dims, walk, motion_zero,
                                                     pose_zero);

  // Phase 0.25 → sin(π/2)=1, peak bob
  float const peak_bob = dims.move_bob_amplitude * 0.62F;
  Render::Elephant::ElephantSpecPose pose_peak{};
  Render::Elephant::ElephantPoseMotion motion_peak{};
  motion_peak.phase = 0.25F;
  motion_peak.anim_time = 0.25F * 1.2F;
  motion_peak.is_moving = true;
  motion_peak.bob = peak_bob;
  Render::Elephant::make_elephant_spec_pose_animated(dims, walk, motion_peak,
                                                     pose_peak);

  EXPECT_GT(pose_peak.barrel_center.y(), pose_zero.barrel_center.y());
}
