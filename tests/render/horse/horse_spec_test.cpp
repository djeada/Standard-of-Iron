// Stage 16.4 — horse CreatureSpec Minimal LOD parity test.
//
// Confirms submit_horse_lod(..., Minimal) emits exactly the five draw
// calls the legacy HorseRendererBase::render_minimal path used to emit
// (one body ellipsoid + four leg cylinders), with matrices that match
// the legacy formulae to within floating-point precision.

#include "render/creature/spec.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/horse/horse_spec.h"
#include "render/horse/rig.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::Creature::CreatureLOD;
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
  void part(Render::GL::Mesh *m, Render::GL::Material *, const QMatrix4x4 &model,
            const QVector3D &col, Render::GL::Texture *, float,
            int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &,
                       float) override {}
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
  d.leg_length = 0.90F;
  return d;
}

Render::GL::HorseVariant make_horse_variant() {
  Render::GL::HorseVariant v{};
  v.coat_color = QVector3D(0.55F, 0.35F, 0.20F);
  return v;
}

bool nearly_equal(const QMatrix4x4 &a, const QMatrix4x4 &b,
                  float eps = 1e-4F) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (std::abs(a(i, j) - b(i, j)) > eps) {
        return false;
      }
    }
  }
  return true;
}

} // namespace

TEST(HorseSpecTest, MinimalLodEmitsFivePrimitives) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Minimal,
                                  identity, sub);

  EXPECT_EQ(sub.calls.size(), 5U);
}

TEST(HorseSpecTest, MinimalBodyMatchesLegacyScale) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Minimal,
                                  identity, sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QMatrix4x4 expected;
  expected.translate(center);
  expected.scale(dims.body_width * 1.2F,
                 dims.body_height + dims.neck_rise * 0.5F,
                 dims.body_length + dims.head_length * 0.5F);

  Call const &body = sub.calls[0];
  EXPECT_EQ(body.mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(body.model, expected));
  EXPECT_NEAR(body.color.x(), variant.coat_color.x(), 1e-5F);
  EXPECT_NEAR(body.color.y(), variant.coat_color.y(), 1e-5F);
  EXPECT_NEAR(body.color.z(), variant.coat_color.z(), 1e-5F);
  EXPECT_EQ(body.material_id, 6);
}

TEST(HorseSpecTest, MinimalLegsMatchLegacyCylinders) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Minimal,
                                  identity, sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);

  struct Leg {
    float x_sign;
    float z_sign;
  };
  std::array<Leg, 4> legs{{
      {1.0F, 1.0F},
      {-1.0F, 1.0F},
      {1.0F, -1.0F},
      {-1.0F, -1.0F},
  }};

  float const radius = dims.body_width * 0.15F;
  QVector3D const expected_color = variant.coat_color * 0.75F;

  for (std::size_t i = 0; i < 4; ++i) {
    QVector3D const top =
        center + QVector3D(legs[i].x_sign * dims.body_width * 0.40F,
                           -dims.body_height * 0.3F,
                           legs[i].z_sign * dims.body_length * 0.25F);
    QVector3D const bottom =
        top + QVector3D(0.0F, -dims.leg_length * 0.60F, 0.0F);
    QMatrix4x4 const expected =
        Render::Geom::cylinder_between(top, bottom, radius);

    Call const &leg = sub.calls[1 + i];
    EXPECT_EQ(leg.mesh, Render::GL::get_unit_cylinder()) << "leg " << i;
    EXPECT_TRUE(nearly_equal(leg.model, expected)) << "leg " << i;
    EXPECT_NEAR(leg.color.x(), expected_color.x(), 1e-5F) << "leg " << i;
    EXPECT_NEAR(leg.color.y(), expected_color.y(), 1e-5F) << "leg " << i;
    EXPECT_NEAR(leg.color.z(), expected_color.z(), 1e-5F) << "leg " << i;
  }
}

TEST(HorseSpecTest, BobOffsetAppliedToBarrelCenter) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.05F, pose);

  EXPECT_NEAR(pose.barrel_center.y(), dims.barrel_center_y + 0.05F, 1e-6F);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Minimal,
                                  identity, sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  EXPECT_NEAR(sub.calls[0].model(1, 3),
              dims.barrel_center_y + 0.05F, 1e-4F);
}

TEST(HorseSpecTest, FullLodEmitsStaticBodyPrimitives) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Full, identity,
                                  sub);
  EXPECT_EQ(sub.calls.size(), 16U);
}

TEST(HorseSpecTest, BillboardLodEmitsNothing) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Billboard,
                                  identity, sub);
  EXPECT_TRUE(sub.calls.empty());
}

// ---- Reduced LOD parity -------------------------------------------------

namespace {

Render::GL::HorseGait make_horse_gait() {
  Render::GL::HorseGait g{};
  g.cycle_time = 1.0F;
  g.front_leg_phase = 0.0F;
  g.rear_leg_phase = 0.5F;
  g.stride_swing = 0.30F;
  g.stride_lift = 0.12F;
  return g;
}

Render::GL::HorseDimensions make_horse_dims_reduced() {
  auto d = make_horse_dims();
  d.hoof_height = 0.08F;
  d.head_width = 0.16F;
  d.head_height = 0.22F;
  return d;
}

Render::GL::HorseVariant make_horse_variant_reduced() {
  auto v = make_horse_variant();
  v.hoof_color = QVector3D(0.10F, 0.08F, 0.05F);
  return v;
}

} // namespace

