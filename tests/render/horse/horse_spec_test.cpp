// Stage 16.5 — horse rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_horse_lod`) was removed when
// the imperative LOD path was retired; the rig now exclusively calls
// the rigged wrappers. With a non-Renderer submitter we exercise the
// software fallback inside `submit_horse_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (5/11/16 for Minimal/Reduced/Full).

#include "render/creature/spec.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/submitter.h"

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
  d.leg_length = 0.90F;
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

TEST(HorseSpecTest, ReducedRiggedFallbackEmitsNinePrimitives) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  auto variant = make_horse_variant();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{0.0F, 0.0F, false}, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Horse::submit_horse_reduced_rigged(pose, variant, identity, sub);

  EXPECT_EQ(sub.calls.size(), 9U);
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
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 9U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 43U);
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
