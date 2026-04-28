// Stage 16.5 — elephant rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_elephant_lod`) was removed
// when the imperative LOD path was retired; the rig now exclusively
// calls the rigged wrappers. With a non-Renderer submitter we exercise
// the software fallback inside `submit_elephant_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (1/13 for Minimal/Full).

#include "render/creature/spec.h"
#include "render/elephant/elephant_manifest.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/rigged_mesh_bake.h"
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

auto find_mesh_node(
    std::span<const Render::Creature::Quadruped::MeshNode> nodes,
    std::string_view name) -> const Render::Creature::Quadruped::MeshNode * {
  auto it = std::find_if(nodes.begin(), nodes.end(), [&](auto const &node) {
    return node.debug_name == name;
  });
  return it == nodes.end() ? nullptr : &*it;
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

auto mesh_vertex_count_for_role(const Render::GL::Mesh &mesh, std::uint8_t role)
    -> std::size_t {
  auto const &vertices = mesh.get_vertices();
  return static_cast<std::size_t>(std::count_if(
      vertices.begin(), vertices.end(), [&](auto const &vertex) {
        return vertex.color_role == role;
      }));
}

auto baked_vertex_count_for_role(
    const Render::Creature::BakedRiggedMeshCpu &mesh, std::uint8_t role)
    -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      mesh.vertices.begin(), mesh.vertices.end(), [&](auto const &vertex) {
        return vertex.color_role == role;
      }));
}

auto shader_role_color(
    const std::array<QVector3D, Render::Elephant::kElephantRoleCount> &roles,
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

auto outline_axis_span(std::span<const QVector3D> outline,
                       std::size_t axis) -> float {
  if (outline.empty()) {
    return 0.0F;
  }
  float min_v = outline.front()[axis];
  float max_v = outline.front()[axis];
  for (auto const &point : outline) {
    min_v = std::min(min_v, point[axis]);
    max_v = std::max(max_v, point[axis]);
  }
  return max_v - min_v;
}

auto max_body_half_width(const Render::Creature::Quadruped::BarrelNode &body)
    -> float {
  float max_width = 0.0F;
  for (auto const &ring : body.rings) {
    max_width = std::max(max_width, ring.half_width);
  }
  return max_width;
}

auto max_body_vertical_extent(const Render::Creature::Quadruped::BarrelNode &body)
    -> float {
  float max_extent = 0.0F;
  for (auto const &ring : body.rings) {
    max_extent = std::max(max_extent, ring.y + ring.top);
    max_extent = std::max(max_extent, ring.bottom - ring.y);
  }
  return max_extent;
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
  EXPECT_EQ(spec.lod_full.primitives.size(), 13U);
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
  EXPECT_GT(pose.trunk_end.z(),
            pose.head_center.z() + dims.trunk_length * 0.35F);
  EXPECT_LT(pose.trunk_end.y(),
            pose.head_center.y() - dims.trunk_length * 0.45F);
  EXPECT_LT(pose.pose_leg_radius, dims.leg_radius * 0.9F);
}

TEST(ElephantSpecTest, RoleColorsUseWhiteTusksAndBlackEyes) {
  auto const variant = Render::GL::make_elephant_variant(
      0U, QVector3D(0.2F, 0.3F, 0.4F), QVector3D(0.5F, 0.6F, 0.7F));
  std::array<QVector3D, Render::Elephant::kElephantRoleCount> roles{};
  Render::Elephant::fill_elephant_role_colors(variant, roles);

  EXPECT_EQ(shader_role_color(roles, 6U), QVector3D(1.0F, 1.0F, 1.0F));
  EXPECT_EQ(shader_role_color(roles, 7U), QVector3D(0.0F, 0.0F, 0.0F));
}

TEST(ElephantSpecTest, WholeMeshCompilationKeepsColoredFaceVertices) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto const *minimal_whole =
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.whole");
  auto const *full_body =
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

TEST(ElephantSpecTest, MinimalSnapshotBakeKeepsColoredFaceRoles) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  Render::Creature::BakeInput in{};
  in.graph = &spec.lod_minimal;
  in.bind_pose = Render::Elephant::elephant_bind_palette();

  auto const baked = Render::Creature::bake_rigged_mesh_cpu(in);

  EXPECT_FALSE(baked.vertices.empty());
  EXPECT_GT(baked_vertex_count_for_role(baked, 6U), 0U);
  EXPECT_GT(baked_vertex_count_for_role(baked, 7U), 0U);
}

