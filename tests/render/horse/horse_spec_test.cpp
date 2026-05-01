

#include "render/creature/spec.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_layout.h"
#include "render/horse/horse_manifest.h"
#include "render/horse/horse_profile_data.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <gtest/gtest.h>
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

Render::GL::HorseDimensions make_horse_dims() {
  Render::GL::HorseDimensions d{};
  d.body_length = 1.4F;
  d.body_width = 0.24F;
  d.body_height = 0.60F;
  d.barrel_center_y = 1.10F;
  d.neck_length = 0.50F;
  d.neck_rise = 0.20F;
  d.head_length = 0.42F;
  d.head_width = 0.18F;
  d.head_height = 0.24F;
  d.muzzle_length = 0.20F;
  d.leg_length = 0.90F;
  d.hoof_height = 0.08F;
  d.tail_length = 0.58F;
  return d;
}

Render::GL::HorseVariant make_horse_variant() {
  Render::GL::HorseVariant v{};
  v.coat_color = QVector3D(0.55F, 0.35F, 0.20F);
  v.hoof_color = QVector3D(0.10F, 0.08F, 0.05F);
  return v;
}

Render::GL::HorseGait make_horse_gait() {
  Render::GL::HorseGait g{};
  g.cycle_time = 1.0F;
  g.front_leg_phase = 0.0F;
  g.rear_leg_phase = 0.5F;
  g.stride_swing = 0.30F;
  g.stride_lift = 0.12F;
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

auto count_primitives(
    std::span<const Render::Creature::PrimitiveInstance> prims,
    std::string_view prefix) -> std::size_t {
  return static_cast<std::size_t>(
      std::count_if(prims.begin(), prims.end(), [&](auto const &prim) {
        return prim.debug_name.rfind(prefix, 0) == 0;
      }));
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

auto mesh_axis_min(const Render::GL::Mesh &mesh, std::size_t axis) -> float {
  auto const &vertices = mesh.get_vertices();
  if (vertices.empty()) {
    return 0.0F;
  }
  float min_v = vertices.front().position[axis];
  for (auto const &vertex : vertices) {
    min_v = std::min(min_v, vertex.position[axis]);
  }
  return min_v;
}

auto mesh_axis_max(const Render::GL::Mesh &mesh, std::size_t axis) -> float {
  auto const &vertices = mesh.get_vertices();
  if (vertices.empty()) {
    return 0.0F;
  }
  float max_v = vertices.front().position[axis];
  for (auto const &vertex : vertices) {
    max_v = std::max(max_v, vertex.position[axis]);
  }
  return max_v;
}

auto max_axis_in_z_band(const Render::GL::Mesh &mesh, float center_z,
                        float half_band, std::size_t axis) -> float {
  bool found = false;
  float best = 0.0F;
  for (auto const &vertex : mesh.get_vertices()) {
    if (std::abs(vertex.position[2] - center_z) > half_band) {
      continue;
    }
    if (!found || vertex.position[axis] > best) {
      best = vertex.position[axis];
      found = true;
    }
  }
  return found ? best : mesh_axis_max(mesh, axis);
}

auto min_axis_in_z_band(const Render::GL::Mesh &mesh, float center_z,
                        float half_band, std::size_t axis) -> float {
  bool found = false;
  float best = 0.0F;
  for (auto const &vertex : mesh.get_vertices()) {
    if (std::abs(vertex.position[2] - center_z) > half_band) {
      continue;
    }
    if (!found || vertex.position[axis] < best) {
      best = vertex.position[axis];
      found = true;
    }
  }
  return found ? best : mesh_axis_min(mesh, axis);
}

auto max_abs_axis_in_z_band(const Render::GL::Mesh &mesh, float center_z,
                            float half_band, std::size_t axis) -> float {
  float best = 0.0F;
  for (auto const &vertex : mesh.get_vertices()) {
    if (std::abs(vertex.position[2] - center_z) > half_band) {
      continue;
    }
    best = std::max(best, std::abs(vertex.position[axis]));
  }
  return best;
}

auto max_ring_radius_at_y(const Render::GL::Mesh &mesh,
                          bool anchor_end) -> float {
  auto const &vertices = mesh.get_vertices();
  float max_radius = 0.0F;
  for (auto const &vertex : vertices) {
    float const y = vertex.position[1];
    if (anchor_end ? (y > -0.49F) : (y < 0.49F)) {
      continue;
    }
    float const radius = std::sqrt(vertex.position[0] * vertex.position[0] +
                                   vertex.position[2] * vertex.position[2]);
    max_radius = std::max(max_radius, radius);
  }
  return max_radius;
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

TEST(HorseSpecTest, CreatureSpecHasTwoLods) {
  auto const &spec = Render::Horse::horse_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 1U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 13U);
}

TEST(HorseSpecTest, TopologyParentsLegsAndNeckFromBody) {
  auto const &topology = Render::Horse::horse_topology();
  using Bone = Render::Horse::HorseBone;

  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::ShoulderFL)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::Body));
  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::ShoulderFR)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::Body));
  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::ShoulderBL)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::Body));
  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::ShoulderBR)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::Body));
  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::NeckTop)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::Body));
  EXPECT_EQ(topology.bones[static_cast<std::size_t>(Bone::Head)].parent,
            static_cast<Render::Creature::BoneIndex>(Bone::NeckTop));
}

