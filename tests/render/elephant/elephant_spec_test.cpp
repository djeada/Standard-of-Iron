// Stage 16.5 — elephant CreatureSpec Minimal LOD parity test.

#include "render/creature/spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/elephant/rig.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
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

Render::GL::ElephantDimensions make_dims() {
  Render::GL::ElephantDimensions d{};
  d.body_length = 3.2F;
  d.body_width = 1.1F;
  d.body_height = 1.6F;
  d.barrel_center_y = 2.0F;
  d.neck_length = 0.9F;
  d.head_length = 1.1F;
  d.leg_length = 1.6F;
  d.leg_radius = 0.22F;
  return d;
}

Render::GL::ElephantVariant make_variant() {
  Render::GL::ElephantVariant v{};
  v.skin_color = QVector3D(0.45F, 0.40F, 0.38F);
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

TEST(ElephantSpecTest, MinimalLodEmitsFivePrimitives) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Minimal,
                                        identity, sub);
  EXPECT_EQ(sub.calls.size(), 5U);
}

TEST(ElephantSpecTest, MinimalBodyMatchesLegacyScale) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Minimal,
                                        identity, sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QMatrix4x4 expected;
  expected.translate(center);
  expected.scale(dims.body_width * 1.2F,
                 dims.body_height + dims.neck_length * 0.3F,
                 dims.body_length + dims.head_length * 0.3F);

  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(sub.calls[0].model, expected));
  EXPECT_NEAR(sub.calls[0].color.x(), variant.skin_color.x(), 1e-5F);
}

TEST(ElephantSpecTest, MinimalLegsMatchLegacyCylinders) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Minimal,
                                        identity, sub);
  ASSERT_EQ(sub.calls.size(), 5U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  std::array<std::pair<float, float>, 4> legs{{
      {1.0F, 1.0F}, {-1.0F, 1.0F}, {1.0F, -1.0F}, {-1.0F, -1.0F},
  }};
  float const radius = dims.leg_radius * 0.70F;
  QVector3D const expected_color = variant.skin_color * 0.80F;

  for (std::size_t i = 0; i < 4; ++i) {
    auto [xs, zs] = legs[i];
    QVector3D top = center + QVector3D(xs * dims.body_width * 0.38F,
                                       -dims.body_height * 0.25F,
                                       zs * dims.body_length * 0.30F);
    QVector3D bottom = top + QVector3D(0, -dims.leg_length * 0.70F, 0);
    QMatrix4x4 expected = Render::Geom::cylinder_between(top, bottom, radius);

    EXPECT_EQ(sub.calls[1 + i].mesh, Render::GL::get_unit_cylinder());
    EXPECT_TRUE(nearly_equal(sub.calls[1 + i].model, expected))
        << "leg " << i;
    EXPECT_NEAR(sub.calls[1 + i].color.x(), expected_color.x(), 1e-5F);
    EXPECT_NEAR(sub.calls[1 + i].color.y(), expected_color.y(), 1e-5F);
    EXPECT_NEAR(sub.calls[1 + i].color.z(), expected_color.z(), 1e-5F);
  }
}

TEST(ElephantSpecTest, FullLodEmitsStaticBodyPrimitives) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Full,
                                        identity, sub);
  EXPECT_EQ(sub.calls.size(), 15U);
}

TEST(ElephantSpecTest, BillboardLodEmitsNothing) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Billboard,
                                        identity, sub);
  EXPECT_TRUE(sub.calls.empty());
}

// ---- Reduced LOD parity -----------------------------------------

namespace {

Render::GL::ElephantDimensions make_dims_reduced() {
  auto d = make_dims();
  d.neck_width = 0.35F;
  d.head_width = 0.70F;
  d.head_height = 0.80F;
  d.trunk_length = 1.10F;
  d.trunk_base_radius = 0.18F;
  d.foot_radius = 0.32F;
  return d;
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

} // namespace

TEST(ElephantSpecTest, ReducedLodEmitsTwelvePrimitives) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion motion{};
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, motion, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);
  EXPECT_EQ(sub.calls.size(), 12U);
}

TEST(ElephantSpecTest, ReducedBodyEllipsoidMatchesLegacy) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, {}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);
  ASSERT_EQ(sub.calls.size(), 12U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QMatrix4x4 expected;
  expected.translate(center);
  expected.scale(dims.body_width, dims.body_height * 0.90F,
                 dims.body_length * 0.75F);
  EXPECT_EQ(sub.calls[0].mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(sub.calls[0].model, expected));
  EXPECT_NEAR(sub.calls[0].color.x(), variant.skin_color.x(), 1e-5F);
  EXPECT_EQ(sub.calls[0].material_id, 6);
}