TEST(ElephantSpecTest, CreatureSpecUsesFacetedLowPolyBodyAndHeadMeshes) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto const &manifest = Render::Elephant::elephant_manifest();
  auto const *body =
      find_primitive(spec.lod_full.primitives, "elephant.full.body");
  auto const *body_node =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.body");

  ASSERT_NE(body, nullptr);
  ASSERT_NE(body_node, nullptr);
  EXPECT_EQ(body->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(body->custom_mesh, nullptr);
  auto const *barrel =
      std::get_if<Render::Creature::Quadruped::BarrelNode>(&body_node->data);
  ASSERT_NE(barrel, nullptr);
  EXPECT_FALSE(body->custom_mesh->get_vertices().empty());
  EXPECT_GT(max_body_half_width(*barrel), 0.45F);
  EXPECT_GT(max_body_vertical_extent(*barrel), 0.40F);
  EXPECT_GT(mesh_axis_span(*body->custom_mesh, 0), 0.90F);
  EXPECT_GT(mesh_axis_span(*body->custom_mesh, 1),
            mesh_axis_span(*body->custom_mesh, 2) * 0.7F);
  EXPECT_GT(mesh_axis_span(*body->custom_mesh, 2),
            mesh_axis_span(*body->custom_mesh, 0) * 1.0F);
  EXPECT_EQ(
      find_primitive(spec.lod_full.primitives, "elephant.full.body.chest"),
      nullptr);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "elephant.head"), nullptr);
}

TEST(ElephantSpecTest, MinimalSpecKeepsHeadAndTrunkIdentity) {
  auto const &spec = Render::Elephant::elephant_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.whole");

  ASSERT_NE(whole, nullptr);

  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 1),
            mesh_axis_span(*whole->custom_mesh, 2) * 0.8F);
  EXPECT_EQ(
      find_primitive(spec.lod_minimal.primitives, "elephant.minimal.leg.fl"),
      nullptr);
}

TEST(ElephantSpecTest, FullSpecUsesFacetedSegmentMeshesForTrunkAndLegs) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  auto const &manifest = Render::Elephant::elephant_manifest();
  auto const *front_thigh =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.leg.fl.thigh");
  auto const *front_calf =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.leg.fl.calf");
  auto const *front_hoof =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.leg.fl.hoof");
  auto const *thigh_prim =
      find_primitive(spec.lod_full.primitives, "elephant.leg.fl.thigh");
  auto const *calf_prim =
      find_primitive(spec.lod_full.primitives, "elephant.leg.fl.calf");
  auto const *hoof_prim =
      find_primitive(spec.lod_full.primitives, "elephant.leg.fl.hoof");

  ASSERT_NE(front_thigh, nullptr);
  ASSERT_NE(front_calf, nullptr);
  ASSERT_NE(front_hoof, nullptr);
  ASSERT_NE(thigh_prim, nullptr);
  ASSERT_NE(calf_prim, nullptr);
  ASSERT_NE(hoof_prim, nullptr);
  EXPECT_NE(
      std::get_if<Render::Creature::Quadruped::TubeNode>(&front_thigh->data),
      nullptr);
  EXPECT_NE(
      std::get_if<Render::Creature::Quadruped::TubeNode>(&front_calf->data),
      nullptr);
  EXPECT_NE(std::get_if<Render::Creature::Quadruped::EllipsoidNode>(
                &front_hoof->data),
            nullptr);
  EXPECT_EQ(thigh_prim->shape, Render::Creature::PrimitiveShape::Cylinder);
  EXPECT_EQ(calf_prim->shape, Render::Creature::PrimitiveShape::Cylinder);
  EXPECT_EQ(hoof_prim->shape, Render::Creature::PrimitiveShape::Box);
  ASSERT_NE(thigh_prim->custom_mesh, nullptr);
  ASSERT_NE(calf_prim->custom_mesh, nullptr);
  EXPECT_EQ(thigh_prim->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Elephant::ElephantBone::ShoulderFL));
  EXPECT_EQ(thigh_prim->params.tail_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Elephant::ElephantBone::KneeFL));
  EXPECT_EQ(calf_prim->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Elephant::ElephantBone::KneeFL));
  EXPECT_EQ(calf_prim->params.tail_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Elephant::ElephantBone::FootFL));
  EXPECT_EQ(hoof_prim->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Elephant::ElephantBone::FootFL));
  EXPECT_GT(hoof_prim->params.head_offset.y(), 0.0F);
  EXPECT_GT(thigh_prim->params.radius, calf_prim->params.radius * 1.6F);
  EXPECT_LT(thigh_prim->custom_mesh->get_vertices().size(),
            Render::GL::get_unit_cylinder()->get_vertices().size());
  EXPECT_LT(calf_prim->custom_mesh->get_vertices().size(),
            Render::GL::get_unit_cylinder()->get_vertices().size());
  EXPECT_NE(find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.trunk"),
            nullptr);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "elephant.full.whole"),
            nullptr);
  EXPECT_EQ(find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.leg"),
            nullptr);
}