TEST(HorseSpecTest,
     BasePoseKeepsHindquartersBroaderAndHindFeetTuckedUnderCroup) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  EXPECT_GT(pose.shoulder_offset_bl.x(), pose.shoulder_offset_fl.x() * 1.10F);
  EXPECT_GT(pose.shoulder_offset_bl.x(), dims.body_width * 0.22F);
  EXPECT_GT(pose.shoulder_offset_fl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_bl.z(), 0.0F);
  EXPECT_GT(pose.shoulder_offset_fl.y(), 0.0F);
  EXPECT_GT(pose.shoulder_offset_bl.y(), 0.0F);
  EXPECT_GT(pose.foot_fl.z(), pose.shoulder_offset_fl.z());
  EXPECT_GT(pose.foot_bl.z(), pose.shoulder_offset_bl.z());
  EXPECT_GT(pose.foot_br.z(), pose.shoulder_offset_br.z());
  EXPECT_GT(pose.leg_radius, dims.body_width * 0.14F);
  EXPECT_LT(pose.leg_radius, dims.body_width * 0.24F);
}

TEST(HorseSpecTest, AnimatedPoseKeepsLegsOutsideTorsoSpread) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, false}, pose);
  QVector3D const shoulder_fl =
      pose.barrel_center + pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_bl =
      pose.barrel_center + pose.shoulder_offset_pose_bl;

  EXPECT_GT(pose.body_half.x(), dims.body_width * 0.85F);
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_fl.x()),
            dims.body_width * 0.20F);
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_bl.x()),
            dims.body_width * 0.16F);
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_bl.x()),
            std::abs(pose.shoulder_offset_pose_fl.x()));
  EXPECT_GT(pose.shoulder_offset_pose_fl.y(), 0.0F);
  EXPECT_GT(pose.shoulder_offset_pose_bl.y(), 0.0F);
  EXPECT_GT(pose.shoulder_offset_pose_fl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_pose_bl.z(), 0.0F);
  EXPECT_LT(pose.foot_fl.y(), shoulder_fl.y() - dims.leg_length * 0.95F);
  EXPECT_LT(pose.foot_bl.y(), shoulder_bl.y() - dims.leg_length * 0.95F);
  EXPECT_GT(pose.neck_base.y() - pose.barrel_center.y(),
            dims.body_height * 0.18F);
  EXPECT_GE(pose.neck_base.z(), dims.body_length * 0.35F);
  EXPECT_GT(pose.neck_top.y() - pose.neck_base.y(), dims.neck_rise * 2.0F);
  EXPECT_GT(pose.neck_top.z(), pose.neck_base.z() + dims.neck_length * 0.60F);
  EXPECT_GT(pose.head_center.z(), pose.neck_top.z() + dims.head_length * 0.28F);
  EXPECT_LT(pose.head_center.z(), pose.neck_top.z() + dims.head_length * 0.75F);
  EXPECT_GT(pose.head_center.y(), pose.neck_top.y());
  EXPECT_GT(pose.neck_radius, dims.body_width * 0.12F);
  EXPECT_LT(pose.neck_radius, dims.body_width * 0.50F);
  EXPECT_LT(pose.neck_radius, pose.body_half.x());
  EXPECT_GT(pose.body_half.y(), dims.body_height);
  EXPECT_LT(pose.body_half.y(),
            Render::Horse::horse_body_visual_height(dims) * 0.75F);
  EXPECT_GT(pose.head_half.z(), pose.head_half.y() * 1.8F);
}

TEST(HorseSpecTest, BasePoseBodyKeepsDepthCloserToWidthThanFlatBlob) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  float const depth_ratio = pose.body_ellipsoid_y / pose.body_ellipsoid_x;
  EXPECT_GT(depth_ratio, 1.60F);
  EXPECT_LT(depth_ratio, 2.45F);
}

TEST(HorseSpecTest, GeneratedHorseDimensionsUseWiderTallerTorso) {
  auto const dims = Render::GL::make_horse_dimensions(0U);

  EXPECT_GE(dims.body_width,
            Render::GL::HorseDimensionRange::k_body_width_min * 1.18F);
  EXPECT_LE(dims.body_width,
            Render::GL::HorseDimensionRange::k_body_width_max * 1.18F);
  EXPECT_GE(dims.body_height,
            Render::GL::HorseDimensionRange::k_body_height_min * 1.2F);
  EXPECT_LE(dims.body_height,
            Render::GL::HorseDimensionRange::k_body_height_max * 1.2F);
}