TEST(HorseSpecTest, ReducedLodEmitsElevenPrimitivesIdleNoMotion) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::HorseReducedMotion motion{0.0F, 0.0F, false};
  Render::Horse::make_horse_spec_pose_reduced(dims, gait, motion, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);
  EXPECT_EQ(sub.calls.size(), 11U);
}

TEST(HorseSpecTest, ReducedBodyEllipsoidMatchesLegacyScale) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(dims, gait,
                                              {0.0F, 0.0F, false}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);
  ASSERT_EQ(sub.calls.size(), 11U);

  // Legacy body: translate(center); scale(W, H*0.85, L*0.80).
  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QMatrix4x4 expected;
  expected.translate(center);
  expected.scale(dims.body_width, dims.body_height * 0.85F,
                 dims.body_length * 0.80F);

  Call const &body = sub.calls[0];
  EXPECT_EQ(body.mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(body.model, expected));
  EXPECT_NEAR(body.color.x(), variant.coat_color.x(), 1e-5F);
  EXPECT_EQ(body.material_id, 6);
}

TEST(HorseSpecTest, ReducedNeckCylinderMatchesLegacy) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(dims, gait,
                                              {0.0F, 0.0F, false}, pose);
  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.35F,
                         dims.body_length * 0.35F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, dims.neck_rise, dims.neck_length);
  QMatrix4x4 const expected =
      Render::Geom::cylinder_between(neck_base, neck_top,
                                     dims.body_width * 0.40F);

  Call const &neck = sub.calls[1];
  EXPECT_EQ(neck.mesh, Render::GL::get_unit_cylinder());
  EXPECT_TRUE(nearly_equal(neck.model, expected));
  EXPECT_NEAR(neck.color.x(), variant.coat_color.x(), 1e-5F);
}

TEST(HorseSpecTest, ReducedHeadEllipsoidMatchesLegacyScale) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(dims, gait,
                                              {0.0F, 0.0F, false}, pose);
  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.35F,
                         dims.body_length * 0.35F);
  QVector3D const neck_top =
      neck_base + QVector3D(0.0F, dims.neck_rise, dims.neck_length);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, dims.head_height * 0.10F,
                           dims.head_length * 0.40F);

  QMatrix4x4 expected;
  expected.translate(head_center);
  expected.scale(dims.head_width * 0.90F, dims.head_height * 0.85F,
                 dims.head_length * 0.75F);

  Call const &head = sub.calls[2];
  EXPECT_EQ(head.mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(head.model, expected));
}

TEST(HorseSpecTest, ReducedLegsAndHoovesAtExpectedPositions) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(dims, gait,
                                              {0.0F, 0.0F, false}, pose);
  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);
  ASSERT_EQ(sub.calls.size(), 11U);

  // At idle (is_moving=false), stride=lift=0 so shoulder = anchor +
  // (lateral_sign * body_width*0.45, 0, 0); foot = shoulder + (0, -leg*0.85, 0).
  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const front_anchor =
      center + QVector3D(0.0F, dims.body_height * 0.05F,
                         dims.body_length * 0.30F);
  QVector3D const front_l_shoulder =
      front_anchor + QVector3D(dims.body_width * 0.45F, 0.0F, 0.0F);
  QVector3D const front_l_foot =
      front_l_shoulder + QVector3D(0.0F, -dims.leg_length * 0.85F, 0.0F);

  // FL leg is call[3], hoof is call[4].
  QMatrix4x4 const expected_leg_fl = Render::Geom::cylinder_between(
      front_l_shoulder, front_l_foot, dims.body_width * 0.22F);
  EXPECT_TRUE(nearly_equal(sub.calls[3].model, expected_leg_fl));
  EXPECT_EQ(sub.calls[3].mesh, Render::GL::get_unit_cylinder());

  QVector3D const expected_leg_color = variant.coat_color * 0.85F;
  EXPECT_NEAR(sub.calls[3].color.x(), expected_leg_color.x(), 1e-5F);

  // Hoof FL matrix: translate(foot); scale(bw*0.28, hoof_h, bw*0.30).
  QMatrix4x4 expected_hoof_fl;
  expected_hoof_fl.translate(front_l_foot);
  expected_hoof_fl.scale(dims.body_width * 0.28F, dims.hoof_height,
                         dims.body_width * 0.30F);
  EXPECT_TRUE(nearly_equal(sub.calls[4].model, expected_hoof_fl));
  EXPECT_EQ(sub.calls[4].mesh, Render::GL::get_unit_cylinder());
  EXPECT_NEAR(sub.calls[4].color.x(), variant.hoof_color.x(), 1e-5F);
  EXPECT_EQ(sub.calls[4].material_id, 8);
}

TEST(HorseSpecTest, ReducedBobLiftsEverything) {
  auto dims = make_horse_dims_reduced();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant_reduced();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(dims, gait,
                                              {0.0F, 0.04F, false}, pose);
  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Reduced, identity,
                                  sub);
  ASSERT_GE(sub.calls.size(), 1U);

  // Body translation Y ≈ barrel_center_y + bob.
  EXPECT_NEAR(sub.calls[0].model(1, 3), dims.barrel_center_y + 0.04F, 1e-4F);
}

TEST(HorseSpecTest, WorldFromUnitIsAppliedOnSubmit) {
  auto dims = make_horse_dims();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 world;
  world.translate(10.0F, 0.0F, -5.0F);

  CapturingSubmitter sub;
  Render::Horse::submit_horse_lod(pose, variant, CreatureLOD::Minimal, world,
                                  sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  EXPECT_NEAR(sub.calls[0].model(0, 3), 10.0F, 1e-4F);
  EXPECT_NEAR(sub.calls[0].model(2, 3), -5.0F, 1e-4F);
}
