// Stage 16.5 — horse rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_horse_lod`) was removed when
// the imperative LOD path was retired; the rig now exclusively calls
// the rigged wrappers. With a non-Renderer submitter we exercise the
// software fallback inside `submit_horse_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (5/29/43 for Minimal/Reduced/Full).

#include "render/creature/spec.h"
#include "render/gl/mesh.h"
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

} // namespace

TEST(HorseSpecTest, CreatureSpecHasAllThreeLods) {
  auto const &spec = Render::Horse::horse_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 5U);
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 29U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 43U);
}

TEST(HorseSpecTest, BasePoseKeepsForehandBroaderAndHindFeetTuckedUnderCroup) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  EXPECT_GT(pose.shoulder_offset_fl.x(), pose.shoulder_offset_bl.x() * 1.35F);
  EXPECT_GT(pose.shoulder_offset_fl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_bl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_bl.y(),
            pose.shoulder_offset_fl.y() - dims.body_height * 0.06F);
  EXPECT_GT(pose.foot_fl.z(), pose.foot_bl.z());
  EXPECT_LT(pose.leg_radius, dims.body_width * 0.14F);
}

TEST(HorseSpecTest, ReducedPoseKeepsLegsOutsideNarrowerTorsoRead) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, false}, pose);

  EXPECT_LT(pose.reduced_body_half.x(), dims.body_width * 0.75F);
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_fl.x()),
            dims.body_width * 0.65F);
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_fl.x()),
            std::abs(pose.shoulder_offset_reduced_bl.x()));
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_bl.x()),
            dims.body_width * 0.50F);
  EXPECT_LT(pose.shoulder_offset_reduced_fl.y(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_reduced_bl.y(),
            pose.shoulder_offset_reduced_fl.y() - dims.body_height * 0.04F);
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y() - dims.leg_length * 1.04F);
  EXPECT_LT(pose.foot_bl.y(), pose.barrel_center.y() - dims.leg_length * 1.00F);
  EXPECT_GT(pose.neck_top.z(), pose.neck_base.z() + dims.neck_length * 0.88F);
  EXPECT_GT(pose.head_center.z(), pose.neck_top.z() + dims.head_length * 0.64F);
  EXPECT_LT(pose.neck_radius, pose.reduced_body_half.x());
}

TEST(HorseSpecTest, BasePoseBodyKeepsDepthCloserToWidthThanFlatBlob) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  float const depth_ratio = pose.body_ellipsoid_y / pose.body_ellipsoid_x;
  EXPECT_GT(depth_ratio, 0.85F);
  EXPECT_LT(depth_ratio, 1.15F);
}