TEST(HorseSpecTest, FullSpecUsesWarhorseSilhouette) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "horse.full.body");

  ASSERT_NE(whole, nullptr);
  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  float const x_span = mesh_axis_span(*whole->custom_mesh, 0);
  float const y_span = mesh_axis_span(*whole->custom_mesh, 1);
  float const z_span = mesh_axis_span(*whole->custom_mesh, 2);
  EXPECT_GT(z_span, y_span * 0.70F);
  EXPECT_GT(z_span, x_span * 1.40F);
  EXPECT_LT(y_span, z_span * 1.45F);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "horse.full.whole"),
            nullptr);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "horse.full.chest"),
            nullptr);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "horse.full.head.cheek.l"),
            nullptr);
}

TEST(HorseSpecTest, FullSpecForearmStaysHeavierThanRearUpperLeg) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "horse.full.body");

  ASSERT_NE(whole, nullptr);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 1),
            mesh_axis_span(*whole->custom_mesh, 0));
}

TEST(HorseSpecTest, FullSpecUsesSegmentedLegPrimitives) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto const dims = Render::GL::make_horse_dimensions(0U);

  auto const *front_thigh =
      find_primitive(spec.lod_full.primitives, "horse.leg.fl.thigh");
  auto const *front_calf =
      find_primitive(spec.lod_full.primitives, "horse.leg.fl.calf");
  auto const *front_hoof =
      find_primitive(spec.lod_full.primitives, "horse.leg.fl.hoof");
  auto const *rear_thigh =
      find_primitive(spec.lod_full.primitives, "horse.leg.bl.thigh");

  ASSERT_NE(front_thigh, nullptr);
  ASSERT_NE(front_calf, nullptr);
  ASSERT_NE(front_hoof, nullptr);
  ASSERT_NE(rear_thigh, nullptr);
  EXPECT_EQ(front_thigh->shape, Render::Creature::PrimitiveShape::BoneSpanMesh);
  EXPECT_EQ(front_calf->shape, Render::Creature::PrimitiveShape::BoneSpanMesh);
  EXPECT_EQ(front_hoof->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(front_thigh->custom_mesh, nullptr);
  ASSERT_NE(front_calf->custom_mesh, nullptr);
  ASSERT_NE(front_hoof->custom_mesh, nullptr);
  EXPECT_EQ(front_thigh->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Horse::HorseBone::ShoulderFL));
  EXPECT_EQ(front_thigh->params.tail_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Horse::HorseBone::KneeFL));
  EXPECT_EQ(front_calf->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Horse::HorseBone::KneeFL));
  EXPECT_EQ(front_calf->params.tail_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Horse::HorseBone::FootFL));
  EXPECT_EQ(front_hoof->params.anchor_bone,
            static_cast<Render::Creature::BoneIndex>(
                Render::Horse::HorseBone::FootFL));
  EXPECT_EQ(front_hoof->color_role, 4U);
  EXPECT_GT(front_thigh->params.radius, front_calf->params.radius * 1.9F);
  EXPECT_GT(front_thigh->params.radius,
            Render::GL::make_horse_dimensions(0U).body_width * 0.22F);
  EXPECT_GT(front_calf->params.radius,
            Render::GL::make_horse_dimensions(0U).body_width * 0.10F);
  EXPECT_GT(front_thigh->params.head_offset.y(), dims.body_height * 0.82F);
  EXPECT_GE(front_thigh->params.head_offset.z(), dims.body_length * 0.04F);
  EXPECT_GT(front_thigh->params.tail_offset.y(), dims.leg_length * 0.07F);
  EXPECT_GT(front_thigh->params.tail_offset.z(), dims.body_length * 0.03F);
  EXPECT_GT(front_calf->params.head_offset.y(), dims.leg_length * 0.02F);
  EXPECT_GT(front_calf->params.tail_offset.y(), 0.0F);
  EXPECT_LT(front_calf->params.tail_offset.y(), dims.hoof_height * 0.04F);
  EXPECT_GT(rear_thigh->params.head_offset.y(), dims.body_height * 1.00F);
  EXPECT_LE(rear_thigh->params.head_offset.z(), -dims.body_length * 0.06F);
  EXPECT_GT(rear_thigh->params.tail_offset.z(), dims.body_length * 0.027F);
  EXPECT_GT(front_hoof->params.half_extents.x(), dims.body_width * 0.040F);
  EXPECT_GT(front_hoof->params.half_extents.z(), dims.body_width * 0.070F);
  EXPECT_GT(front_hoof->params.half_extents.y(), dims.hoof_height * 0.25F);
  EXPECT_GT(max_ring_radius_at_y(*front_thigh->custom_mesh, true),
            max_ring_radius_at_y(*front_thigh->custom_mesh, false) * 1.7F);
  EXPECT_GT(max_ring_radius_at_y(*front_calf->custom_mesh, true),
            max_ring_radius_at_y(*front_calf->custom_mesh, false) * 1.35F);
  EXPECT_LT(front_thigh->custom_mesh->get_vertices().size(),
            Render::GL::get_unit_cylinder()->get_vertices().size());
  EXPECT_LT(front_calf->custom_mesh->get_vertices().size(),
            Render::GL::get_unit_cylinder()->get_vertices().size());
  EXPECT_EQ(count_primitives(spec.lod_full.primitives, "horse.leg."), 12U);
}

