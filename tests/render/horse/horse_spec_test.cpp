// Stage 16.5 — horse rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_horse_lod`) was removed when
// the imperative LOD path was retired; the rig now exclusively calls
// the rigged wrappers. With a non-Renderer submitter we exercise the
// software fallback inside `submit_horse_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (5/17/43 for Minimal/Reduced/Full).

#include "render/creature/spec.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/submitter.h"

#include <algorithm>
#include <QMatrix4x4>
#include <QVector3D>
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

} // namespace

TEST(HorseSpecTest, MinimalRiggedFallbackEmitsFivePrimitives) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_minimal_rigged(pose, variant, identity, sub);

  EXPECT_EQ(sub.calls.size(), 5U);
}

TEST(HorseSpecTest, ReducedRiggedFallbackEmitsSeventeenPrimitives) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, false}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_reduced_rigged(pose, variant, identity, sub);

  EXPECT_EQ(sub.calls.size(), 17U);
}

TEST(HorseSpecTest, FullRiggedFallbackEmitsAnatomicalPrimitives) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, false}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_full_rigged(pose, variant, identity, sub);

  EXPECT_EQ(sub.calls.size(), 43U);
}

TEST(HorseSpecTest, CreatureSpecHasAllThreeLods) {
  auto const &spec = Render::Horse::horse_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 5U);
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 17U);
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
  EXPECT_LT(pose.leg_radius, dims.body_width * 0.10F);
}

TEST(HorseSpecTest, ReducedPoseKeepsLegsOutsideNarrowerTorsoRead) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, false}, pose);

  // body_half.x is now bw*0.88 so the barrel spans the full horse width.
  EXPECT_LT(pose.reduced_body_half.x(), dims.body_width * 1.05F);
  // Shoulders must still be outside the body half-width (anatomically correct).
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_fl.x()), dims.body_width * 0.60F);
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_fl.x()),
             std::abs(pose.shoulder_offset_reduced_bl.x()));
  EXPECT_GT(std::abs(pose.shoulder_offset_reduced_bl.x()),
            dims.body_width * 0.40F);
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

  auto const *body =
      find_primitive(spec.lod_reduced.primitives, "horse.body.reduced");
  auto const *chest =
      find_primitive(spec.lod_reduced.primitives, "horse.chest.reduced");
  auto const *rump =
      find_primitive(spec.lod_reduced.primitives, "horse.rump.reduced");
  auto const *neck_head =
      find_primitive(spec.lod_reduced.primitives, "horse.neck_head.reduced");
  auto const *tail =
      find_primitive(spec.lod_reduced.primitives, "horse.tail.reduced");
  auto const *front_upper =
      find_primitive(spec.lod_reduced.primitives, "horse.leg.fl.upper.r");
  auto const *front_lower =
      find_primitive(spec.lod_reduced.primitives, "horse.leg.fl.lower.r");
  auto const *front_hoof =
      find_primitive(spec.lod_reduced.primitives, "horse.hoof.fl.r");

  ASSERT_NE(body, nullptr);
  ASSERT_NE(chest, nullptr);
  ASSERT_NE(rump, nullptr);
  ASSERT_NE(neck_head, nullptr);
  ASSERT_NE(tail, nullptr);
  ASSERT_NE(front_upper, nullptr);
  ASSERT_NE(front_lower, nullptr);
  ASSERT_NE(front_hoof, nullptr);

  // Body/chest/rump are now Capsules for organic rounded silhouette.
  EXPECT_EQ(body->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(chest->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(rump->shape, Render::Creature::PrimitiveShape::Capsule);
  // Chest head is forward of the barrel center; rump head is behind it.
  EXPECT_GT(chest->params.head_offset.z(), body->params.head_offset.z());
  EXPECT_LE(rump->params.head_offset.z(), body->params.tail_offset.z());
  // Chest sits above ground level, rump behind zero.
  EXPECT_GT(chest->params.head_offset.z(), 0.0F);
  EXPECT_LT(rump->params.head_offset.z(), 0.0F);
  // Neck cylinder must be narrower than the body capsule radius.
  EXPECT_LT(neck_head->params.radius, body->params.radius);
  EXPECT_EQ(tail->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(front_upper->shape, Render::Creature::PrimitiveShape::Capsule);
  EXPECT_EQ(front_lower->shape, Render::Creature::PrimitiveShape::Cylinder);
  EXPECT_EQ(front_hoof->shape, Render::Creature::PrimitiveShape::Mesh);
  EXPECT_GT(front_upper->params.radius, front_lower->params.radius);
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

  EXPECT_GT(withers->params.head_offset.y(), croup->params.head_offset.y());
  EXPECT_GT(chest->params.head_offset.z(), ribcage->params.head_offset.z());
  EXPECT_LT(sternum->params.head_offset.y(), chest->params.head_offset.y());
  EXPECT_LT(belly->params.half_extents.z(), ribcage->params.half_extents.z());
  EXPECT_LT(shoulder->params.half_extents.x(), ribcage->params.half_extents.x());
  EXPECT_LT(hindquarter->params.half_extents.x(), ribcage->params.half_extents.x());
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