TEST(ElephantSpecTest, ManifestKeepsTusksProminent) {
  auto const &manifest = Render::Elephant::elephant_manifest();

  std::size_t tusk_count = 0U;
  for (auto const &node : manifest.lod_full.mesh_nodes) {
    if (node.debug_name != std::string_view{"elephant.tusk"}) {
      continue;
    }
    auto const *tusk =
        std::get_if<Render::Creature::Quadruped::ConeNode>(&node.data);
    ASSERT_NE(tusk, nullptr);
    EXPECT_GT(tusk->base_radius, 0.06F);
    ++tusk_count;
  }

  EXPECT_EQ(tusk_count, 2U);
}

TEST(ElephantSpecTest, ManifestBridgesTrunkAndAddsEyes) {
  auto const &manifest = Render::Elephant::elephant_manifest();
  auto const *head =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.head");
  auto const *bridge =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.trunk.bridge");
  auto const *trunk =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.trunk");

  ASSERT_NE(head, nullptr);
  ASSERT_NE(bridge, nullptr);
  ASSERT_NE(trunk, nullptr);

  auto const *head_shape =
      std::get_if<Render::Creature::Quadruped::EllipsoidNode>(&head->data);
  auto const *bridge_shape =
      std::get_if<Render::Creature::Quadruped::EllipsoidNode>(&bridge->data);
  auto const *trunk_snout =
      std::get_if<Render::Creature::Quadruped::SnoutNode>(&trunk->data);
  ASSERT_NE(head_shape, nullptr);
  ASSERT_NE(bridge_shape, nullptr);
  ASSERT_NE(trunk_snout, nullptr);
  float const head_front_z = head_shape->center.z() + head_shape->radii.z();
  float const bridge_front_z = bridge_shape->center.z() + bridge_shape->radii.z();
  EXPECT_LT(bridge_shape->center.z(), head_front_z);
  EXPECT_GT(bridge_front_z, head_front_z);
  EXPECT_LT(bridge_front_z, head_front_z + head_shape->radii.z() * 0.03F);
  EXPECT_GT(bridge_front_z, trunk_snout->start.z() - 0.03F);
  EXPECT_GT(bridge_shape->radii.y(), head_shape->radii.y() * 0.12F);
  EXPECT_GT(bridge_shape->radii.z(), head_shape->radii.z() * 0.12F);

  std::size_t eye_count = 0U;
  for (auto const &node : manifest.lod_full.mesh_nodes) {
    if (node.debug_name != std::string_view{"elephant.eye"}) {
      continue;
    }
    auto const *eye =
        std::get_if<Render::Creature::Quadruped::EllipsoidNode>(&node.data);
    ASSERT_NE(eye, nullptr);
    EXPECT_EQ(node.color_role, 7U);
    EXPECT_GT(std::abs(eye->center.x()), head_shape->radii.x() * 0.50F);
    EXPECT_LT(std::abs(eye->center.x()), head_shape->radii.x() * 0.58F);
    EXPECT_GT(eye->center.z() + eye->radii.z(),
              head_front_z - head_shape->radii.z() * 0.03F);
    EXPECT_LT(eye->center.z() + eye->radii.z(),
              head_front_z + head_shape->radii.z() * 0.005F);
    EXPECT_GT(eye->radii.x(), 0.025F);
    ++eye_count;
  }

  EXPECT_EQ(eye_count, 2U);
}