TEST(HorseSpecTest, ManifestUsesFacetedHorseHeadAndEars) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *neck_node = find_node("horse.neck");
  auto const *head_upper_node = find_node("horse.head.upper");
  auto const *ear_l_node = find_node("horse.ear.l");

  ASSERT_NE(neck_node, nullptr);
  ASSERT_NE(head_upper_node, nullptr);
  ASSERT_NE(ear_l_node, nullptr);
  EXPECT_EQ(find_node("horse.eye.l"), nullptr);
  EXPECT_EQ(find_node("horse.eye.r"), nullptr);
  EXPECT_EQ(find_node("horse.head.nose"), nullptr);
  EXPECT_EQ(find_node("horse.muzzle"), nullptr);
  EXPECT_EQ(find_node("horse.head.muzzle"), nullptr);
  EXPECT_EQ(find_node("horse.head.jaw"), nullptr);
  EXPECT_EQ(find_node("horse.head.cheek.l"), nullptr);
  EXPECT_EQ(find_node("horse.head.cheek.r"), nullptr);

  auto const *neck =
      std::get_if<Render::Creature::Quadruped::TubeNode>(&neck_node->data);
  auto const *head_upper =
      std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
          &head_upper_node->data);
  auto const *ear_l =
      std::get_if<Render::Creature::Quadruped::FlatFanNode>(&ear_l_node->data);

  ASSERT_NE(neck, nullptr);
  ASSERT_NE(head_upper, nullptr);
  ASSERT_NE(ear_l, nullptr);
  Render::GL::Mesh head_upper_mesh(head_upper->vertices, head_upper->indices);
  float const head_upper_z_span = mesh_axis_span(head_upper_mesh, 2);
  float const head_upper_y_span = mesh_axis_span(head_upper_mesh, 1);
  EXPECT_GT(neck->start_radius, neck->end_radius);
  EXPECT_GT((neck->end - neck->start).length(), 0.60F);
  EXPECT_GT(neck->end.y(), neck->start.y());
  EXPECT_LT(neck->start_radius, 0.40F);
  EXPECT_GT(head_upper_z_span, head_upper_y_span * 0.90F);
  EXPECT_GE(ear_l->outline.size(), 3U);
  EXPECT_GT(ear_l->outline[1].y(), ear_l->outline[0].y());
}

TEST(HorseSpecTest, ManifestFrontTorsoTopRisesIntoNeck) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *front_node = find_node("horse.body.front");
  auto const *neck_node = find_node("horse.neck");

  ASSERT_NE(front_node, nullptr);
  ASSERT_NE(neck_node, nullptr);

  auto const *front = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
      &front_node->data);
  auto const *neck =
      std::get_if<Render::Creature::Quadruped::TubeNode>(&neck_node->data);

  ASSERT_NE(front, nullptr);
  ASSERT_NE(neck, nullptr);

  Render::GL::Mesh front_mesh(front->vertices, front->indices);
  float const z_min = mesh_axis_min(front_mesh, 2);
  float const z_max = mesh_axis_max(front_mesh, 2);
  float const z_span = z_max - z_min;
  float const band = z_span * 0.18F;
  float const mid_z = z_min + z_span * 0.42F;
  float const fore_z = z_min + z_span * 0.54F;
  float const mid_dorsal = max_axis_in_z_band(front_mesh, mid_z, band, 1);
  float const fore_dorsal = max_axis_in_z_band(front_mesh, fore_z, band, 1);
  float const mid_width = max_abs_axis_in_z_band(front_mesh, mid_z, band, 0);
  float const fore_width = max_abs_axis_in_z_band(front_mesh, fore_z, band, 0);

  EXPECT_GE(fore_dorsal, mid_dorsal);
  EXPECT_GT(fore_width, mid_width * 0.60F);
  EXPECT_GE(neck->start.z(), z_min + z_span * 0.32F);
  EXPECT_LE(neck->start.z(), z_max + 0.10F);
}