TEST(ElephantSpecTest, ReducedNeckCylinderMatchesLegacy) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, {}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.20F,
                         dims.body_length * 0.45F);
  QVector3D const head_center =
      neck_base +
      QVector3D(0.0F, dims.neck_length * 0.50F, dims.head_length * 0.60F);
  QMatrix4x4 const expected = Render::Geom::cylinder_between(
      neck_base, head_center, dims.neck_width * 0.85F);

  EXPECT_EQ(sub.calls[1].mesh, Render::GL::get_unit_cylinder());
  EXPECT_TRUE(nearly_equal(sub.calls[1].model, expected));
  EXPECT_NEAR(sub.calls[1].color.x(), variant.skin_color.x(), 1e-5F);
}

TEST(ElephantSpecTest, ReducedHeadEllipsoidMatchesLegacy) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, {}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.20F,
                         dims.body_length * 0.45F);
  QVector3D const head_center =
      neck_base +
      QVector3D(0.0F, dims.neck_length * 0.50F, dims.head_length * 0.60F);
  QMatrix4x4 expected;
  expected.translate(head_center);
  expected.scale(dims.head_width * 0.85F, dims.head_height * 0.80F,
                 dims.head_length * 0.70F);

  EXPECT_EQ(sub.calls[2].mesh, Render::GL::get_unit_sphere());
  EXPECT_TRUE(nearly_equal(sub.calls[2].model, expected));
}

TEST(ElephantSpecTest, ReducedTrunkConeMatchesLegacy) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, {}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  QVector3D const neck_base =
      center + QVector3D(0.0F, dims.body_height * 0.20F,
                         dims.body_length * 0.45F);
  QVector3D const head_center =
      neck_base +
      QVector3D(0.0F, dims.neck_length * 0.50F, dims.head_length * 0.60F);
  QVector3D const trunk_end =
      head_center + QVector3D(0.0F, -dims.trunk_length * 0.50F,
                              dims.trunk_length * 0.40F);
  QMatrix4x4 const expected = Render::Geom::cone_from_to(
      trunk_end, head_center, dims.trunk_base_radius * 0.8F);

  EXPECT_EQ(sub.calls[3].mesh, Render::GL::get_unit_cone());
  EXPECT_TRUE(nearly_equal(sub.calls[3].model, expected));
  QVector3D const expected_color = variant.skin_color * 0.90F;
  EXPECT_NEAR(sub.calls[3].color.x(), expected_color.x(), 1e-5F);
}

TEST(ElephantSpecTest, ReducedFrontLeftLegAndPadMatchLegacyIdle) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  // Idle: phase=0, bob=0, not moving, not fighting → stride=lift=0.
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, {}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);
  ASSERT_EQ(sub.calls.size(), 12U);

  QVector3D const center(0.0F, dims.barrel_center_y, 0.0F);
  float const front_forward = dims.body_length * 0.35F;
  QVector3D const hip = center + QVector3D(dims.body_width * 0.40F,
                                           -dims.body_height * 0.30F,
                                           front_forward);
  QVector3D const foot = hip + QVector3D(0.0F, -dims.leg_length * 0.85F, 0.0F);

  QMatrix4x4 const expected_leg =
      Render::Geom::cylinder_between(hip, foot, dims.leg_radius * 0.85F);
  EXPECT_TRUE(nearly_equal(sub.calls[4].model, expected_leg));
  EXPECT_EQ(sub.calls[4].mesh, Render::GL::get_unit_cylinder());
  QVector3D const expected_leg_color = variant.skin_color * 0.88F;
  EXPECT_NEAR(sub.calls[4].color.x(), expected_leg_color.x(), 1e-5F);

  QVector3D const pad_center =
      foot + QVector3D(0.0F, -dims.foot_radius * 0.18F, 0.0F);
  QMatrix4x4 expected_pad;
  expected_pad.translate(pad_center);
  expected_pad.scale(dims.foot_radius, dims.foot_radius * 0.65F,
                     dims.foot_radius * 1.10F);
  EXPECT_TRUE(nearly_equal(sub.calls[5].model, expected_pad));
  EXPECT_EQ(sub.calls[5].mesh, Render::GL::get_unit_sphere());
  QVector3D const expected_pad_color = variant.skin_color * 0.75F;
  EXPECT_NEAR(sub.calls[5].color.x(), expected_pad_color.x(), 1e-5F);
  EXPECT_EQ(sub.calls[5].material_id, 8);
}

TEST(ElephantSpecTest, ReducedBobLiftsBody) {
  auto dims = make_dims_reduced();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion motion{};
  motion.bob = 0.05F;
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, motion, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_lod(pose, variant, CreatureLOD::Reduced,
                                        identity, sub);
  ASSERT_GE(sub.calls.size(), 1U);

  EXPECT_NEAR(sub.calls[0].model(1, 3), dims.barrel_center_y + 0.05F, 1e-4F);
}