TEST(HorseSpecTest, ReducedSpecSeparatesChestRumpAndTailSilhouette) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *ribcage =
      find_primitive(spec.lod_reduced.primitives, "horse.body.ribcage.reduced");
  auto const *chest =
      find_primitive(spec.lod_reduced.primitives, "horse.body.chest.reduced");
  auto const *pelvis =
      find_primitive(spec.lod_reduced.primitives, "horse.body.pelvis.reduced");
  auto const *belly =
      find_primitive(spec.lod_reduced.primitives, "horse.body.belly.reduced");
  auto const *withers =
      find_primitive(spec.lod_reduced.primitives, "horse.withers.reduced");
  auto const *neck =
      find_primitive(spec.lod_reduced.primitives, "horse.neck.reduced");
  auto const *mane =
      find_primitive(spec.lod_reduced.primitives, "horse.mane.reduced");
  auto const *cranium =
      find_primitive(spec.lod_reduced.primitives, "horse.head.cranium.reduced");
  auto const *muzzle =
      find_primitive(spec.lod_reduced.primitives, "horse.head.muzzle.reduced");
  auto const *jaw =
      find_primitive(spec.lod_reduced.primitives, "horse.head.jaw.reduced");
  auto const *ear_l =
      find_primitive(spec.lod_reduced.primitives, "horse.head.ear.l.reduced");
  auto const *ear_r =
      find_primitive(spec.lod_reduced.primitives, "horse.head.ear.r.reduced");
  auto const *tail =
      find_primitive(spec.lod_reduced.primitives, "horse.tail.reduced");
  auto const *front_upper =
      find_primitive(spec.lod_reduced.primitives, "horse.leg.fl.upper.r");
  auto const *front_knee =
      find_primitive(spec.lod_reduced.primitives, "horse.leg.fl.knee.r");
  auto const *front_lower =
      find_primitive(spec.lod_reduced.primitives, "horse.leg.fl.lower.r");
  auto const *front_hoof =
      find_primitive(spec.lod_reduced.primitives, "horse.hoof.fl.r");

  ASSERT_NE(ribcage, nullptr);
  ASSERT_NE(chest, nullptr);
  ASSERT_NE(pelvis, nullptr);
  ASSERT_NE(belly, nullptr);
  ASSERT_NE(withers, nullptr);
  ASSERT_NE(neck, nullptr);
  ASSERT_NE(mane, nullptr);
  ASSERT_NE(cranium, nullptr);
  ASSERT_NE(muzzle, nullptr);
  ASSERT_NE(jaw, nullptr);
  ASSERT_NE(ear_l, nullptr);
  ASSERT_NE(ear_r, nullptr);
  ASSERT_NE(tail, nullptr);
  ASSERT_NE(front_upper, nullptr);
  ASSERT_NE(front_knee, nullptr);
  ASSERT_NE(front_lower, nullptr);
  ASSERT_NE(front_hoof, nullptr);

  EXPECT_EQ(ribcage->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(ribcage->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*ribcage->custom_mesh, 2),
            mesh_axis_span(*ribcage->custom_mesh, 1) * 1.5F);
  EXPECT_EQ(chest->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(pelvis->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(belly->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(withers->shape, Render::Creature::PrimitiveShape::Cone);
  EXPECT_EQ(neck->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(mane->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(cranium->shape, Render::Creature::PrimitiveShape::Box);
  EXPECT_EQ(muzzle->shape, Render::Creature::PrimitiveShape::Cone);
  EXPECT_EQ(jaw->shape, Render::Creature::PrimitiveShape::Box);
  EXPECT_EQ(ear_l->shape, Render::Creature::PrimitiveShape::Cone);
  EXPECT_EQ(ear_r->shape, Render::Creature::PrimitiveShape::Cone);
  EXPECT_EQ(ribcage->color_role, 1U);
  EXPECT_EQ(mane->color_role, 5U);
  EXPECT_GT(chest->params.head_offset.z(), ribcage->params.head_offset.z());
  EXPECT_LT(pelvis->params.head_offset.z(), ribcage->params.head_offset.z());
  EXPECT_LT(belly->params.head_offset.y(), ribcage->params.head_offset.y());
  EXPECT_GT(withers->params.tail_offset.y(), withers->params.head_offset.y());
  EXPECT_GT(neck->params.radius, mane->params.radius);
  EXPECT_GT(mane->params.radius, 0.0F);
  EXPECT_GT(muzzle->params.radius, 0.0F);
  EXPECT_GT(jaw->params.half_extents.y(), 0.0F);
  EXPECT_GT(muzzle->params.tail_offset.z(), 0.0F);
  EXPECT_LT(jaw->params.head_offset.y(), 0.0F);
  EXPECT_GT(ear_l->params.tail_offset.y(), ear_l->params.head_offset.y());
  EXPECT_GT(ear_r->params.tail_offset.y(), ear_r->params.head_offset.y());
  EXPECT_EQ(tail->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(front_upper->shape, Render::Creature::PrimitiveShape::Cone);
  EXPECT_EQ(front_knee->shape, Render::Creature::PrimitiveShape::Sphere);
  EXPECT_EQ(front_lower->shape, Render::Creature::PrimitiveShape::Cylinder);
  EXPECT_EQ(front_hoof->shape, Render::Creature::PrimitiveShape::Mesh);
  EXPECT_GT(front_upper->params.radius, front_lower->params.radius);
  EXPECT_GT(front_knee->params.radius, front_lower->params.radius);
}

TEST(HorseSpecTest, FullSpecPreservesToplineDepthAndHeadTaper) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *ribcage =
      find_primitive(spec.lod_full.primitives, "horse.full.ribcage");
  auto const *withers =
      find_primitive(spec.lod_full.primitives, "horse.full.withers");
  auto const *belly =
      find_primitive(spec.lod_full.primitives, "horse.full.belly");
  auto const *chest =
      find_primitive(spec.lod_full.primitives, "horse.full.chest");
  auto const *sternum =
      find_primitive(spec.lod_full.primitives, "horse.full.sternum");
  auto const *croup =
      find_primitive(spec.lod_full.primitives, "horse.full.croup");
  auto const *shoulder =
      find_primitive(spec.lod_full.primitives, "horse.full.shoulder.l");
  auto const *hindquarter =
      find_primitive(spec.lod_full.primitives, "horse.full.hq.l");
  auto const *muzzle =
      find_primitive(spec.lod_full.primitives, "horse.full.head.muzzle");
  auto const *jaw =
      find_primitive(spec.lod_full.primitives, "horse.full.head.jaw");
  auto const *cheek =
      find_primitive(spec.lod_full.primitives, "horse.full.head.cheek.l");
  auto const *cranium =
      find_primitive(spec.lod_full.primitives, "horse.full.head.cranium");

  ASSERT_NE(ribcage, nullptr);
  ASSERT_NE(withers, nullptr);
  ASSERT_NE(belly, nullptr);
  ASSERT_NE(chest, nullptr);
  ASSERT_NE(sternum, nullptr);
  ASSERT_NE(croup, nullptr);
  ASSERT_NE(shoulder, nullptr);
  ASSERT_NE(hindquarter, nullptr);
  ASSERT_NE(muzzle, nullptr);
  ASSERT_NE(jaw, nullptr);
  ASSERT_NE(cheek, nullptr);
  ASSERT_NE(cranium, nullptr);

  EXPECT_EQ(ribcage->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(ribcage->custom_mesh, nullptr);
  EXPECT_EQ(chest->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(croup->shape, Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_GT(mesh_axis_span(*ribcage->custom_mesh, 2),
            mesh_axis_span(*ribcage->custom_mesh, 1) * 1.5F);
  EXPECT_GT(withers->params.head_offset.y(), croup->params.head_offset.y());
  EXPECT_GT(chest->params.head_offset.z(), ribcage->params.head_offset.z());
  EXPECT_LT(sternum->params.head_offset.y(), chest->params.head_offset.y());
  EXPECT_LT(belly->params.half_extents.z(), ribcage->params.half_extents.z());
  EXPECT_LT(shoulder->params.half_extents.x(),
            ribcage->params.half_extents.x());
  EXPECT_LT(hindquarter->params.half_extents.x(),
            ribcage->params.half_extents.x());
  EXPECT_LT(muzzle->params.radius, jaw->params.half_extents.x());
  EXPECT_LT(muzzle->params.radius, cheek->params.radius);
  EXPECT_GT(cranium->params.half_extents.z(), jaw->params.half_extents.z());
}

TEST(HorseSpecTest, FullSpecForearmStaysHeavierThanRearUpperLeg) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *forearm =
      find_primitive(spec.lod_full.primitives, "horse.full.leg.fl.upper");
  auto const *gaskin =
      find_primitive(spec.lod_full.primitives, "horse.full.leg.bl.upper");

  ASSERT_NE(forearm, nullptr);
  ASSERT_NE(gaskin, nullptr);

  EXPECT_GT(forearm->params.radius, gaskin->params.radius);
}

TEST(HorseSpecTest, ReducedWalkKeepsRearHoofLowerDuringStanceThanSwing) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  gait.cycle_time = 1.1F;
  gait.front_leg_phase = 0.25F;
  gait.rear_leg_phase = 0.0F;
  gait.stride_swing = 0.34F;
  gait.stride_lift = 0.14F;

  Render::Horse::HorseSpecPose stance_pose;
  Render::Horse::HorseSpecPose swing_pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, true},
      stance_pose);
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.75F, 0.0F, true},
      swing_pose);

  EXPECT_LT(stance_pose.foot_bl.y(), swing_pose.foot_bl.y());
  EXPECT_GT(swing_pose.foot_bl.y() - stance_pose.foot_bl.y(), 0.05F);
}