TEST(HorseSpecTest, ManifestTorsoSectionsUseFourFifthsOriginalLength) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *rear_node = find_node("horse.body.rear");
  auto const *mid_node = find_node("horse.body.mid");
  auto const *front_node = find_node("horse.body.front");

  ASSERT_NE(rear_node, nullptr);
  ASSERT_NE(mid_node, nullptr);
  ASSERT_NE(front_node, nullptr);

  auto const *rear = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
      &rear_node->data);
  auto const *mid =
      std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&mid_node->data);
  auto const *front = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
      &front_node->data);

  ASSERT_NE(rear, nullptr);
  ASSERT_NE(mid, nullptr);
  ASSERT_NE(front, nullptr);

  Render::GL::Mesh rear_mesh(rear->vertices, rear->indices);
  Render::GL::Mesh mid_mesh(mid->vertices, mid->indices);
  Render::GL::Mesh front_mesh(front->vertices, front->indices);
  float const body_length = Render::Horse::horse_body_visual_length(
      Render::GL::make_horse_dimensions(0U));
  float const expected_section_span = body_length * 0.48F * 0.80F;

  EXPECT_NEAR(mesh_axis_span(rear_mesh, 2), expected_section_span, 1.0e-4F);
  EXPECT_NEAR(mesh_axis_span(mid_mesh, 2), expected_section_span, 1.0e-4F);
  EXPECT_NEAR(mesh_axis_span(front_mesh, 2), expected_section_span, 1.0e-4F);
}

TEST(HorseSpecTest, ManifestRearTorsoRoundsIntoCroupInsteadOfFlatTube) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *rear_node = find_node("horse.body.rear");
  auto const *mid_node = find_node("horse.body.mid");
  ASSERT_NE(rear_node, nullptr);
  ASSERT_NE(mid_node, nullptr);
  EXPECT_EQ(find_node("horse.body.core"), nullptr);
  EXPECT_EQ(find_node("horse.body.croup"), nullptr);
  EXPECT_EQ(find_node("horse.body.haunch.l"), nullptr);
  EXPECT_EQ(find_node("horse.body.haunch.r"), nullptr);

  auto const *rear = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
      &rear_node->data);
  auto const *mid =
      std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&mid_node->data);
  ASSERT_NE(rear, nullptr);
  ASSERT_NE(mid, nullptr);

  Render::GL::Mesh rear_mesh(rear->vertices, rear->indices);
  Render::GL::Mesh mid_mesh(mid->vertices, mid->indices);
  float const rear_z_min = mesh_axis_min(rear_mesh, 2);
  float const rear_z_max = mesh_axis_max(rear_mesh, 2);
  float const rear_z_span = rear_z_max - rear_z_min;
  float const rear_band = rear_z_span * 0.14F;
  float const tail_z = rear_z_min + rear_z_span * 0.16F;
  float const croup_z = rear_z_min + rear_z_span * 0.50F;
  float const flank_z = rear_z_min + rear_z_span * 0.82F;
  float const mid_z =
      mesh_axis_min(mid_mesh, 2) + mesh_axis_span(mid_mesh, 2) * 0.18F;

  float const tail_width =
      max_abs_axis_in_z_band(rear_mesh, tail_z, rear_band, 0);
  float const croup_width =
      max_abs_axis_in_z_band(rear_mesh, croup_z, rear_band, 0);
  float const flank_width =
      max_abs_axis_in_z_band(rear_mesh, flank_z, rear_band, 0);
  float const tail_dorsal = max_axis_in_z_band(rear_mesh, tail_z, rear_band, 1);
  float const croup_dorsal =
      max_axis_in_z_band(rear_mesh, croup_z, rear_band, 1);
  float const tail_ventral =
      min_axis_in_z_band(rear_mesh, tail_z, rear_band, 1);
  float const croup_ventral =
      min_axis_in_z_band(rear_mesh, croup_z, rear_band, 1);
  float const flank_ventral =
      min_axis_in_z_band(rear_mesh, flank_z, rear_band, 1);
  float const mid_ventral = min_axis_in_z_band(
      mid_mesh, mid_z, mesh_axis_span(mid_mesh, 2) * 0.16F, 1);

  EXPECT_LT(tail_width, croup_width);
  EXPECT_GT(croup_width, flank_width);
  EXPECT_GT(croup_dorsal, tail_dorsal);
  EXPECT_GT(tail_ventral + 0.01F, croup_ventral);
  EXPECT_GT(flank_ventral, mid_ventral);
  EXPECT_GT(rear_z_max, mesh_axis_min(mid_mesh, 2));
  EXPECT_LT(rear_z_max - mesh_axis_min(mid_mesh, 2), 0.50F);
}