TEST(ElephantSpecTest, ManifestTailSitsSlightlyHigherAndFurtherBack) {
  auto const &manifest = Render::Elephant::elephant_manifest();
  auto const *tail =
      find_mesh_node(manifest.lod_full.mesh_nodes, "elephant.tail");

  ASSERT_NE(tail, nullptr);
  auto const *tail_tube =
      std::get_if<Render::Creature::Quadruped::TubeNode>(&tail->data);
  ASSERT_NE(tail_tube, nullptr);

  EXPECT_GT(tail_tube->start.y(), 0.08F);
  EXPECT_GT(tail_tube->end.y(), -0.45F);
  EXPECT_LT(tail_tube->start.z(), -0.40F);
  EXPECT_LT(tail_tube->end.z(), tail_tube->start.z() - 0.35F);
  EXPECT_LT(tail_tube->start_radius, 0.03F);
  EXPECT_LT(tail_tube->end_radius, 0.04F);
}

TEST(ElephantSpecTest, ManifestKeepsEarsCompact) {
  auto const &manifest = Render::Elephant::elephant_manifest();

  std::size_t ear_count = 0U;
  for (auto const &node : manifest.lod_full.mesh_nodes) {
    if (node.debug_name != std::string_view{"elephant.ear"}) {
      continue;
    }
    auto const *ear =
        std::get_if<Render::Creature::Quadruped::FlatFanNode>(&node.data);
    ASSERT_NE(ear, nullptr);
    EXPECT_LT(outline_axis_span(ear->outline, 0), 0.60F);
    EXPECT_LT(outline_axis_span(ear->outline, 1), 0.90F);
    ++ear_count;
  }

  EXPECT_EQ(ear_count, 2U);
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

TEST(ElephantSpecTest, WalkFootLandsAheadOfShoulderDuringSwing) {
  // The foot should land noticeably in front of the shoulder's z position
  // during the swing phase of a walk, giving the elephant a reaching gait
  // rather than vertical piston-like steps.
  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = 0.25F;
  motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, motion, pose);

  QVector3D const shoulder_fl =
      pose.barrel_center + pose.shoulder_offset_pose_fl;

  // Foot must be in front of (larger z than) the shoulder during forward swing.
  EXPECT_GT(pose.foot_fl.z(), shoulder_fl.z());
  // And the forward reach must be at least 10% of the stride swing extent.
  EXPECT_GT(pose.foot_fl.z() - shoulder_fl.z(), dims.body_length * 0.02F);
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

TEST(ElephantSpecTest, FightPoseTrunkRaisedAboveIdlePose) {
  auto const dims = make_dims();
  auto const gait = make_gait();

  Render::Elephant::ElephantSpecPose fight_pose{};
  Render::Elephant::ElephantPoseMotion fight_motion{};
  fight_motion.anim_time = 0.5F;
  fight_motion.is_fighting = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, fight_motion,
                                                     fight_pose);

  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::ElephantPoseMotion idle_motion{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, idle_motion,
                                                     idle_pose);

  // Trunk tip must be raised well above its idle position during combat.
  EXPECT_GT(fight_pose.trunk_end.y(),
            idle_pose.trunk_end.y() + dims.trunk_length * 0.30F);
}

TEST(ElephantSpecTest, MovingFrontLegLiftExceedsRearAtPeakSwing) {
  auto const dims = make_dims();
  Render::GL::ElephantGait gait{};
  gait.cycle_time = 1.2F;
  gait.front_leg_phase = 0.0F;
  gait.rear_leg_phase = 0.5F;
  gait.stride_swing = 0.30F;
  gait.stride_lift = 0.12F;

  // Idle reference (no motion)
  Render::Elephant::ElephantSpecPose idle_pose{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait,
                                                     Render::Elephant::ElephantPoseMotion{},
                                                     idle_pose);

  // At phase=0.25: FL has leg_phase=0.25 (peak swing), BR has leg_phase=0.25
  // (peak swing); FR has leg_phase=0.75 (grounded), BL has leg_phase=0.75.
  Render::Elephant::ElephantSpecPose swing_pose{};
  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = 0.25F;
  motion.is_moving = true;
  Render::Elephant::make_elephant_spec_pose_animated(dims, gait, motion,
                                                     swing_pose);

  float const front_lift = swing_pose.foot_pose_fl.y() - idle_pose.foot_pose_fl.y();
  float const rear_lift = swing_pose.foot_pose_br.y() - idle_pose.foot_pose_br.y();

  EXPECT_GT(front_lift, 0.0F);
  EXPECT_GT(rear_lift, 0.0F);
  // Front leg must lift measurably higher than rear leg.
  EXPECT_GT(front_lift, rear_lift);
}