TEST(HorseSpecTest, ManifestUsesTailDockAndHairInsteadOfSingleTube) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  EXPECT_EQ(find_node("horse.tail"), nullptr);

  auto const *dock_node = find_node("horse.tail.dock");
  auto const *hair_node = find_node("horse.tail.hair");
  ASSERT_NE(dock_node, nullptr);
  ASSERT_NE(hair_node, nullptr);

  auto const *dock =
      std::get_if<Render::Creature::Quadruped::ConeNode>(&dock_node->data);
  auto const *hair =
      std::get_if<Render::Creature::Quadruped::FlatFanNode>(&hair_node->data);
  ASSERT_NE(dock, nullptr);
  ASSERT_NE(hair, nullptr);
  ASSERT_GE(hair->outline.size(), 4U);

  EXPECT_GT(dock->base_radius, 0.01F);
  EXPECT_LT(dock->tip.y(), dock->base_center.y());
  EXPECT_LT(dock->tip.z(), dock->base_center.z());
  EXPECT_LT(hair->outline[1].y(), hair->outline[0].y());
  EXPECT_LT(hair->outline[2].y(), hair->outline[1].y());
  EXPECT_LT(hair->outline[2].z(), hair->outline[1].z());
  EXPECT_GT(hair->thickness, 0.01F);
}

TEST(HorseSpecTest, ManifestAddsWithersAndBrisketMassForForequarterRead) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *front_node = find_node("horse.body.front");
  auto const *mid_node = find_node("horse.body.mid");
  auto const *neck_node = find_node("horse.neck");

  ASSERT_NE(front_node, nullptr);
  ASSERT_NE(mid_node, nullptr);
  ASSERT_NE(neck_node, nullptr);
  EXPECT_EQ(find_node("horse.chest"), nullptr);
  EXPECT_EQ(find_node("horse.withers"), nullptr);
  EXPECT_EQ(find_node("horse.body.back"), nullptr);
  EXPECT_EQ(find_node("horse.body.belly"), nullptr);
  EXPECT_EQ(find_node("horse.body.shoulder.l"), nullptr);
  EXPECT_EQ(find_node("horse.body.shoulder.r"), nullptr);

  auto const *front = std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
      &front_node->data);
  auto const *mid =
      std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&mid_node->data);
  auto const *neck =
      std::get_if<Render::Creature::Quadruped::TubeNode>(&neck_node->data);

  ASSERT_NE(front, nullptr);
  ASSERT_NE(mid, nullptr);
  ASSERT_NE(neck, nullptr);

  Render::GL::Mesh front_mesh(front->vertices, front->indices);
  Render::GL::Mesh mid_mesh(mid->vertices, mid->indices);
  float const front_z_min = mesh_axis_min(front_mesh, 2);
  float const front_z_span = mesh_axis_span(front_mesh, 2);
  float const mid_z_min = mesh_axis_min(mid_mesh, 2);
  float const mid_z_span = mesh_axis_span(mid_mesh, 2);
  float const brisket_low = min_axis_in_z_band(
      front_mesh, front_z_min + front_z_span * 0.28F, front_z_span * 0.14F, 1);
  float const belly_tuck = min_axis_in_z_band(
      mid_mesh, mid_z_min + mid_z_span * 0.48F, mid_z_span * 0.16F, 1);
  float const mid_fore_dorsal = max_axis_in_z_band(
      front_mesh, front_z_min + front_z_span * 0.44F, front_z_span * 0.14F, 1);
  float const withers_high = max_axis_in_z_band(
      front_mesh, front_z_min + front_z_span * 0.76F, front_z_span * 0.14F, 1);

  EXPECT_LT(brisket_low, belly_tuck);
  EXPECT_GT(withers_high, mid_fore_dorsal);
  EXPECT_GT(neck->start.z(), front_z_min + front_z_span * 0.30F);
  EXPECT_LT(neck->start.z(), mesh_axis_max(front_mesh, 2) + 0.10F);
}

TEST(HorseSpecTest, ManifestKeepsLongerMoreTaperedMuzzleProfile) {
  auto const &manifest = Render::Horse::horse_manifest();

  auto const find_node = [&](std::string_view name)
      -> const Render::Creature::Quadruped::MeshNode * {
    auto it =
        std::find_if(manifest.lod_full.mesh_nodes.begin(),
                     manifest.lod_full.mesh_nodes.end(),
                     [&](auto const &node) { return node.debug_name == name; });
    return it == manifest.lod_full.mesh_nodes.end() ? nullptr : &*it;
  };

  auto const *head_upper_node = find_node("horse.head.upper");
  ASSERT_NE(head_upper_node, nullptr);
  EXPECT_EQ(find_node("horse.head.nose"), nullptr);

  auto const *head_upper =
      std::get_if<Render::Creature::Quadruped::CustomMeshNode>(
          &head_upper_node->data);

  ASSERT_NE(head_upper, nullptr);

  Render::GL::Mesh head_upper_mesh(head_upper->vertices, head_upper->indices);
  float const head_z_min = mesh_axis_min(head_upper_mesh, 2);
  float const head_z_span = mesh_axis_span(head_upper_mesh, 2);
  float const head_band = head_z_span * 0.14F;
  float const muzzle_base_z = head_z_min + head_z_span * 0.56F;
  float const muzzle_tip_z = head_z_min + head_z_span * 0.92F;
  float const base_width =
      max_abs_axis_in_z_band(head_upper_mesh, muzzle_base_z, head_band, 0);
  float const tip_width =
      max_abs_axis_in_z_band(head_upper_mesh, muzzle_tip_z, head_band, 0);
  float const base_top =
      max_axis_in_z_band(head_upper_mesh, muzzle_base_z, head_band, 1);
  float const tip_top =
      max_axis_in_z_band(head_upper_mesh, muzzle_tip_z, head_band, 1);
  float const base_bottom =
      min_axis_in_z_band(head_upper_mesh, muzzle_base_z, head_band, 1);
  float const tip_bottom =
      min_axis_in_z_band(head_upper_mesh, muzzle_tip_z, head_band, 1);

  EXPECT_GT(mesh_axis_max(head_upper_mesh, 2),
            head_z_min + head_z_span * 0.90F);
  EXPECT_LT(tip_width, base_width * 0.75F);
  EXPECT_LT(tip_top - tip_bottom, (base_top - base_bottom) * 0.70F);
  EXPECT_LT((tip_top + tip_bottom) * 0.5F, (base_top + base_bottom) * 0.5F);
}

TEST(HorseSpecTest, FullSpecFrontHoofCentersUnderFrontCannon) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *hoof =
      find_primitive(spec.lod_full.primitives, "horse.leg.fl.hoof");

  ASSERT_NE(hoof, nullptr);
  EXPECT_EQ(hoof->shape, Render::Creature::PrimitiveShape::Mesh);
  EXPECT_GT(hoof->params.half_extents.z(), hoof->params.half_extents.x());
  EXPECT_GT(hoof->params.head_offset.y(),
            hoof->params.half_extents.y() * 0.90F);
  EXPECT_GT(hoof->params.half_extents.x(),
            Render::GL::make_horse_dimensions(0U).body_width * 0.055F);
  EXPECT_GT(hoof->params.half_extents.y(),
            Render::GL::make_horse_dimensions(0U).hoof_height * 0.32F);
}

TEST(HorseSpecTest, MinimalSpecMatchesWarhorseSilhouette) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_minimal.primitives, "horse.minimal.whole");

  ASSERT_NE(whole, nullptr);
  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  float const x_span = mesh_axis_span(*whole->custom_mesh, 0);
  float const y_span = mesh_axis_span(*whole->custom_mesh, 1);
  float const z_span = mesh_axis_span(*whole->custom_mesh, 2);
  EXPECT_GT(z_span, y_span * 0.70F);
  EXPECT_GT(z_span, x_span * 1.40F);
  EXPECT_EQ(find_primitive(spec.lod_minimal.primitives, "horse.minimal.leg.fl"),
            nullptr);
}

TEST(HorseSpecTest, AnimatedWalkKeepsRearHoofLowerDuringStanceThanSwing) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  gait.cycle_time = 1.1F;
  gait.front_leg_phase = 0.25F;
  gait.rear_leg_phase = 0.0F;
  gait.stride_swing = 0.34F;
  gait.stride_lift = 0.14F;

  Render::Horse::HorseSpecPose stance_pose;
  Render::Horse::HorseSpecPose swing_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, true},
      stance_pose);
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.75F, 0.0F, true},
      swing_pose);

  EXPECT_LT(stance_pose.foot_bl.y(), swing_pose.foot_bl.y());
  EXPECT_GT(swing_pose.foot_bl.y() - stance_pose.foot_bl.y(), 0.05F);
}

TEST(HorseSpecTest,
     AnimatedPoseBendsFrontKneesForwardAndRearKneesBackDuringMotion) {
  auto dims = make_horse_dims();
  auto gait =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.70F, 0.0F, true}, pose);

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
            dims.body_length * 0.030F);
  EXPECT_LT(point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl),
            dims.body_length * 0.27F);

  EXPECT_LT(pose.knee_bl.y(), shoulder_bl.y());
  EXPECT_GT(pose.knee_bl.y(), pose.foot_bl.y());
  EXPECT_GT(pose.knee_bl.z(), shoulder_bl.z());
  EXPECT_LT(pose.knee_bl.z(), rear_mid_z);
  EXPECT_LT(pose.knee_bl.z(), pose.foot_bl.z());
  EXPECT_GT(point_to_segment_distance(pose.knee_bl, shoulder_bl, pose.foot_bl),
            dims.body_length * 0.004F);
  EXPECT_LT(point_to_segment_distance(pose.knee_bl, shoulder_bl, pose.foot_bl),
            dims.body_length * 0.24F);
}

TEST(HorseSpecTest, WalkAndTrotProduceDifferentKneeFlexProfiles) {
  auto dims = make_horse_dims();
  auto walk =
      Render::GL::gait_for_type(Render::GL::GaitType::WALK, make_horse_gait());
  auto trot =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  Render::Horse::HorseSpecPose walk_pose;
  Render::Horse::HorseSpecPose trot_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, walk, Render::Horse::HorsePoseMotion{0.72F, 0.0F, true}, walk_pose);
  Render::Horse::make_horse_spec_pose_animated(
      dims, trot, Render::Horse::HorsePoseMotion{0.72F, 0.0F, true}, trot_pose);

  QVector3D const walk_shoulder =
      walk_pose.barrel_center + walk_pose.shoulder_offset_pose_fl;
  QVector3D const trot_shoulder =
      trot_pose.barrel_center + trot_pose.shoulder_offset_pose_fl;
  float const walk_bend = point_to_segment_distance(
      walk_pose.knee_fl, walk_shoulder, walk_pose.foot_fl);
  float const trot_bend = point_to_segment_distance(
      trot_pose.knee_fl, trot_shoulder, trot_pose.foot_fl);

  EXPECT_GT(trot_bend, walk_bend * 1.03F);
}

TEST(HorseSpecTest, IdlePoseKeepsVisibleFrontKneeBreak) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QVector3D const shoulder_fl = pose.barrel_center + pose.shoulder_offset_fl;
  float const front_bend =
      point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl);

  EXPECT_GT(front_bend, dims.body_length * 0.012F);
  EXPECT_LT(front_bend, dims.body_length * 0.12F);
  EXPECT_GT(pose.knee_fl.z(), (shoulder_fl.z() + pose.foot_fl.z()) * 0.5F);
}

TEST(HorseSpecTest, FightPoseRaisesFrontFeetHigherThanIdlePose) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose fight_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, true},
      fight_pose);

  Render::Horse::HorseSpecPose idle_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, false},
      idle_pose);

  EXPECT_GT(fight_pose.foot_fl.y() - idle_pose.foot_fl.y(),
            dims.leg_length * 0.25F);

  EXPECT_NEAR(fight_pose.foot_bl.y(), idle_pose.foot_bl.y(),
              dims.leg_length * 0.05F);
  EXPECT_NEAR(fight_pose.foot_br.y(), idle_pose.foot_br.y(),
              dims.leg_length * 0.05F);
}

TEST(HorseSpecTest, FightPoseStrikePhaseLowersFrontFeetBelowPeak) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose peak_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, true},
      peak_pose);

  Render::Horse::HorseSpecPose strike_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.72F, 0.0F, false, true},
      strike_pose);

  EXPECT_LT(strike_pose.foot_fl.y(), peak_pose.foot_fl.y());
}

TEST(HorseSpecTest, MovementBobVariesAcrossFrames) {
  auto dims = make_horse_dims();
  dims.move_bob_amplitude = 0.030F;
  auto gait =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  Render::Horse::HorseSpecPose pose_zero;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, true, false},
      pose_zero);

  float const max_bob = dims.move_bob_amplitude * 0.85F;
  Render::Horse::HorseSpecPose pose_peak;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.25F, max_bob, true, false},
      pose_peak);

  EXPECT_GT(pose_peak.barrel_center.y(), pose_zero.barrel_center.y());
}

TEST(HorseSpecTest, AnimatedIdleKeepsFrontKneeSlightlyBent) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, false, false},
      pose);

  QVector3D const shoulder_fl =
      pose.barrel_center + pose.shoulder_offset_pose_fl;
  float const front_bend =
      point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl);

  EXPECT_GT(front_bend, dims.body_length * 0.004F);
  EXPECT_GT(pose.knee_fl.z(), shoulder_fl.z());
  EXPECT_LT(pose.knee_fl.z(), pose.foot_fl.z());
}

TEST(HorseSpecTest, FightPoseNeckArchedHigherThanIdlePose) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose fight_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.5F, 0.0F, false, true},
      fight_pose);

  Render::Horse::HorseSpecPose idle_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.5F, 0.0F, false, false},
      idle_pose);

  EXPECT_GT(fight_pose.neck_top.y(), idle_pose.neck_top.y());
  EXPECT_GT(fight_pose.neck_top.y() - idle_pose.neck_top.y(),
            dims.neck_rise * 0.10F);

  float const fight_head_drop =
      fight_pose.neck_top.y() - fight_pose.head_center.y();
  float const idle_head_drop =
      idle_pose.neck_top.y() - idle_pose.head_center.y();
  EXPECT_GT(fight_head_drop, idle_head_drop);
}
